#include "src/camera/qhy/qhy_camera.h"
#include "src/core/clock.h"
#include "src/core/logger.h"

#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>

namespace sabit::camera {

namespace {
constexpr uint32_t QHYCCD_SUCCESS_CODE = 0;
std::mutex g_qhy_sdk_mutex;
bool g_qhy_resource_initialized = false;
} // namespace

QhyCamera::QhyCamera(const std::string& dll_path) : dll_path_(dll_path) {}

QhyCamera::~QhyCamera() {
    close();
    unload_sdk();
}

bool QhyCamera::load_sdk() {
    if (dll_handle_) return true;

    char exe_buf[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, exe_buf, MAX_PATH);
    std::filesystem::path exe_dir = std::filesystem::path(exe_buf).parent_path();

    const std::vector<std::filesystem::path> candidates = {
        dll_path_,
        "qhyccd.dll",
        "vendor/qhy/qhyccd.dll",
        ".onyx_ref/Onyx_Test/qhyccd.dll",
        exe_dir / "qhyccd.dll",
        exe_dir / "vendor" / "qhy" / "qhyccd.dll",
        exe_dir.parent_path() / "vendor" / "qhy" / "qhyccd.dll",
        exe_dir.parent_path() / ".onyx_ref" / "Onyx_Test" / "qhyccd.dll"
    };

    HMODULE mod = nullptr;
    for (const auto& cand : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(cand, ec)) {
            auto parent = std::filesystem::absolute(cand, ec).parent_path();
            if (!parent.empty()) {
                SetDllDirectoryA(parent.string().c_str());
            }
            mod = LoadLibraryA(cand.string().c_str());
            if (mod) break;
        }
    }

    if (!mod) {
        mod = LoadLibraryA("qhyccd.dll");
    }
    SetDllDirectoryA(NULL);

    if (!mod) {
        SABIT_LOG_WARN("QHYCCD SDK DLL (qhyccd.dll) not found.");
        return false;
    }

    dll_handle_ = reinterpret_cast<void*>(mod);

    auto load_fn = [mod](const char* name) {
        return reinterpret_cast<void*>(GetProcAddress(mod, name));
    };

    p_InitQHYCCDResource       = reinterpret_cast<Fn_InitQHYCCDResource>(load_fn("InitQHYCCDResource"));
    p_ReleaseQHYCCDResource    = reinterpret_cast<Fn_ReleaseQHYCCDResource>(load_fn("ReleaseQHYCCDResource"));
    p_ScanQHYCCD               = reinterpret_cast<Fn_ScanQHYCCD>(load_fn("ScanQHYCCD"));
    p_GetQHYCCDId              = reinterpret_cast<Fn_GetQHYCCDId>(load_fn("GetQHYCCDId"));
    p_OpenQHYCCD               = reinterpret_cast<Fn_OpenQHYCCD>(load_fn("OpenQHYCCD"));
    p_CloseQHYCCD              = reinterpret_cast<Fn_CloseQHYCCD>(load_fn("CloseQHYCCD"));
    p_SetQHYCCDStreamMode      = reinterpret_cast<Fn_SetQHYCCDStreamMode>(load_fn("SetQHYCCDStreamMode"));
    p_InitQHYCCD               = reinterpret_cast<Fn_InitQHYCCD>(load_fn("InitQHYCCD"));
    p_GetQHYCCDChipInfo        = reinterpret_cast<Fn_GetQHYCCDChipInfo>(load_fn("GetQHYCCDChipInfo"));
    p_IsQHYCCDControlAvailable = reinterpret_cast<Fn_IsQHYCCDControlAvailable>(load_fn("IsQHYCCDControlAvailable"));
    p_SetQHYCCDParam           = reinterpret_cast<Fn_SetQHYCCDParam>(load_fn("SetQHYCCDParam"));
    p_GetQHYCCDParam           = reinterpret_cast<Fn_GetQHYCCDParam>(load_fn("GetQHYCCDParam"));
    p_GetQHYCCDParamMinMaxStep = reinterpret_cast<Fn_GetQHYCCDParamMinMaxStep>(load_fn("GetQHYCCDParamMinMaxStep"));
    p_SetQHYCCDBitsMode        = reinterpret_cast<Fn_SetQHYCCDBitsMode>(load_fn("SetQHYCCDBitsMode"));
    p_SetQHYCCDBinMode         = reinterpret_cast<Fn_SetQHYCCDBinMode>(load_fn("SetQHYCCDBinMode"));
    p_SetQHYCCDResolution      = reinterpret_cast<Fn_SetQHYCCDResolution>(load_fn("SetQHYCCDResolution"));
    p_GetQHYCCDMemLength       = reinterpret_cast<Fn_GetQHYCCDMemLength>(load_fn("GetQHYCCDMemLength"));
    p_BeginQHYCCDLive          = reinterpret_cast<Fn_BeginQHYCCDLive>(load_fn("BeginQHYCCDLive"));
    p_GetQHYCCDLiveFrame       = reinterpret_cast<Fn_GetQHYCCDLiveFrame>(load_fn("GetQHYCCDLiveFrame"));
    p_StopQHYCCDLive           = reinterpret_cast<Fn_StopQHYCCDLive>(load_fn("StopQHYCCDLive"));
    p_ExpQHYCCDSingleFrame     = reinterpret_cast<Fn_ExpQHYCCDSingleFrame>(load_fn("ExpQHYCCDSingleFrame"));
    p_GetQHYCCDSingleFrame     = reinterpret_cast<Fn_GetQHYCCDSingleFrame>(load_fn("GetQHYCCDSingleFrame"));
    p_ControlQHYCCDTemp        = reinterpret_cast<Fn_ControlQHYCCDTemp>(load_fn("ControlQHYCCDTemp"));

    return (p_InitQHYCCDResource && p_ScanQHYCCD && p_GetQHYCCDId && p_OpenQHYCCD && p_InitQHYCCD);
}

void QhyCamera::unload_sdk() {
    if (dll_handle_) {
        // Note: Do not call ReleaseQHYCCDResource() here as it resets global libusb handles across active instances
        resource_initialized_ = false;
        FreeLibrary(reinterpret_cast<HMODULE>(dll_handle_));
        dll_handle_ = nullptr;
    }
}

int QhyCamera::get_connected_camera_count(const std::string& dll_path) {
    std::lock_guard<std::mutex> sdk_lk(g_qhy_sdk_mutex);
    QhyCamera temp(dll_path);
    if (!temp.load_sdk()) return 0;
    if (!g_qhy_resource_initialized) {
        if (temp.p_InitQHYCCDResource() != QHYCCD_SUCCESS_CODE) return 0;
        g_qhy_resource_initialized = true;
    }
    uint32_t count = temp.p_ScanQHYCCD();
    return (count == 0xFFFFFFFFU) ? 0 : static_cast<int>(count);
}

std::vector<CameraId> QhyCamera::enumerate_cameras(const std::string& dll_path) {
    std::lock_guard<std::mutex> sdk_lk(g_qhy_sdk_mutex);
    std::vector<CameraId> list;
    QhyCamera temp(dll_path);
    if (!temp.load_sdk()) return list;

    if (!g_qhy_resource_initialized) {
        if (temp.p_InitQHYCCDResource() != QHYCCD_SUCCESS_CODE) return list;
        g_qhy_resource_initialized = true;
    }

    uint32_t count = temp.p_ScanQHYCCD();
    if (count == 0 || count == 0xFFFFFFFFU) return list;

    for (uint32_t i = 0; i < count; ++i) {
        char id_buf[64] = {0};
        if (temp.p_GetQHYCCDId(i, id_buf) == QHYCCD_SUCCESS_CODE && id_buf[0] != '\0') {
            CameraId cid;
            cid.driver = "qhy";
            cid.id = std::string(id_buf);
            cid.name = "QHYCCD " + std::string(id_buf);
            list.push_back(cid);
        }
    }
    return list;
}

core::Result<CameraCaps> QhyCamera::open(const CameraId& id) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (is_open_) {
        return core::Result<CameraCaps>::err(core::ErrorCode::Busy, "QHYCCD Camera already open");
    }

    if (!load_sdk()) {
        return core::Result<CameraCaps>::err(core::ErrorCode::DriverMissing, "qhyccd.dll could not be loaded");
    }

    {
        std::lock_guard<std::mutex> sdk_lk(g_qhy_sdk_mutex);
        if (!g_qhy_resource_initialized) {
            p_InitQHYCCDResource();
            g_qhy_resource_initialized = true;
        }
        resource_initialized_ = true;
    }

    uint32_t num_cams = p_ScanQHYCCD();
    if (num_cams == 0 || num_cams == 0xFFFFFFFFU) {
        return core::Result<CameraCaps>::err(core::ErrorCode::NotFound, "No QHYCCD cameras physically connected on USB bus");
    }

    char selected_id[64] = {0};
    if (!id.id.empty() && id.id != "0" && id.id != "auto") {
        std::strncpy(selected_id, id.id.c_str(), sizeof(selected_id) - 1);
    } else {
        if (p_GetQHYCCDId(0, selected_id) != QHYCCD_SUCCESS_CODE || selected_id[0] == '\0') {
            return core::Result<CameraCaps>::err(core::ErrorCode::NotFound, "Failed to retrieve QHYCCD camera ID");
        }
    }

    hw_id_ = selected_id;
    cam_handle_ = p_OpenQHYCCD(selected_id);
    if (!cam_handle_) {
        return core::Result<CameraCaps>::err(core::ErrorCode::HardwareError, "OpenQHYCCD failed for ID: " + hw_id_);
    }

    // Set Live Video Stream Mode (mode = 1) before InitQHYCCD
    if (p_SetQHYCCDStreamMode) {
        p_SetQHYCCDStreamMode(cam_handle_, 1);
    }

    if (p_InitQHYCCD(cam_handle_) != QHYCCD_SUCCESS_CODE) {
        if (p_CloseQHYCCD) p_CloseQHYCCD(cam_handle_);
        cam_handle_ = nullptr;
        return core::Result<CameraCaps>::err(core::ErrorCode::HardwareError, "InitQHYCCD failed for ID: " + hw_id_);
    }

    // Query Chip Info
    if (p_GetQHYCCDChipInfo) {
        p_GetQHYCCDChipInfo(
            cam_handle_,
            &chip_w_mm_, &chip_h_mm_,
            &max_w_, &max_h_,
            &pixel_w_um_, &pixel_h_um_,
            &bpp_
        );
    }
    if (max_w_ == 0) max_w_ = 1920;
    if (max_h_ == 0) max_h_ = 1080;
    if (bpp_ != 8 && bpp_ != 16) bpp_ = 16;

    roi_w_ = max_w_;
    roi_h_ = max_h_;

    caps_.model = "QHYCCD " + hw_id_;
    caps_.serial = hw_id_;
    caps_.driver = "qhy";
    caps_.sdk_version = "QHYCCD Native C-SDK";
    caps_.max_width = static_cast<int>(max_w_);
    caps_.max_height = static_cast<int>(max_h_);
    caps_.supported_bins = {1, 2, 3, 4};
    caps_.supported_bit_depths = {8, 16};
    caps_.has_cooler = (p_IsQHYCCDControlAvailable && p_IsQHYCCDControlAvailable(cam_handle_, QHY_CONTROL_COOLER) == QHYCCD_SUCCESS_CODE);
    caps_.is_color = (p_IsQHYCCDControlAvailable && p_IsQHYCCDControlAvailable(cam_handle_, QHY_CAM_COLOR) == QHYCCD_SUCCESS_CODE);
    caps_.bayer = caps_.is_color ? core::BayerPattern::RGGB : core::BayerPattern::None;

    caps_.capabilities.can_control_exposure = true;
    caps_.capabilities.can_control_gain = true;
    caps_.capabilities.can_control_wb = caps_.is_color;
    caps_.capabilities.can_control_roi = true;
    caps_.capabilities.can_control_pixel_format = true;
    caps_.capabilities.has_hardware_exposure = true;
    caps_.capabilities.has_hardware_gain = true;
    caps_.capabilities.has_software_fallback = false;
    caps_.capabilities.can_auto_exposure = false;
    caps_.capabilities.can_auto_gain = false;
    caps_.capabilities.can_crop_dslr_aspect = false;

    if (p_GetQHYCCDParamMinMaxStep) {
        double min_v = 0, max_v = 0, step_v = 0;
        if (p_GetQHYCCDParamMinMaxStep(cam_handle_, QHY_CONTROL_EXPOSURE, &min_v, &max_v, &step_v) == QHYCCD_SUCCESS_CODE && max_v > min_v) {
            caps_.min_exposure_us = static_cast<int64_t>(min_v);
            caps_.max_exposure_us = static_cast<int64_t>(max_v);
        }
        if (p_GetQHYCCDParamMinMaxStep(cam_handle_, QHY_CONTROL_GAIN, &min_v, &max_v, &step_v) == QHYCCD_SUCCESS_CODE && max_v > min_v) {
            caps_.min_gain = min_v;
            caps_.max_gain = max_v;
        }
    }

    // Configure initial baseline parameters matching qhyccd.py
    active_settings_.exposure_us = 100000; // 100 ms
    active_settings_.gain = 100.0;
    active_settings_.binning = 1;
    active_settings_.bit_depth = 16;

    if (p_SetQHYCCDParam) {
        p_SetQHYCCDParam(cam_handle_, QHY_CONTROL_USBTRAFFIC, 50.0);
        p_SetQHYCCDParam(cam_handle_, QHY_CONTROL_TRANSFERBIT, 16.0);
        p_SetQHYCCDParam(cam_handle_, QHY_CONTROL_EXPOSURE, 100000.0);
        p_SetQHYCCDParam(cam_handle_, QHY_CONTROL_GAIN, 100.0);
    }
    if (p_SetQHYCCDBitsMode) {
        p_SetQHYCCDBitsMode(cam_handle_, 16);
    }
    if (p_SetQHYCCDBinMode) {
        p_SetQHYCCDBinMode(cam_handle_, 1, 1);
    }
    if (p_SetQHYCCDResolution) {
        p_SetQHYCCDResolution(cam_handle_, 0, 0, roi_w_, roi_h_);
    }

    is_open_ = true;
    is_disconnected_.store(false);

    SABIT_LOG_INFO("Successfully opened QHYCCD Camera: ", caps_.model, " (", max_w_, "x", max_h_, ")");
    return core::Result<CameraCaps>::ok(caps_);
}

void QhyCamera::close() {
    stop();
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (is_open_) {
        if (cam_handle_ && p_CloseQHYCCD) {
            p_CloseQHYCCD(cam_handle_);
        }
        cam_handle_ = nullptr;
        is_open_ = false;
    }
}

CameraCaps QhyCamera::caps() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return caps_;
}

CameraStatus QhyCamera::status() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    CameraStatus s{};
    s.is_connected = is_open_ && !is_disconnected_.load();
    s.is_streaming = is_streaming_.load();
    s.actual_fps = actual_fps_;
    s.frames_captured = frames_captured_;
    s.frames_dropped_hw = frames_dropped_hw_;
    s.sensor_temp_c = sensor_temp_c_;
    s.cooler_power_percent = cooler_power_pct_;
    return s;
}

core::Result<CameraSettings> QhyCamera::apply(const CameraSettings& settings) {
    if (!is_open_) {
        return core::Result<CameraSettings>::err(core::ErrorCode::InvalidState, "QHYCCD Camera is not open");
    }

    if (is_streaming_.load()) {
        std::lock_guard<std::mutex> lock(cmd_mutex_);
        if (settings.auto_exposure.has_value()) active_settings_.auto_exposure = settings.auto_exposure;
        if (settings.exposure_us.has_value()) active_settings_.exposure_us = std::clamp<int64_t>(*settings.exposure_us, 1, 3600000000LL);
        if (settings.auto_gain.has_value()) active_settings_.auto_gain = settings.auto_gain;
        if (settings.gain.has_value()) active_settings_.gain = std::clamp(*settings.gain, 0.0, 1000.0);
        if (settings.wb_red.has_value()) active_settings_.wb_red = settings.wb_red;
        if (settings.wb_blue.has_value()) active_settings_.wb_blue = settings.wb_blue;

        // Coalesce pending commands so rapid slider drags never block the UI thread
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

core::Result<CameraSettings> QhyCamera::execute_apply_hardware(const CameraSettings& req) {
    if (!is_open_ || !cam_handle_) {
        return core::Result<CameraSettings>::err(core::ErrorCode::InvalidState, "QHYCCD Hardware unavailable");
    }

    // 1. Auto Exposure / Exposure
    if (req.auto_exposure.has_value() && p_SetQHYCCDParam) {
        p_SetQHYCCDParam(cam_handle_, QHY_3A_AUTOEXPOSURE, *req.auto_exposure ? 1.0 : 0.0);
        active_settings_.auto_exposure = req.auto_exposure;
    }
    if (req.exposure_us.has_value() && p_SetQHYCCDParam) {
        int64_t exp = *req.exposure_us;
        if (exp < 1 || exp > 3600000000LL) {
            return core::Result<CameraSettings>::err(core::ErrorCode::OutOfRange, "Exposure out of valid range");
        }
        p_SetQHYCCDParam(cam_handle_, QHY_CONTROL_EXPOSURE, static_cast<double>(exp));
        active_settings_.exposure_us = exp;
    }

    // 2. Gain
    if (req.gain.has_value() && p_SetQHYCCDParam) {
        double g = *req.gain;
        if (g < 0.0 || g > 1000.0) {
            return core::Result<CameraSettings>::err(core::ErrorCode::OutOfRange, "Gain out of valid range");
        }
        p_SetQHYCCDParam(cam_handle_, QHY_CONTROL_GAIN, g);
        active_settings_.gain = g;
    }

    // 3. White Balance (Red & Blue)
    if (req.wb_red.has_value() && p_SetQHYCCDParam) {
        p_SetQHYCCDParam(cam_handle_, QHY_CONTROL_WBR, static_cast<double>(*req.wb_red));
        active_settings_.wb_red = req.wb_red;
    }
    if (req.wb_blue.has_value() && p_SetQHYCCDParam) {
        p_SetQHYCCDParam(cam_handle_, QHY_CONTROL_WBB, static_cast<double>(*req.wb_blue));
        active_settings_.wb_blue = req.wb_blue;
    }

    // 4. Bit Depth (8-bit or 16-bit) — only reconfigure USB resolution if bit depth actually changed
    if (req.bit_depth.has_value()) {
        int bd = (*req.bit_depth == 8) ? 8 : 16;
        if (static_cast<uint32_t>(bd) != bpp_) {
            bpp_ = static_cast<uint32_t>(bd);
            if (p_SetQHYCCDParam) {
                p_SetQHYCCDParam(cam_handle_, QHY_CONTROL_TRANSFERBIT, static_cast<double>(bd));
            }
            if (p_SetQHYCCDBitsMode) {
                p_SetQHYCCDBitsMode(cam_handle_, bpp_);
            }
            if (p_SetQHYCCDResolution) {
                p_SetQHYCCDResolution(cam_handle_, 0, 0, roi_w_, roi_h_);
            }
        }
        active_settings_.bit_depth = bd;
    }

    // 5. Hardware Binning (1x1, 2x2, 3x3, 4x4) — only reconfigure ROI if binning actually changed
    if (req.binning.has_value()) {
        int bin = std::clamp(*req.binning, 1, 4);
        uint32_t target_w = std::max<uint32_t>(1, max_w_ / static_cast<uint32_t>(bin));
        uint32_t target_h = std::max<uint32_t>(1, max_h_ / static_cast<uint32_t>(bin));
        if (target_w != roi_w_ || target_h != roi_h_) {
            if (p_SetQHYCCDBinMode) {
                p_SetQHYCCDBinMode(cam_handle_, static_cast<uint32_t>(bin), static_cast<uint32_t>(bin));
            }
            roi_w_ = target_w;
            roi_h_ = target_h;
            if (p_SetQHYCCDResolution) {
                p_SetQHYCCDResolution(cam_handle_, 0, 0, roi_w_, roi_h_);
            }
        }
        active_settings_.binning = bin;
    }

    // 6. TEC Cooler
    if (req.cooler.has_value()) {
        const auto& c = *req.cooler;
        double target_c = (c.enabled && c.target_c.has_value()) ? *c.target_c : 30.0;
        if (p_ControlQHYCCDTemp && c.enabled) {
            p_ControlQHYCCDTemp(cam_handle_, target_c);
        } else if (p_SetQHYCCDParam) {
            if (!c.enabled) {
                p_SetQHYCCDParam(cam_handle_, QHY_CONTROL_MANULPWM, 0.0);
            } else {
                p_SetQHYCCDParam(cam_handle_, QHY_CONTROL_COOLER, target_c);
            }
        }
        active_settings_.cooler = c;
    }

    // Readback actual exposure & gain if available
    if (p_GetQHYCCDParam) {
        double exp_rb = p_GetQHYCCDParam(cam_handle_, QHY_CONTROL_EXPOSURE);
        if (exp_rb > 0.0 && exp_rb < 3.6e9) {
            active_settings_.exposure_us = static_cast<int64_t>(exp_rb);
        }
        double gain_rb = p_GetQHYCCDParam(cam_handle_, QHY_CONTROL_GAIN);
        if (gain_rb >= 0.0 && gain_rb <= 1000.0) {
            active_settings_.gain = gain_rb;
        }
    }

    return core::Result<CameraSettings>::ok(active_settings_);
}

core::Result<void> QhyCamera::start(FrameCallback on_frame) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!is_open_ || !cam_handle_) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "QHYCCD Camera is not open");
    }
    if (is_streaming_.load()) {
        return core::Result<void>::ok();
    }

    frame_callback_ = std::move(on_frame);
    is_streaming_.store(true);
    is_disconnected_.store(false);
    frames_captured_ = 0;
    frames_dropped_hw_ = 0;

    worker_thread_ = std::thread(&QhyCamera::camera_worker_loop, this);
    return core::Result<void>::ok();
}

void QhyCamera::stop() {
    if (is_streaming_.exchange(false)) {
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
}

bool QhyCamera::try_reconnect() {
    const int delays_sec[] = {1, 2, 4, 8, 8};
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (!is_streaming_.load()) return false;
        SABIT_LOG_INFO("[QHYCCD Reconnect] Attempt ", attempt + 1, " after ", delays_sec[attempt], "s...");
        std::this_thread::sleep_for(std::chrono::seconds(delays_sec[attempt]));

        if (!is_streaming_.load()) return false;

        uint32_t n = p_ScanQHYCCD ? p_ScanQHYCCD() : 0;
        if (n > 0 && n != 0xFFFFFFFFU) {
            char id_buf[64] = {0};
            std::strncpy(id_buf, hw_id_.c_str(), sizeof(id_buf) - 1);
            if (id_buf[0] == '\0' && p_GetQHYCCDId) {
                p_GetQHYCCDId(0, id_buf);
            }
            cam_handle_ = p_OpenQHYCCD(id_buf);
            if (cam_handle_) {
                if (p_SetQHYCCDStreamMode) p_SetQHYCCDStreamMode(cam_handle_, 1);
                if (p_InitQHYCCD(cam_handle_) == QHYCCD_SUCCESS_CODE) {
                    execute_apply_hardware(active_settings_);
                    if (p_BeginQHYCCDLive) p_BeginQHYCCDLive(cam_handle_);
                    is_disconnected_.store(false);
                    SABIT_LOG_INFO("[QHYCCD Reconnect] Reconnected to ", id_buf);
                    return true;
                }
            }
        }
    }
    return false;
}

void QhyCamera::camera_worker_loop() {
    if (p_BeginQHYCCDLive && p_BeginQHYCCDLive(cam_handle_) != QHYCCD_SUCCESS_CODE) {
        SABIT_LOG_ERROR("BeginQHYCCDLive failed on QHYCCD camera");
        is_streaming_.store(false);
        return;
    }

    uint64_t seq = 0;
    auto last_fps_time = std::chrono::steady_clock::now();
    auto last_temp_time = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    uint64_t frames_in_second = 0;

    std::vector<uint8_t> raw_buffer;

    while (is_streaming_.load()) {
        // 1. Process CMDQ queue safely on camera thread
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

        // 2. Ensure raw buffer is large enough for GetQHYCCDLiveFrame
        size_t required_bytes = static_cast<size_t>(roi_w_) * static_cast<size_t>(roi_h_) * 4;
        if (p_GetQHYCCDMemLength) {
            uint32_t sdk_len = p_GetQHYCCDMemLength(cam_handle_);
            if (sdk_len > required_bytes && sdk_len < 512ULL * 1024ULL * 1024ULL) {
                required_bytes = sdk_len;
            }
        }
        if (raw_buffer.size() < required_bytes) {
            raw_buffer.resize(required_bytes);
        }

        uint32_t w = roi_w_;
        uint32_t h = roi_h_;
        uint32_t bpp = bpp_;
        uint32_t channels = 1;

        uint32_t ret = p_GetQHYCCDLiveFrame
            ? p_GetQHYCCDLiveFrame(cam_handle_, &w, &h, &bpp, &channels, raw_buffer.data())
            : 0xFFFFFFFFU;

        if (ret == QHYCCD_SUCCESS_CODE && w > 0 && h > 0) {
            int out_ch = (channels == 3) ? 3 : 1;
            core::Frame frame = core::Frame::create_allocated(
                static_cast<int>(w),
                static_cast<int>(h),
                out_ch,
                16
            );
            frame.seq = ++seq;
            frame.bayer = caps_.bayer;

            size_t total_samples = static_cast<size_t>(w) * static_cast<size_t>(h) * static_cast<size_t>(out_ch);
            uint16_t* dst = frame.pixels.get();

            if (bpp == 16) {
                const uint16_t* src16 = reinterpret_cast<const uint16_t*>(raw_buffer.data());
                std::memcpy(dst, src16, total_samples * sizeof(uint16_t));
            } else {
                // 8-bit buffer: upcast to 16-bit range (0..65535) for uniform pipeline processing
                for (size_t i = 0; i < total_samples; ++i) {
                    uint16_t v = static_cast<uint16_t>(raw_buffer[i]);
                    dst[i] = static_cast<uint16_t>((v << 8) | v);
                }
            }

            // Read sensor temperature & PWM every ~1 second
            auto now = std::chrono::steady_clock::now();
            if (p_GetQHYCCDParam && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_temp_time).count() >= 1000) {
                double t = p_GetQHYCCDParam(cam_handle_, QHY_CONTROL_CURTEMP);
                if (t > -100.0 && t < 100.0) {
                    sensor_temp_c_ = t;
                }
                double pwm = p_GetQHYCCDParam(cam_handle_, QHY_CONTROL_CURPWM);
                if (pwm >= 0.0 && pwm <= 255.0) {
                    cooler_power_pct_ = static_cast<int>((pwm / 255.0) * 100.0);
                }
                last_temp_time = now;
            }

            frame.sensor_temp_c = sensor_temp_c_;
            frame.exposure_us = active_settings_.exposure_us.value_or(100000);
            frame.gain = active_settings_.gain.value_or(100.0);
            frame.binning = active_settings_.binning.value_or(1);
            frame.timestamp_utc = core::Clock::now_utc();
            frame.clock_sync = core::Clock::get_sync();

            ++frames_captured_;
            ++frames_in_second;

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time).count();
            if (elapsed >= 1000) {
                actual_fps_ = (static_cast<double>(frames_in_second) * 1000.0) / elapsed;
                frames_in_second = 0;
                last_fps_time = now;
            }

            if (frame_callback_) {
                frame_callback_(frame);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    }

    if (p_StopQHYCCDLive && cam_handle_) {
        p_StopQHYCCDLive(cam_handle_);
    }
}

} // namespace sabit::camera
