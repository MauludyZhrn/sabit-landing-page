#include "src/detect/onnx_detector.h"
#include "src/detect/sha256.h"
#include "src/core/logger.h"

#include <windows.h>
#define _stdcall __stdcall
#include "vendor/onnxruntime/onnxruntime_c_api.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <filesystem>

namespace sabit::detect {

namespace {

float compute_iou(float x0_a, float y0_a, float x1_a, float y1_a,
                  float x0_b, float y0_b, float x1_b, float y1_b) {
    float inter_x0 = std::max(x0_a, x0_b);
    float inter_y0 = std::max(y0_a, y0_b);
    float inter_x1 = std::min(x1_a, x1_b);
    float inter_y1 = std::min(y1_a, y1_b);

    float inter_w = std::max(0.0f, inter_x1 - inter_x0);
    float inter_h = std::max(0.0f, inter_y1 - inter_y0);
    float inter_area = inter_w * inter_h;

    float area_a = std::max(0.0f, x1_a - x0_a) * std::max(0.0f, y1_a - y0_a);
    float area_b = std::max(0.0f, x1_b - x0_b) * std::max(0.0f, y1_b - y0_b);

    float union_area = area_a + area_b - inter_area;
    if (union_area <= 1e-6f) return 0.0f;
    return inter_area / union_area;
}

std::wstring to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], size_needed);
    return wstr;
}

} // namespace

OnnxDetector::OnnxDetector() = default;

OnnxDetector::~OnnxDetector() {
    close();
}

bool OnnxDetector::verify_manifest_hash() {
    if (config_.manifest_path.empty()) return true;

    std::ifstream f(config_.manifest_path);
    if (!f.is_open()) {
        SABIT_LOG_WARN("Manifest file not found: ", config_.manifest_path);
        return false;
    }

    std::stringstream buffer;
    buffer << f.rdbuf();
    std::string content = buffer.str();

    // Simple search for "sha256": "..."
    std::string search_key = "\"sha256\": \"";
    size_t pos = content.find(search_key);
    if (pos == std::string::npos) {
        search_key = "\"sha256\":\"";
        pos = content.find(search_key);
    }
    if (pos == std::string::npos) {
        SABIT_LOG_WARN("Could not find sha256 key in manifest");
        return false;
    }

    size_t start = pos + search_key.length();
    size_t end = content.find("\"", start);
    if (end == std::string::npos) return false;

    std::string expected_hash = content.substr(start, end - start);
    std::string actual_hash = Sha256::file_sha256(config_.model_path);

    if (actual_hash != expected_hash) {
        SABIT_LOG_ERROR("ONNX Model hash mismatch! Expected: ", expected_hash, ", got: ", actual_hash);
        return false;
    }

    SABIT_LOG_INFO("ONNX Model SHA-256 verified successfully: ", actual_hash);
    return true;
}

core::Result<void> OnnxDetector::init(const OnnxDetectorConfig& config) {
    close();
    config_ = config;
    conf_threshold_.store(config.conf_threshold);
    nms_threshold_.store(config.nms_threshold);
    max_fps_.store(config.max_fps);

    // Resolve relative paths to application directory if not found in current directory
    char exe_buf[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_buf, MAX_PATH) > 0) {
        std::filesystem::path app_dir = std::filesystem::path(exe_buf).parent_path();
        if (!std::filesystem::exists(config_.model_path) && std::filesystem::exists(app_dir / config_.model_path)) {
            config_.model_path = (app_dir / config_.model_path).string();
        }
        if (!std::filesystem::exists(config_.manifest_path) && std::filesystem::exists(app_dir / config_.manifest_path)) {
            config_.manifest_path = (app_dir / config_.manifest_path).string();
        }
        if (!std::filesystem::exists(config_.dll_path) && std::filesystem::exists(app_dir / config_.dll_path)) {
            config_.dll_path = (app_dir / config_.dll_path).string();
        }
        if (!std::filesystem::exists(config_.dll_path) && std::filesystem::exists(app_dir / "onnxruntime.dll")) {
            config_.dll_path = (app_dir / "onnxruntime.dll").string();
        }
    }

    if (config_.verify_hash && !verify_manifest_hash()) {
        return core::Result<void>::err(core::Error(core::ErrorCode::InvalidState, "ONNX model SHA-256 hash mismatch with manifest"));
    }

    HMODULE hMod = LoadLibraryA(config_.dll_path.c_str());
    if (!hMod) {
        hMod = LoadLibraryA("onnxruntime.dll");
    }
    if (!hMod) {
        return core::Result<void>::err(core::Error(core::ErrorCode::DriverMissing, "Failed to load onnxruntime.dll"));
    }
    dll_handle_ = reinterpret_cast<void*>(hMod);

    typedef const OrtApiBase* (*FnOrtGetApiBase)(void);
    auto pGetApiBase = reinterpret_cast<FnOrtGetApiBase>(reinterpret_cast<void*>(GetProcAddress(hMod, "OrtGetApiBase")));
    if (!pGetApiBase) {
        close();
        return core::Result<void>::err(core::Error(core::ErrorCode::HardwareError, "OrtGetApiBase not found in onnxruntime.dll"));
    }

    const OrtApiBase* api_base = pGetApiBase();
    ort_ = api_base->GetApi(ORT_API_VERSION);
    if (!ort_) {
        close();
        return core::Result<void>::err(core::Error(core::ErrorCode::Unsupported, "Failed to obtain OrtApi v17"));
    }

    OrtStatus* st = ort_->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "SabitDetector", &env_);
    if (st) {
        std::string err_msg = ort_->GetErrorMessage(st);
        ort_->ReleaseStatus(st);
        close();
        return core::Result<void>::err(core::Error(core::ErrorCode::HardwareError, "CreateEnv failed: " + err_msg));
    }

    st = ort_->CreateSessionOptions(&session_options_);
    if (st) {
        std::string err_msg = ort_->GetErrorMessage(st);
        ort_->ReleaseStatus(st);
        close();
        return core::Result<void>::err(core::Error(core::ErrorCode::HardwareError, "CreateSessionOptions failed: " + err_msg));
    }

    if (OrtStatus* st_t = ort_->SetIntraOpNumThreads(session_options_, config_.num_threads)) {
        ort_->ReleaseStatus(st_t);
    }
    if (OrtStatus* st_o = ort_->SetSessionGraphOptimizationLevel(session_options_, ORT_ENABLE_ALL)) {
        ort_->ReleaseStatus(st_o);
    }

    std::wstring w_model_path = to_wstring(config_.model_path);
    st = ort_->CreateSession(env_, w_model_path.c_str(), session_options_, &session_);
    if (st) {
        std::string err_msg = ort_->GetErrorMessage(st);
        ort_->ReleaseStatus(st);
        close();
        return core::Result<void>::err(core::Error(core::ErrorCode::NotFound, "CreateSession failed on " + config_.model_path + ": " + err_msg));
    }

    st = ort_->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &mem_info_);
    if (st) {
        std::string err_msg = ort_->GetErrorMessage(st);
        ort_->ReleaseStatus(st);
        close();
        return core::Result<void>::err(core::Error(core::ErrorCode::HardwareError, "CreateCpuMemoryInfo failed: " + err_msg));
    }

    is_initialized_.store(true);
    SABIT_LOG_INFO("ONNX Detector initialized successfully with model: ", config_.model_path);
    return core::Result<void>::ok();
}

void OnnxDetector::close() {
    stop_worker();

    is_initialized_.store(false);

    if (ort_) {
        if (mem_info_) { ort_->ReleaseMemoryInfo(mem_info_); mem_info_ = nullptr; }
        if (session_) { ort_->ReleaseSession(session_); session_ = nullptr; }
        if (session_options_) { ort_->ReleaseSessionOptions(session_options_); session_options_ = nullptr; }
        if (env_) { ort_->ReleaseEnv(env_); env_ = nullptr; }
    }

    if (dll_handle_) {
        FreeLibrary(reinterpret_cast<HMODULE>(dll_handle_));
        dll_handle_ = nullptr;
    }
    ort_ = nullptr;
}

bool OnnxDetector::is_initialized() const {
    return is_initialized_.load();
}

core::Result<DetectionResult> OnnxDetector::detect(const core::Frame& frame) {
    if (!is_initialized_.load() || !session_ || !ort_) {
        return core::Result<DetectionResult>::err(core::Error(core::ErrorCode::InvalidState, "Detector is not initialized"));
    }

    if (!frame.is_valid()) {
        return core::Result<DetectionResult>::err(core::Error(core::ErrorCode::Unsupported, "Invalid input frame"));
    }

    const int orig_w = frame.width;
    const int orig_h = frame.height;
    const int channels = frame.channels;

    // 1. Letterbox preprocessing (640x640, preserve aspect ratio)
    constexpr int net_w = 640;
    constexpr int net_h = 640;
    const float scale = std::min(static_cast<float>(net_w) / orig_w, static_cast<float>(net_h) / orig_h);
    const int new_w = std::min(net_w, static_cast<int>(orig_w * scale));
    const int new_h = std::min(net_h, static_cast<int>(orig_h * scale));
    const int pad_x = (net_w - new_w) / 2;
    const int pad_y = (net_h - new_h) / 2;

    // Allocate 1x3x640x640 tensor initialized with 114/255 padding
    std::vector<float> input_tensor(1 * 3 * net_w * net_h, 114.0f / 255.0f);

    constexpr float inv_65535 = 1.0f / 65535.0f;

    for (int y = 0; y < new_h; ++y) {
        int src_y = std::clamp(static_cast<int>(y / scale), 0, orig_h - 1);
        int dst_y = pad_y + y;
        size_t row_dst_offset = dst_y * net_w;

        for (int x = 0; x < new_w; ++x) {
            int src_x = std::clamp(static_cast<int>(x / scale), 0, orig_w - 1);
            int dst_x = pad_x + x;
            size_t dst_idx = row_dst_offset + dst_x;

            if (channels == 1) {
                float val = frame.pixels[src_y * orig_w + src_x] * inv_65535;
                input_tensor[0 * net_w * net_h + dst_idx] = val;
                input_tensor[1 * net_w * net_h + dst_idx] = val;
                input_tensor[2 * net_w * net_h + dst_idx] = val;
            } else {
                size_t src_idx = (static_cast<size_t>(src_y) * orig_w + src_x) * channels;
                float r = frame.pixels[src_idx + 0] * inv_65535;
                float g = frame.pixels[src_idx + 1] * inv_65535;
                float b = frame.pixels[src_idx + 2] * inv_65535;
                input_tensor[0 * net_w * net_h + dst_idx] = r;
                input_tensor[1 * net_w * net_h + dst_idx] = g;
                input_tensor[2 * net_w * net_h + dst_idx] = b;
            }
        }
    }

    // 2. Create input OrtValue
    int64_t input_shape[4] = {1, 3, net_w, net_h};
    OrtValue* input_val = nullptr;
    OrtStatus* st = ort_->CreateTensorWithDataAsOrtValue(
        mem_info_,
        input_tensor.data(),
        input_tensor.size() * sizeof(float),
        input_shape,
        4,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
        &input_val
    );
    if (st) {
        std::string err_msg = ort_->GetErrorMessage(st);
        ort_->ReleaseStatus(st);
        return core::Result<DetectionResult>::err(core::Error(core::ErrorCode::HardwareError, "CreateTensor failed: " + err_msg));
    }

    // 3. Run inference
    const char* input_names[] = {"images"};
    const char* output_names[] = {"output0"};
    OrtValue* output_val = nullptr;

    auto t_start = std::chrono::steady_clock::now();
    st = ort_->Run(
        session_,
        nullptr,
        input_names,
        const_cast<const OrtValue**>(&input_val),
        1,
        output_names,
        1,
        &output_val
    );
    auto t_end = std::chrono::steady_clock::now();
    double inference_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    ort_->ReleaseValue(input_val);

    if (st) {
        std::string err_msg = ort_->GetErrorMessage(st);
        ort_->ReleaseStatus(st);
        return core::Result<DetectionResult>::err(core::Error(core::ErrorCode::HardwareError, "Inference Run failed: " + err_msg));
    }

    // 4. Extract output tensor: shape [1, 5, 8400]
    float* out_data = nullptr;
    st = ort_->GetTensorMutableData(output_val, reinterpret_cast<void**>(&out_data));
    if (st || !out_data) {
        if (st) ort_->ReleaseStatus(st);
        ort_->ReleaseValue(output_val);
        return core::Result<DetectionResult>::err(core::Error(core::ErrorCode::HardwareError, "GetTensorMutableData failed"));
    }

    struct Candidate {
        float x0;
        float y0;
        float x1;
        float y1;
        float conf;
    };
    std::vector<Candidate> candidates;

    const float cur_conf_thresh = conf_threshold_.load();
    const float cur_nms_thresh = nms_threshold_.load();

    constexpr int num_anchors = 8400;
    for (int i = 0; i < num_anchors; ++i) {
        float conf = out_data[4 * num_anchors + i];
        if (conf >= cur_conf_thresh) {
            float cx = out_data[0 * num_anchors + i];
            float cy = out_data[1 * num_anchors + i];
            float w  = out_data[2 * num_anchors + i];
            float h  = out_data[3 * num_anchors + i];

            float x0 = cx - w * 0.5f;
            float y0 = cy - h * 0.5f;
            float x1 = cx + w * 0.5f;
            float y1 = cy + h * 0.5f;

            candidates.push_back(Candidate{x0, y0, x1, y1, conf});
        }
    }

    ort_->ReleaseValue(output_val);

    // 5. Non-Maximum Suppression (NMS)
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.conf > b.conf;
    });

    std::vector<bool> suppressed(candidates.size(), false);
    std::vector<Candidate> kept;

    for (size_t i = 0; i < candidates.size(); ++i) {
        if (suppressed[i]) continue;
        kept.push_back(candidates[i]);

        for (size_t j = i + 1; j < candidates.size(); ++j) {
            if (suppressed[j]) continue;
            float iou = compute_iou(
                candidates[i].x0, candidates[i].y0, candidates[i].x1, candidates[i].y1,
                candidates[j].x0, candidates[j].y0, candidates[j].x1, candidates[j].y1
            );
            if (iou > cur_nms_thresh) {
                suppressed[j] = true;
            }
        }
    }

    // 6. Map coordinates back to original image dimensions
    DetectionResult res{};
    res.is_valid = true;
    res.frame_index = frame.seq;
    res.timestamp_utc_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        frame.timestamp_utc.time_since_epoch()).count();
    res.inference_time_ms = inference_ms;

    const float inv_scale = 1.0f / scale;

    for (const auto& c : kept) {
        float unpad_x0 = (c.x0 - pad_x) * inv_scale;
        float unpad_y0 = (c.y0 - pad_y) * inv_scale;
        float unpad_x1 = (c.x1 - pad_x) * inv_scale;
        float unpad_y1 = (c.y1 - pad_y) * inv_scale;

        float clamped_x0 = std::clamp(unpad_x0, 0.0f, static_cast<float>(orig_w));
        float clamped_y0 = std::clamp(unpad_y0, 0.0f, static_cast<float>(orig_h));
        float clamped_x1 = std::clamp(unpad_x1, 0.0f, static_cast<float>(orig_w));
        float clamped_y1 = std::clamp(unpad_y1, 0.0f, static_cast<float>(orig_h));

        Detection det{};
        det.label = "Hilal";
        det.confidence = c.conf;
        det.x = clamped_x0;
        det.y = clamped_y0;
        det.width = std::max(0.0f, clamped_x1 - clamped_x0);
        det.height = std::max(0.0f, clamped_y1 - clamped_y0);
        det.timestamp_utc_ms = res.timestamp_utc_ms;

        res.detections.push_back(det);
    }

    return core::Result<DetectionResult>::ok(res);
}

void OnnxDetector::start_worker() {
    if (is_running_.load()) return;
    is_running_.store(true);
    worker_thread_ = std::thread(&OnnxDetector::worker_loop, this);
}

void OnnxDetector::stop_worker() {
    if (is_running_.load()) {
        is_running_.store(false);
        frame_cv_.notify_all();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
}

void OnnxDetector::submit_frame(const core::Frame& frame) {
    if (!is_running_.load()) return;
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        pending_frame_ = frame;
    }
    frame_cv_.notify_one();
}

std::vector<Detection> OnnxDetector::get_latest_overlay(int64_t max_age_ms) const {
    std::lock_guard<std::mutex> lock(result_mutex_);
    if (!latest_result_.is_valid) return {};

    auto now = std::chrono::steady_clock::now();
    auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - latest_result_time_).count();
    if (age_ms > max_age_ms) {
        // Discard overlay if older than max_age_ms (FR-21, TC-054)
        return {};
    }
    return latest_result_.detections;
}

void OnnxDetector::worker_loop() {
    auto last_inference_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);

    while (is_running_.load()) {
        core::Frame frame;
        {
            std::unique_lock<std::mutex> lock(frame_mutex_);
            frame_cv_.wait(lock, [this]() {
                return !is_running_.load() || pending_frame_.has_value();
            });

            if (!is_running_.load()) break;

            if (pending_frame_.has_value()) {
                frame = std::move(*pending_frame_);
                pending_frame_.reset();
            }
        }

        // Rate limiting based on max_fps_ (TC-052)
        double fps = std::max(0.1, max_fps_.load());
        int min_interval_ms = static_cast<int>(1000.0 / fps);

        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_inference_time).count();
        if (elapsed_ms < min_interval_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(min_interval_ms - elapsed_ms));
        }
        last_inference_time = std::chrono::steady_clock::now();

        auto res = detect(frame);
        if (res.is_ok()) {
            {
                std::lock_guard<std::mutex> lock(result_mutex_);
                latest_result_ = res.value();
                latest_result_time_ = std::chrono::steady_clock::now();
            }

            if (on_detection_) {
                on_detection_(frame, res.value());
            }
        }
    }
}

} // namespace sabit::detect
