#include "src/camera/zwo/zwo_camera.h"
#include "src/core/clock.h"
#include "src/core/logger.h"
#include <windows.h>
#include <iostream>
#include <chrono>
#include <cstring>
#include <filesystem>

namespace sabit::camera {

ZwoCamera::ZwoCamera(const std::string& dll_path) : dll_path_(dll_path) {}

ZwoCamera::~ZwoCamera() {
    close();
    unload_sdk();
}

bool ZwoCamera::load_sdk() {
    if (dll_handle_) return true;

    char exe_buf[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, exe_buf, MAX_PATH);
    std::filesystem::path exe_dir = std::filesystem::path(exe_buf).parent_path();

    const std::vector<std::filesystem::path> candidates = {
        dll_path_,
        "ASICamera2.dll",
        "vendor/zwo/ASICamera2.dll",
        ".onyx_ref/Onyx_Test/ASICamera2.dll",
        exe_dir / "ASICamera2.dll",
        exe_dir / "vendor" / "zwo" / "ASICamera2.dll",
        exe_dir.parent_path() / "vendor" / "zwo" / "ASICamera2.dll",
        exe_dir.parent_path() / ".onyx_ref" / "Onyx_Test" / "ASICamera2.dll"
    };

    HMODULE mod = nullptr;
    for (const auto& cand : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(cand, ec)) {
            mod = LoadLibraryA(cand.string().c_str());
            if (mod) break;
        }
    }
    if (!mod) {
        mod = LoadLibraryA("ASICamera2.dll");
    }
    if (!mod) {
        SABIT_LOG_WARN("ZWO SDK DLL not found at: ", dll_path_);
        return false;
    }

    dll_handle_ = reinterpret_cast<void*>(mod);

    auto load_fn = [mod](const char* name) {
        return reinterpret_cast<void*>(GetProcAddress(mod, name));
    };

    p_ASIGetNumOfConnectedCameras = reinterpret_cast<Fn_ASIGetNumOfConnectedCameras>(load_fn("ASIGetNumOfConnectedCameras"));
    p_ASIGetCameraProperty = reinterpret_cast<Fn_ASIGetCameraProperty>(load_fn("ASIGetCameraProperty"));
    p_ASIOpenCamera = reinterpret_cast<Fn_ASIOpenCamera>(load_fn("ASIOpenCamera"));
    p_ASIInitCamera = reinterpret_cast<Fn_ASIInitCamera>(load_fn("ASIInitCamera"));
    p_ASICloseCamera = reinterpret_cast<Fn_ASICloseCamera>(load_fn("ASICloseCamera"));
    p_ASIGetControlValue = reinterpret_cast<Fn_ASIGetControlValue>(load_fn("ASIGetControlValue"));
    p_ASISetControlValue = reinterpret_cast<Fn_ASISetControlValue>(load_fn("ASISetControlValue"));
    p_ASIGetROIFormat = reinterpret_cast<Fn_ASIGetROIFormat>(load_fn("ASIGetROIFormat"));
    p_ASISetROIFormat = reinterpret_cast<Fn_ASISetROIFormat>(load_fn("ASISetROIFormat"));
    p_ASIGetDroppedFrames = reinterpret_cast<Fn_ASIGetDroppedFrames>(load_fn("ASIGetDroppedFrames"));
    p_ASIStartVideoCapture = reinterpret_cast<Fn_ASIStartVideoCapture>(load_fn("ASIStartVideoCapture"));
    p_ASIStopVideoCapture = reinterpret_cast<Fn_ASIStopVideoCapture>(load_fn("ASIStopVideoCapture"));
    p_ASIGetVideoData = reinterpret_cast<Fn_ASIGetVideoData>(load_fn("ASIGetVideoData"));
    p_ASIGetSDKVersion = reinterpret_cast<Fn_ASIGetSDKVersion>(load_fn("ASIGetSDKVersion"));

    return (p_ASIGetNumOfConnectedCameras && p_ASIOpenCamera && p_ASIInitCamera);
}

void ZwoCamera::unload_sdk() {
    if (dll_handle_) {
        FreeLibrary(reinterpret_cast<HMODULE>(dll_handle_));
        dll_handle_ = nullptr;
    }
}

int ZwoCamera::get_connected_camera_count(const std::string& dll_path) {
    ZwoCamera temp(dll_path);
    if (!temp.load_sdk() || !temp.p_ASIGetNumOfConnectedCameras) {
        return 0;
    }
    return temp.p_ASIGetNumOfConnectedCameras();
}

std::vector<CameraId> ZwoCamera::enumerate_cameras(const std::string& dll_path) {
    std::vector<CameraId> list;
    ZwoCamera temp(dll_path);
    if (!temp.load_sdk() || !temp.p_ASIGetNumOfConnectedCameras || !temp.p_ASIGetCameraProperty) {
        return list;
    }

    int count = temp.p_ASIGetNumOfConnectedCameras();
    for (int i = 0; i < count; ++i) {
        ASI_CAMERA_INFO prop{};
        if (temp.p_ASIGetCameraProperty(&prop, i) == ASI_SUCCESS) {
            CameraId cid;
            cid.driver = "zwo";
            cid.id = std::to_string(prop.CameraID);
            cid.name = std::string(prop.Name);
            list.push_back(cid);
        }
    }
    return list;
}

core::Result<CameraCaps> ZwoCamera::open(const CameraId& id) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (is_open_) {
        return core::Result<CameraCaps>::err(core::ErrorCode::Busy, "Camera already open");
    }

    if (!load_sdk()) {
        return core::Result<CameraCaps>::err(core::ErrorCode::DriverMissing, "ASICamera2.dll could not be loaded");
    }

    int num_cams = p_ASIGetNumOfConnectedCameras();
    if (num_cams <= 0) {
        return core::Result<CameraCaps>::err(core::ErrorCode::NotFound, "No ZWO cameras physically connected");
    }

    camera_index_ = 0;
    if (!id.id.empty() && id.id != "auto") {
        try {
            int target_id = std::stoi(id.id);
            for (int i = 0; i < num_cams; ++i) {
                ASI_CAMERA_INFO prop{};
                if (p_ASIGetCameraProperty(&prop, i) == ASI_SUCCESS) {
                    if (prop.CameraID == target_id || i == target_id) {
                        camera_index_ = i;
                        break;
                    }
                }
            }
        } catch (...) {
            camera_index_ = 0;
        }
    }

    if (p_ASIGetCameraProperty(&camera_info_, camera_index_) != ASI_SUCCESS) {
        return core::Result<CameraCaps>::err(core::ErrorCode::NotFound, "Failed to get camera property for index: " + std::to_string(camera_index_));
    }

    camera_id_ = camera_info_.CameraID;
    if (p_ASIOpenCamera(camera_id_) != ASI_SUCCESS) {
        return core::Result<CameraCaps>::err(core::ErrorCode::HardwareError, "ASIOpenCamera failed for ID: " + std::to_string(camera_id_));
    }

    if (p_ASIInitCamera(camera_id_) != ASI_SUCCESS) {
        p_ASICloseCamera(camera_id_);
        return core::Result<CameraCaps>::err(core::ErrorCode::HardwareError, "ASIInitCamera failed for ID: " + std::to_string(camera_id_));
    }

    is_open_ = true;
    is_disconnected_ = false;

    // Populate Caps
    caps_.model = camera_info_.Name;
    caps_.serial = "ZWO-" + std::to_string(camera_id_);
    caps_.driver = "zwo";
    if (p_ASIGetSDKVersion) {
        caps_.sdk_version = p_ASIGetSDKVersion();
    } else {
        caps_.sdk_version = "ZWO ASI SDK";
    }
    caps_.max_width = static_cast<int>(camera_info_.MaxWidth);
    caps_.max_height = static_cast<int>(camera_info_.MaxHeight);
    caps_.has_cooler = (camera_info_.IsCoolerCam == ASI_TRUE);
    caps_.is_color = (camera_info_.IsColorCam == ASI_TRUE);
    caps_.bayer = static_cast<core::BayerPattern>(camera_info_.BayerPattern + 1);

    caps_.capabilities.can_control_exposure = true;
    caps_.capabilities.can_control_gain = true;
    caps_.capabilities.can_control_wb = caps_.is_color;
    caps_.capabilities.can_control_roi = true;
    caps_.capabilities.can_control_pixel_format = true;
    caps_.capabilities.has_hardware_exposure = true;
    caps_.capabilities.has_hardware_gain = true;
    caps_.capabilities.has_software_fallback = false;
    caps_.capabilities.can_auto_exposure = true;
    caps_.capabilities.can_auto_gain = true;
    caps_.capabilities.can_crop_dslr_aspect = false;

    // Initial default settings
    active_settings_.exposure_us = 100000;
    active_settings_.gain = 100.0;
    active_settings_.binning = 1;
    active_settings_.bit_depth = 16;

    // Apply baseline RAW16 format
    p_ASISetROIFormat(camera_id_, caps_.max_width, caps_.max_height, 1, ASI_IMG_RAW16);
    p_ASISetControlValue(camera_id_, ASI_EXPOSURE, 100000, ASI_FALSE);
    p_ASISetControlValue(camera_id_, ASI_GAIN, 100, ASI_FALSE);

    SABIT_LOG_INFO("Successfully opened ZWO Camera: ", caps_.model, " (ID: ", camera_id_, ")");
    return caps_;
}

void ZwoCamera::close() {
    stop();
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (is_open_) {
        if (p_ASICloseCamera && camera_id_ >= 0) {
            p_ASICloseCamera(camera_id_);
        }
        is_open_ = false;
        camera_id_ = -1;
    }
}

CameraCaps ZwoCamera::caps() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return caps_;
}

CameraStatus ZwoCamera::status() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    CameraStatus s;
    s.is_connected = is_open_ && !is_disconnected_.load();
    s.is_streaming = is_streaming_.load();
    s.actual_fps = actual_fps_;
    s.frames_captured = frames_captured_;
    s.frames_dropped_hw = frames_dropped_hw_;
    s.sensor_temp_c = sensor_temp_c_;
    return s;
}

core::Result<CameraSettings> ZwoCamera::apply(const CameraSettings& settings) {
    if (!is_open_) {
        return core::Result<CameraSettings>::err(core::ErrorCode::InvalidState, "Camera is not open");
    }

    if (is_streaming_.load()) {
        // Enqueue command to be executed in camera thread with coalescing (non-blocking UI)
        std::lock_guard<std::mutex> lock(cmd_mutex_);
        if (settings.auto_exposure.has_value()) active_settings_.auto_exposure = settings.auto_exposure;
        if (settings.exposure_us.has_value()) active_settings_.exposure_us = std::clamp<int64_t>(*settings.exposure_us, 32, 3600000000LL);
        if (settings.auto_gain.has_value()) active_settings_.auto_gain = settings.auto_gain;
        if (settings.gain.has_value()) active_settings_.gain = std::clamp(*settings.gain, 0.0, 600.0);
        if (settings.wb_red.has_value()) active_settings_.wb_red = settings.wb_red;
        if (settings.wb_blue.has_value()) active_settings_.wb_blue = settings.wb_blue;

        while (!cmd_queue_.empty()) {
            auto stale = std::move(cmd_queue_.front());
            cmd_queue_.pop();
            if (stale.promise) {
                stale.promise->set_value(core::Result<CameraSettings>::ok(active_settings_));
            }
        }
        cmd_queue_.push(Command{settings, nullptr});
        return core::Result<CameraSettings>::ok(active_settings_);
    } else {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return execute_apply_hardware(settings);
    }
}

core::Result<CameraSettings> ZwoCamera::execute_apply_hardware(const CameraSettings& req) {
    if (!is_open_ || camera_id_ < 0) {
        return core::Result<CameraSettings>::err(core::ErrorCode::InvalidState, "Hardware unavailable");
    }

    // 1. Exposure
    ASI_BOOL exp_auto = (req.auto_exposure.value_or(active_settings_.auto_exposure.value_or(false))) ? ASI_TRUE : ASI_FALSE;
    if (req.exposure_us.has_value() || req.auto_exposure.has_value()) {
        int64_t exp = req.exposure_us.value_or(active_settings_.exposure_us.value_or(100000));
        if (exp < 32 || exp > 3600000000LL) {
            return core::Result<CameraSettings>::err(core::ErrorCode::OutOfRange, "Exposure out of valid range");
        }
        p_ASISetControlValue(camera_id_, ASI_EXPOSURE, static_cast<long>(exp), exp_auto);
    }

    // 2. Gain
    ASI_BOOL gain_auto = (req.auto_gain.value_or(active_settings_.auto_gain.value_or(false))) ? ASI_TRUE : ASI_FALSE;
    if (req.gain.has_value() || req.auto_gain.has_value()) {
        double g = req.gain.value_or(active_settings_.gain.value_or(100.0));
        if (g < 0.0 || g > 600.0) {
            return core::Result<CameraSettings>::err(core::ErrorCode::OutOfRange, "Gain out of range [0, 600]");
        }
        p_ASISetControlValue(camera_id_, ASI_GAIN, static_cast<long>(g), gain_auto);
    }

    // 2b. White Balance (Red & Blue)
    if (req.wb_red.has_value()) {
        p_ASISetControlValue(camera_id_, ASI_WB_R, static_cast<long>(*req.wb_red), ASI_FALSE);
    }
    if (req.wb_blue.has_value()) {
        p_ASISetControlValue(camera_id_, ASI_WB_B, static_cast<long>(*req.wb_blue), ASI_FALSE);
    }

    // 3. Binning & Bit depth
    int bin = req.binning.value_or(active_settings_.binning.value_or(1));
    int bd = req.bit_depth.value_or(active_settings_.bit_depth.value_or(16));
    ASI_IMG_TYPE img_type = (bd == 8) ? ASI_IMG_RAW8 : ASI_IMG_RAW16;

    int cur_w = 0, cur_h = 0, cur_bin = 0;
    ASI_IMG_TYPE cur_type = ASI_IMG_RAW16;
    p_ASIGetROIFormat(camera_id_, &cur_w, &cur_h, &cur_bin, &cur_type);

    int target_w = caps_.max_width / bin;
    int target_h = caps_.max_height / bin;
    if (target_w != cur_w || target_h != cur_h || bin != cur_bin || img_type != cur_type) {
        p_ASISetROIFormat(camera_id_, target_w, target_h, bin, img_type);
    }

    // 4. Cooler
    if (caps_.has_cooler && req.cooler.has_value()) {
        const auto& c = *req.cooler;
        p_ASISetControlValue(camera_id_, ASI_COOLER_ON, c.enabled ? ASI_TRUE : ASI_FALSE, ASI_FALSE);
        if (c.enabled && c.target_c.has_value()) {
            p_ASISetControlValue(camera_id_, ASI_TARGET_TEMP, static_cast<long>(*c.target_c), ASI_FALSE);
        }
    }

    // Readback verification (FR-04)
    long exp_read = 0, gain_read = 0;
    ASI_BOOL b_auto = ASI_FALSE;
    p_ASIGetControlValue(camera_id_, ASI_EXPOSURE, &exp_read, &b_auto);
    p_ASIGetControlValue(camera_id_, ASI_GAIN, &gain_read, &b_auto);

    p_ASIGetROIFormat(camera_id_, &cur_w, &cur_h, &cur_bin, &cur_type);

    CameraSettings applied;
    applied.exposure_us = exp_read;
    applied.gain = static_cast<double>(gain_read);
    applied.binning = cur_bin;
    applied.bit_depth = (cur_type == ASI_IMG_RAW8) ? 8 : 16;
    active_settings_ = applied;

    return applied;
}

core::Result<void> ZwoCamera::start(FrameCallback on_frame) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!is_open_) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "Camera is not open");
    }
    if (is_streaming_.load()) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "Streaming already active");
    }

    frame_callback_ = std::move(on_frame);
    is_streaming_.store(true);
    is_disconnected_.store(false);

    worker_thread_ = std::thread(&ZwoCamera::camera_worker_loop, this);
    return core::Result<void>::ok();
}

void ZwoCamera::stop() {
    if (is_streaming_.exchange(false)) {
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
}

bool ZwoCamera::try_reconnect() {
    // Exponential backoff: 1, 2, 4, 8 seconds (max 5 attempts, FR-05)
    int delays_sec[] = {1, 2, 4, 8, 8};
    for (int attempt = 0; attempt < 5; ++attempt) {
        SABIT_LOG_INFO("[ZWO Reconnect] Attempt ", attempt + 1, " after ", delays_sec[attempt], "s...");
        std::this_thread::sleep_for(std::chrono::seconds(delays_sec[attempt]));

        if (!is_streaming_.load()) return false;

        int n = p_ASIGetNumOfConnectedCameras();
        if (n > 0) {
            if (p_ASIOpenCamera(camera_id_) == ASI_SUCCESS && p_ASIInitCamera(camera_id_) == ASI_SUCCESS) {
                // Reapply last active settings
                execute_apply_hardware(active_settings_);
                p_ASIStartVideoCapture(camera_id_);
                is_disconnected_.store(false);
                SABIT_LOG_INFO("[ZWO Reconnect] Successfully reconnected to camera ID: ", camera_id_);
                return true;
            }
        }
    }
    return false;
}

void ZwoCamera::camera_worker_loop() {
    if (p_ASIStartVideoCapture(camera_id_) != ASI_SUCCESS) {
        SABIT_LOG_ERROR("Failed to start video capture on ZWO camera");
        is_streaming_.store(false);
        return;
    }

    uint64_t seq = 0;
    auto last_fps_time = std::chrono::steady_clock::now();
    uint64_t frames_in_second = 0;

    while (is_streaming_.load()) {
        // 1. Process CMDQ queue
        {
            std::lock_guard<std::mutex> lock(cmd_mutex_);
            while (!cmd_queue_.empty()) {
                auto cmd = std::move(cmd_queue_.front());
                cmd_queue_.pop();

                auto res = execute_apply_hardware(cmd.settings);
                if (cmd.promise) {
                    cmd.promise->set_value(res);
                }
            }
        }

        // 2. Query ROI format to know buffer size
        int w = 0, h = 0, bin = 1;
        ASI_IMG_TYPE img_type = ASI_IMG_RAW16;
        p_ASIGetROIFormat(camera_id_, &w, &h, &bin, &img_type);

        int bytes_per_pixel = (img_type == ASI_IMG_RAW8) ? 1 : 2;
        long buffer_size = static_cast<long>(w * h * bytes_per_pixel);

        core::Frame frame = core::Frame::create_allocated(w, h, 1, 16);
        frame.seq = ++seq;
        frame.bayer = caps_.bayer;

        // 3. Capture frame data (handle 8-bit vs 16-bit buffer properly)
        std::vector<uint8_t> raw8_buf;
        unsigned char* capture_ptr = nullptr;
        if (img_type == ASI_IMG_RAW8) {
            raw8_buf.resize(static_cast<size_t>(buffer_size));
            capture_ptr = raw8_buf.data();
        } else {
            capture_ptr = reinterpret_cast<unsigned char*>(frame.pixels.get());
        }

        ASI_ERROR_CODE err = p_ASIGetVideoData(
            camera_id_,
            capture_ptr,
            buffer_size,
            200 // timeout in ms
        );

        if (err == ASI_SUCCESS) {
            if (img_type == ASI_IMG_RAW8) {
                uint16_t* dst = frame.pixels.get();
                size_t total = static_cast<size_t>(w) * static_cast<size_t>(h);
                for (size_t i = 0; i < total; ++i) {
                    uint16_t v = raw8_buf[i];
                    dst[i] = static_cast<uint16_t>((v << 8) | v);
                }
            }
            // Read temperature periodically
            long temp_raw = 0;
            ASI_BOOL b_dummy = ASI_FALSE;
            if (p_ASIGetControlValue(camera_id_, ASI_TEMPERATURE, &temp_raw, &b_dummy) == ASI_SUCCESS) {
                sensor_temp_c_ = temp_raw / 10.0;
            }
            frame.sensor_temp_c = sensor_temp_c_;
            frame.exposure_us = active_settings_.exposure_us.value_or(100000);
            frame.gain = active_settings_.gain.value_or(100.0);
            frame.binning = bin;
            frame.timestamp_utc = core::Clock::now_utc();
            frame.clock_sync = core::Clock::get_sync();

            ++frames_captured_;
            ++frames_in_second;

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time).count();
            if (elapsed >= 1000) {
                actual_fps_ = (static_cast<double>(frames_in_second) * 1000.0) / elapsed;
                frames_in_second = 0;
                last_fps_time = now;
            }

            if (frame_callback_) {
                frame_callback_(frame);
            }
        } else if (err == ASI_ERROR_CAMERA_REMOVED || err == ASI_ERROR_CAMERA_CLOSED) {
            SABIT_LOG_ERROR("ZWO Camera disconnected! (Error: ", static_cast<int>(err), ")");
            is_disconnected_.store(true);
            if (on_disconnected_) {
                on_disconnected_();
            }

            if (auto_reconnect_) {
                if (!try_reconnect()) {
                    is_streaming_.store(false);
                    break;
                }
            } else {
                is_streaming_.store(false);
                break;
            }
        } else {
            // Drop or timeout
            int dropped = 0;
            if (p_ASIGetDroppedFrames(camera_id_, &dropped) == ASI_SUCCESS) {
                frames_dropped_hw_ = dropped;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    p_ASIStopVideoCapture(camera_id_);
}

} // namespace sabit::camera
