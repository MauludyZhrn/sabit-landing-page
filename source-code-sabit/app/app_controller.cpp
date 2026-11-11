#include "src/app/app_controller.h"
#include "src/core/logger.h"
#include "src/imaging/io/fits_writer.h"
#include "src/imaging/io/image_writer.h"

#include <windows.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace sabit::app {

AppController::AppController() {
    imaging::StackerConfig stack_cfg;
    stack_cfg.method = imaging::StackMethod::Mean;
    stack_cfg.stack_ram_mb = 2048; // 2048 MB RAM limit
    stacker_ = std::make_unique<imaging::LiveStacker>(stack_cfg);

    pipeline_running_.store(true);
    processing_thread_ = std::thread(&AppController::processing_worker, this);
    writer_thread_ = std::thread(&AppController::writer_worker, this);

    check_disk_space();
}

AppController::~AppController() {
    stop_recording();
    end_session();

    pipeline_running_.store(false);
    frame_cv_.notify_all();
    record_ring_buffer_.shutdown();

    if (processing_thread_.joinable()) processing_thread_.join();
    if (writer_thread_.joinable()) writer_thread_.join();

    disconnect_camera();
    update_power_management(false);
}

void AppController::update_power_management(bool recording) {
#if defined(_WIN32)
    if (recording) {
        // Prevent Windows from sleeping, turning off display, or idling during observations (FR-32)
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
    } else {
        SetThreadExecutionState(ES_CONTINUOUS);
    }
#endif
}

void AppController::check_disk_space() {
    try {
        std::filesystem::space_info si = std::filesystem::space(captures_dir_);
        disk_free_bytes_.store(si.available);
        bool low = (si.available < 2ULL * 1024ULL * 1024ULL * 1024ULL); // < 2 GB (FR-18)
        disk_low_space_warning_.store(low);

        if (low && is_recording_.load()) {
            if (si.available < 500ULL * 1024ULL * 1024ULL) { // < 500 MB Emergency stop
                SABIT_LOG_ERROR("Disk free space critical (< 500 MB). Stopping recording immediately!");
                stop_recording();
                if (status_msg_callback_) {
                    status_msg_callback_("Recording stopped: Disk full (< 500 MB)!");
                }
            }
        }
    } catch (...) {}
}

bool AppController::start_session(const std::string& location, const std::string& target, const std::string& observer) {
    bool ok = session_.start(captures_dir_, location, target, camera_caps_, observer);
    if (ok) {
        std::string csv_path = session_.get_detections_csv_filepath();
        detection_logger_ = std::make_unique<detect::DetectionLogger>(csv_path);

        if (detector_.is_initialized()) {
            detector_.set_detection_callback([this](const core::Frame& f, const detect::DetectionResult& r) {
                if (detection_logger_ && !r.detections.empty()) {
                    detection_logger_->log_batch(f, r.detections);
                }
            });
        }
    }
    return ok;
}

void AppController::end_session() {
    if (is_recording_.load()) {
        stop_recording();
    }
    session_.finish();
    if (detection_logger_) {
        detection_logger_->flush();
        detection_logger_->close();
    }
}

bool AppController::is_session_active() const {
    return session_.is_active();
}

SessionInfo AppController::session_info() const {
    return session_.info();
}

void AppController::log_session_event(const std::string& event) {
    if (session_.is_active()) {
        session_.log_event(event);
    }
}

std::vector<camera::CameraId> AppController::enumerate_cameras(bool force_rescan) {
    std::lock_guard<std::mutex> lk(enum_mutex_);
    if (!force_rescan && !cached_cameras_.empty()) {
        return cached_cameras_;
    }

    std::vector<camera::CameraId> list;

    // 1. UVC / DirectShow Driver Cameras (Webcam & Capture Cards)
    auto uvc_list = camera::UvcCamera::enumerate_cameras();
    list.insert(list.end(), uvc_list.begin(), uvc_list.end());

    // 2. ZWO ASI Native SDK Cameras
    auto zwo_list = camera::ZwoCamera::enumerate_cameras();
    if (zwo_list.empty()) {
        list.push_back(camera::CameraId{.driver = "zwo", .id = "auto", .name = "Connect ZWO ASI (Native SDK)"});
    } else {
        list.insert(list.end(), zwo_list.begin(), zwo_list.end());
    }

    // 3. QHYCCD Native SDK Cameras (avoid re-scanning USB endpoints while a QHY camera is actively streaming)
    if (camera_connected_.load() && camera_caps_.driver == "qhy") {
        list.push_back(camera::CameraId{
            .driver = "qhy",
            .id = camera_caps_.serial.empty() ? "auto" : camera_caps_.serial,
            .name = camera_caps_.model.empty() ? "Connect QHYCCD (Native SDK)" : camera_caps_.model
        });
    } else {
        auto qhy_list = camera::QhyCamera::enumerate_cameras();
        if (qhy_list.empty()) {
            list.push_back(camera::CameraId{.driver = "qhy", .id = "auto", .name = "Connect QHYCCD (Native SDK)"});
        } else {
            list.insert(list.end(), qhy_list.begin(), qhy_list.end());
        }
    }

    // 4. ASCOM Camera COM Drivers
    auto ascom_list = camera::AscomCamera::enumerate_cameras(true);
    list.insert(list.end(), ascom_list.begin(), ascom_list.end());

#if defined(_WIN32)
    // 4b. Scan Windows USB Registry History for astronomy & DSLR cameras previously connected to this PC
    // (ZWO VID_03C3, QHYCCD VID_1618, Canon VID_04A9, Nikon VID_04B0, Sony VID_054C, SVBONY VID_F266, Touptek VID_0547)
    HKEY h_usb = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Enum\\USB", 0, KEY_READ, &h_usb) == ERROR_SUCCESS) {
        wchar_t vid_key[256];
        DWORD idx = 0;
        while (RegEnumKeyW(h_usb, idx++, vid_key, 256) == ERROR_SUCCESS) {
            std::wstring wvid(vid_key);
            for (auto& ch : wvid) ch = static_cast<wchar_t>(towupper(ch));
            std::string mapped_driver;
            if (wvid.find(L"VID_03C3") != std::wstring::npos) mapped_driver = "zwo";
            else if (wvid.find(L"VID_1618") != std::wstring::npos) mapped_driver = "qhy";
            else if (wvid.find(L"VID_04A9") != std::wstring::npos || wvid.find(L"VID_04B0") != std::wstring::npos) mapped_driver = "ascom_dslr_usb";
            if (mapped_driver.empty()) continue;

            HKEY h_dev = nullptr;
            if (RegOpenKeyExW(h_usb, vid_key, 0, KEY_READ, &h_dev) == ERROR_SUCCESS) {
                wchar_t inst_key[256];
                DWORD inst_idx = 0;
                while (RegEnumKeyW(h_dev, inst_idx++, inst_key, 256) == ERROR_SUCCESS) {
                    HKEY h_inst = nullptr;
                    if (RegOpenKeyExW(h_dev, inst_key, 0, KEY_READ, &h_inst) == ERROR_SUCCESS) {
                        wchar_t fname[512] = {0};
                        DWORD fname_sz = sizeof(fname);
                        DWORD type = 0;
                        bool ok = (RegQueryValueExW(h_inst, L"FriendlyName", nullptr, &type, reinterpret_cast<LPBYTE>(fname), &fname_sz) == ERROR_SUCCESS && fname[0] != L'\0');
                        if (!ok) {
                            fname_sz = sizeof(fname);
                            ok = (RegQueryValueExW(h_inst, L"DeviceDesc", nullptr, &type, reinterpret_cast<LPBYTE>(fname), &fname_sz) == ERROR_SUCCESS && fname[0] != L'\0');
                        }
                        if (ok) {
                            int len = WideCharToMultiByte(CP_UTF8, 0, fname, -1, nullptr, 0, nullptr, nullptr);
                            std::string name_utf8;
                            if (len > 1) {
                                name_utf8.resize(len - 1);
                                WideCharToMultiByte(CP_UTF8, 0, fname, -1, name_utf8.data(), len, nullptr, nullptr);
                            }
                            auto semi = name_utf8.find_last_of(';');
                            if (semi != std::string::npos && semi + 1 < name_utf8.size()) {
                                name_utf8 = name_utf8.substr(semi + 1);
                            }
                            if (!name_utf8.empty() && name_utf8.find("Composite") == std::string::npos) {
                                std::string display_name = (mapped_driver == "zwo" || mapped_driver == "qhy")
                                    ? name_utf8
                                    : (name_utf8 + " (USB / DSLR)");
                                bool dup = false;
                                for (const auto& existing : list) {
                                    if (existing.name == display_name || existing.name == name_utf8) { dup = true; break; }
                                }
                                if (!dup) {
                                    if (mapped_driver == "zwo") {
                                        list.push_back(camera::CameraId{.driver = "zwo", .id = "auto", .name = display_name});
                                    } else if (mapped_driver == "qhy") {
                                        list.push_back(camera::CameraId{.driver = "qhy", .id = "auto", .name = display_name});
                                    } else {
                                        list.push_back(camera::CameraId{.driver = "ascom", .id = "ASCOM.DSLR.Camera", .name = display_name});
                                    }
                                }
                            }
                        }
                        RegCloseKey(h_inst);
                    }
                }
                RegCloseKey(h_dev);
            }
        }
        RegCloseKey(h_usb);
    }
#endif

    // 5. Simulator & Test Cameras (SharpCap-compatible Test Cameras + Hilal Simulator)
    list.push_back(camera::CameraId{.driver = "sim", .id = "starfield", .name = "Test Camera 1 (Deep Sky)"});
    list.push_back(camera::CameraId{.driver = "sim", .id = "gradient", .name = "Test Camera 2 (High Speed)"});
    list.push_back(camera::CameraId{.driver = "sim", .id = "hilal", .name = "Simulator: Hilal Dusk"});

    cached_cameras_ = list;
    return cached_cameras_;
}

core::Result<camera::CameraCaps> AppController::connect_camera(const camera::CameraId& id) {
    disconnect_camera();
    last_connected_camera_ = id;

    if (id.driver == "sim") {
        camera::SimulatorMode mode = camera::SimulatorMode::Hilal;
        if (id.id == "gradient") mode = camera::SimulatorMode::Gradient;
        else if (id.id == "starfield") mode = camera::SimulatorMode::StarField;
        camera_ = std::make_unique<camera::SimulatorCamera>(mode, 1280, 720);
    } else if (id.driver == "zwo") {
        auto zwo = std::make_unique<camera::ZwoCamera>();
        zwo->set_disconnected_callback([this]() {
            SABIT_LOG_WARN("ZWO Camera disconnected unexpectedly!");
            camera_connected_.store(false);
            session_.log_event("WARNING: ZWO Camera hardware disconnected!");
            if (status_msg_callback_) status_msg_callback_("ZWO Camera disconnected!");
        });
        camera_ = std::move(zwo);
    } else if (id.driver == "qhy") {
        auto qhy = std::make_unique<camera::QhyCamera>();
        qhy->set_disconnected_callback([this]() {
            SABIT_LOG_WARN("QHYCCD Camera disconnected unexpectedly!");
            camera_connected_.store(false);
            session_.log_event("WARNING: QHYCCD Camera hardware disconnected!");
            if (status_msg_callback_) status_msg_callback_("QHYCCD Camera disconnected!");
        });
        camera_ = std::move(qhy);
    } else if (id.driver == "ascom") {
        auto ascom = std::make_unique<camera::AscomCamera>();
        ascom->set_disconnected_callback([this]() {
            SABIT_LOG_WARN("ASCOM Camera disconnected unexpectedly!");
            camera_connected_.store(false);
            session_.log_event("WARNING: ASCOM Camera disconnected!");
            if (status_msg_callback_) status_msg_callback_("ASCOM Camera disconnected!");
        });
        camera_ = std::move(ascom);
    } else if (id.driver == "uvc") {
        auto uvc = std::make_unique<camera::UvcCamera>();
        uvc->set_disconnected_callback([this]() {
            SABIT_LOG_WARN("UVC Camera disconnected unexpectedly!");
            camera_connected_.store(false);
            session_.log_event("WARNING: UVC Camera disconnected!");
            if (status_msg_callback_) status_msg_callback_("UVC Camera disconnected!");
        });
        camera_ = std::move(uvc);
    } else {
        return core::Result<camera::CameraCaps>::err(core::Error(core::ErrorCode::Unsupported, "Unknown camera driver: " + id.driver));
    }

    auto res = camera_->open(id);
    if (!res.is_ok()) {
        camera_.reset();
        return res;
    }

    camera_caps_ = res.value();
    camera_connected_.store(true);

    auto start_res = camera_->start([this](const core::Frame& frame) {
        on_camera_frame(frame);
    });

    if (!start_res.is_ok()) {
        camera_->close();
        camera_.reset();
        camera_connected_.store(false);
        return core::Result<camera::CameraCaps>::err(start_res.error());
    }

    SABIT_LOG_INFO("Camera connected: ", camera_caps_.model, " (", camera_caps_.driver, ")");
    return core::Result<camera::CameraCaps>::ok(camera_caps_);
}

void AppController::disconnect_camera() {
    if (camera_) {
        camera_->stop();
        camera_->close();
        camera_.reset();
    }
    camera_connected_.store(false);
    camera_caps_ = {};
    measured_fps_.store(0.0);
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        next_process_frame_.reset();
    }
    preview_slot_.clear();
}

std::optional<camera::CameraId> AppController::last_connected_camera() const {
    return last_connected_camera_;
}

core::Result<camera::CameraSettings> AppController::apply_camera_settings(const camera::CameraSettings& settings) {
    if (!camera_) {
        return core::Result<camera::CameraSettings>::err(core::Error(core::ErrorCode::InvalidState, "No camera connected"));
    }
    return camera_->apply(settings);
}

camera::CameraCaps AppController::camera_caps() const {
    return camera_caps_;
}

camera::CameraStatus AppController::camera_status() const {
    if (camera_) {
        return camera_->status();
    }
    return camera::CameraStatus{};
}

void AppController::set_master_dark(imaging::MasterDark dark) {
    calibration_.set_master_dark(std::move(dark));
    session_.log_event("Master Dark loaded.");
}

void AppController::set_master_flat(imaging::MasterFlat flat) {
    calibration_.set_master_flat(std::move(flat));
    session_.log_event("Master Flat loaded.");
}

bool AppController::load_dark_file(const std::string& path) {
    auto res = imaging::io::FitsWriter::read_fits_16bit(path);
    if (!res.is_ok()) {
        res = imaging::io::ImageWriter::read_tiff_16bit(path);
    }
    if (!res.is_ok()) return false;
    auto dark_res = imaging::CalibrationProcessor::create_master_dark({res.value()});
    if (!dark_res.is_ok()) return false;
    set_master_dark(std::move(dark_res.value()));
    enable_dark_subtraction(true);
    return true;
}

bool AppController::load_flat_file(const std::string& path) {
    auto res = imaging::io::FitsWriter::read_fits_16bit(path);
    if (!res.is_ok()) {
        res = imaging::io::ImageWriter::read_tiff_16bit(path);
    }
    if (!res.is_ok()) return false;
    const imaging::MasterDark* dark_ptr = calibration_.master_dark().is_valid() ? &calibration_.master_dark() : nullptr;
    auto flat_res = imaging::CalibrationProcessor::create_master_flat({res.value()}, dark_ptr);
    if (!flat_res.is_ok()) return false;
    set_master_flat(std::move(flat_res.value()));
    enable_flat_correction(true);
    return true;
}

void AppController::enable_dark_subtraction(bool enable) {
    calibration_.enable_dark(enable);
}

void AppController::enable_flat_correction(bool enable) {
    calibration_.enable_flat(enable);
}

bool AppController::has_master_dark() const {
    return calibration_.master_dark().is_valid();
}

bool AppController::has_master_flat() const {
    return calibration_.master_flat().is_valid();
}

void AppController::clear_master_dark() {
    calibration_.clear_dark();
    session_.log_event("Master Dark cleared.");
}

void AppController::clear_master_flat() {
    calibration_.clear_flat();
    session_.log_event("Master Flat cleared.");
}

bool AppController::export_diagnostic(const std::string& zip_dest_path, bool mask_coordinates) {
    return session_.export_diagnostic_zip(zip_dest_path, mask_coordinates);
}

void AppController::enable_stacking(bool enable) {
    stacking_enabled_.store(enable);
    if (enable && stacker_) {
        stacker_->reset();
    }
}

void AppController::enable_stack_alignment(bool enable) {
    if (stacker_) {
        auto cfg = stacker_->config();
        cfg.align_enabled = enable;
        stacker_->set_config(cfg);
    }
}

void AppController::set_stack_method(imaging::StackMethod method) {
    if (stacker_) {
        auto cfg = stacker_->config();
        cfg.method = method;
        stacker_->set_config(cfg);
    }
}

void AppController::reset_stacker() {
    if (stacker_) {
        stacker_->reset();
    }
}

void AppController::set_stretch_enabled(bool enable) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.stretch_enabled = enable;
    stretcher_.set_config(cfg);
}

void AppController::set_auto_stretch(bool enable) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.auto_stretch = enable;
    stretcher_.set_config(cfg);
}

void AppController::set_manual_stretch(double black_point, double white_point) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.auto_stretch = false;
    cfg.black_point = static_cast<float>(black_point);
    cfg.white_point = static_cast<float>(white_point);
    stretcher_.set_config(cfg);
}

void AppController::set_manual_stretch_full(double black_point, double white_point, double gamma) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.auto_stretch = false;
    cfg.black_point = static_cast<float>(black_point);
    cfg.white_point = static_cast<float>(white_point);
    cfg.gamma = static_cast<float>(gamma);
    stretcher_.set_config(cfg);
}

void AppController::set_gamma(double gamma) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.gamma = static_cast<float>(gamma);
    stretcher_.set_config(cfg);
}

void AppController::set_exp_gain_shift(double shift_ev) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.exp_gain_shift = static_cast<float>(shift_ev);
    stretcher_.set_config(cfg);
}

void AppController::set_fps_limit(double max_fps) {
    max_fps_limit_.store(max_fps);
}

void AppController::enable_clahe(bool enable) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.clahe_enabled = enable;
    stretcher_.set_config(cfg);
}

void AppController::enable_unsharp_mask(bool enable) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.sharpen_enabled = enable;
    stretcher_.set_config(cfg);
}

void AppController::enable_invert(bool enable) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.invert_enabled = enable;
    stretcher_.set_config(cfg);
}

void AppController::set_timestamp_frames(bool enable) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.timestamp_frames = enable;
    stretcher_.set_config(cfg);
}

void AppController::set_banding_suppression(int level) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.banding_suppression = level;
    stretcher_.set_config(cfg);
}

void AppController::set_planet_stabilization(bool enable) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.planet_stabilization = enable;
    stretcher_.set_config(cfg);
}

void AppController::set_background_subtraction(int mode) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.background_subtraction = mode;
    stretcher_.set_config(cfg);
}

void AppController::set_remove_satellite_trails(bool enable) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.remove_satellite_trails = enable;
    stretcher_.set_config(cfg);
}

void AppController::set_flip_mode(int mode) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.flip_mode = mode;
    stretcher_.set_config(cfg);
}

void AppController::set_mask_over_exposed(bool enable) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.mask_over_exposed = enable;
    stretcher_.set_config(cfg);
}

void AppController::set_ca_reduction(bool enable) {
    std::lock_guard<std::mutex> lock(stretcher_mutex_);
    auto cfg = stretcher_.config();
    cfg.ca_reduction = enable;
    stretcher_.set_config(cfg);
}

core::Result<void> AppController::init_ai(const std::string& model_path, const std::string& manifest_path) {
    detect::OnnxDetectorConfig cfg;
    cfg.model_path = model_path;
    cfg.manifest_path = manifest_path;
    cfg.conf_threshold = 0.25f;
    cfg.nms_threshold = 0.45f;
    cfg.num_threads = 2;
    cfg.max_fps = 3.0; // 3 FPS rate-limiter

    auto res = detector_.init(cfg);
    if (res.is_ok()) {
        detector_.start_worker();
    }
    return res;
}

void AppController::enable_ai(bool enable) {
    ai_enabled_.store(enable);
    session_.log_event(enable ? "AI Detection enabled." : "AI Detection disabled.");
}

bool AppController::is_ai_enabled() const {
    return ai_enabled_.load();
}

void AppController::set_ai_conf_threshold(float conf) {
    detector_.set_conf_threshold(conf);
}

void AppController::set_ai_nms_threshold(float nms) {
    detector_.set_nms_threshold(nms);
}

std::vector<detect::Detection> AppController::get_latest_overlay(int64_t max_age_ms) const {
    if (!ai_enabled_.load()) return {};
    return detector_.get_latest_overlay(max_age_ms);
}

void AppController::set_captures_dir(const std::string& dir) {
    if (!dir.empty()) {
        captures_dir_ = dir;
    }
}

std::string AppController::captures_dir() const {
    return captures_dir_;
}

void AppController::set_active_target(const std::string& target) {
    active_target_ = target.empty() ? "Hilal" : target;
}

std::string AppController::active_target() const {
    return active_target_.empty() ? "Hilal" : active_target_;
}

bool AppController::start_recording(const std::string& format, int count, const std::string& target_override) {
    std::string target = target_override.empty() ? active_target() : target_override;
    if (target.empty()) target = "Hilal";

    // Auto-switch / restart session if session is inactive or if target changed
    if (!session_.is_active() || session_.info().target != target) {
        if (session_.is_active()) {
            end_session();
        }
        start_session(site_.name, target, "Sabit Observer");
    }

    // Drain any stale queued frames before starting a new recording/snapshot
    while (record_ring_buffer_.pop(std::chrono::milliseconds(0)).has_value()) {}

    record_format_ = format;
    target_record_count_.store(count);
    frames_recorded_.store(0);
    is_recording_.store(true);
    update_power_management(true);

    last_manifest_write_time_ = std::chrono::steady_clock::now();
    session_.log_event("Recording started: target=" + target + ", format=" + format + ", target_count=" + std::to_string(count));

    return true;
}

void AppController::stop_recording() {
    if (is_recording_.exchange(false)) {
        update_power_management(false);
        {
            std::lock_guard<std::mutex> lk(avi_mutex_);
            if (avi_writer_ && avi_writer_->is_open()) {
                avi_writer_->close();
                avi_writer_.reset();
            }
            if (mp4_writer_ && mp4_writer_->is_open()) {
                mp4_writer_->close();
                mp4_writer_.reset();
            }
        }
        session_.log_event("Recording stopped. Total frames: " + std::to_string(frames_recorded_.load()));
        session_.write_manifest();
    }
}

bool AppController::is_recording() const {
    return is_recording_.load();
}

void AppController::on_camera_frame(const core::Frame& frame) {
    if (!camera_connected_.load() || !frame.is_valid()) return;

    const double fps_lim = max_fps_limit_.load();
    if (fps_lim > 0.1) {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(fps_mutex_);
        if (last_frame_accept_time_.time_since_epoch().count() != 0) {
            double min_interval_ms = 1000.0 / fps_lim;
            double elapsed_ms = std::chrono::duration<double, std::milli>(now - last_frame_accept_time_).count();
            if (elapsed_ms < min_interval_ms * 0.92) {
                return;
            }
        }
        last_frame_accept_time_ = now;
    }

    ++frames_captured_;

    {
        std::lock_guard<std::mutex> lock(fps_mutex_);
        ++fps_counter_;
        auto now = std::chrono::steady_clock::now();
        if (last_fps_time_.time_since_epoch().count() == 0) {
            last_fps_time_ = now;
        } else {
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time_).count();
            if (elapsed_ms >= 500) {
                measured_fps_.store((static_cast<double>(fps_counter_) * 1000.0) / elapsed_ms);
                fps_counter_ = 0;
                last_fps_time_ = now;
            }
        }
    }

    // Put into incoming slot for processing thread
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        next_process_frame_ = frame;
    }
    frame_cv_.notify_one();
}

void AppController::processing_worker() {
    while (pipeline_running_.load()) {
        core::Frame frame;
        {
            std::unique_lock<std::mutex> lock(frame_mutex_);
            frame_cv_.wait(lock, [this]() {
                return !pipeline_running_.load() || next_process_frame_.has_value();
            });

            if (!pipeline_running_.load()) break;

            if (next_process_frame_.has_value()) {
                frame = std::move(*next_process_frame_);
                next_process_frame_.reset();
            }
        }

        if (!frame.is_valid()) continue;

        // 1. Submit to AI Thread if AI active (independent copy, non-destructive)
        if (ai_enabled_.load() && detector_.is_initialized()) {
            detector_.submit_frame(frame);
        }

        // 3. Calibration pipeline (for preview and stacking)
        // Make an independent working frame for preview processing so raw frame is preserved
        core::Frame proc_frame = core::Frame::create_allocated(frame.width, frame.height, frame.channels, frame.bit_depth);
        proc_frame.seq = frame.seq;
        proc_frame.exposure_us = frame.exposure_us;
        proc_frame.gain = frame.gain;
        proc_frame.sensor_temp_c = frame.sensor_temp_c;
        proc_frame.timestamp_utc = frame.timestamp_utc;
        std::memcpy(proc_frame.pixels.get(), frame.pixels.get(), frame.byte_size());

        calibration_.apply(proc_frame);

        // 4. Stacking pipeline
        if (stacking_enabled_.load() && stacker_) {
            proc_frame = stacker_->process_frame(proc_frame);
        }

        // 5. Stretch to 8-bit preview using ImageStretcher
        PreviewData preview{};
        preview.width = proc_frame.width;
        preview.height = proc_frame.height;
        preview.seq = frame.seq;
        preview.exposure_us = frame.exposure_us;
        preview.gain = frame.gain;
        preview.sensor_temp_c = frame.sensor_temp_c;
        preview.actual_fps = measured_fps_.load();
        preview.timestamp_utc_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            frame.timestamp_utc.time_since_epoch()).count();

        imaging::Histogram hist{};
        {
            std::lock_guard<std::mutex> lock(stretcher_mutex_);
            auto stretched8 = stretcher_.convert_to_preview_8bit(proc_frame, &hist, &proc_frame);
            preview.stretch_black_pt = stretcher_.effective_black_norm();
            preview.stretch_white_pt = stretcher_.effective_white_norm();
            preview.stretch_gamma = stretcher_.effective_gamma();
            preview.stretch_enabled = stretcher_.config().stretch_enabled;
            preview.timestamp_frames = stretcher_.config().timestamp_frames;

            if (is_recording_.load()) {
                if (record_format_ == "png" || record_format_ == "jpg" || record_format_ == "jpeg" ||
                    record_format_ == "avi" || record_format_ == "mp4") {
                    core::Frame vis_frame = core::Frame::create_allocated(
                        proc_frame.width, proc_frame.height, proc_frame.channels, 16);
                    vis_frame.seq = proc_frame.seq;
                    vis_frame.exposure_us = proc_frame.exposure_us;
                    vis_frame.gain = proc_frame.gain;
                    vis_frame.sensor_temp_c = proc_frame.sensor_temp_c;
                    vis_frame.timestamp_utc = proc_frame.timestamp_utc;
                    uint16_t* vdst = vis_frame.pixels.get();
                    const size_t n = std::min(vis_frame.pixel_count(), stretched8.size());
                    for (size_t i = 0; i < n; ++i) {
                        uint16_t v = stretched8[i];
                        vdst[i] = static_cast<uint16_t>((v << 8) | v);
                    }
                    record_ring_buffer_.push(vis_frame);
                } else {
                    record_ring_buffer_.push(proc_frame);
                }
            }

            if (proc_frame.channels >= 3) {
                // Already 3-channel RGB — move directly without 3x grayscale replication!
                preview.rgb_pixels = std::move(stretched8);
            } else {
                // Replicate 8-bit grayscale to 3-channel RGB for display
                preview.rgb_pixels.resize(stretched8.size() * 3);
                for (size_t i = 0; i < stretched8.size(); ++i) {
                    uint8_t val = stretched8[i];
                    preview.rgb_pixels[i * 3 + 0] = val;
                    preview.rgb_pixels[i * 3 + 1] = val;
                    preview.rgb_pixels[i * 3 + 2] = val;
                }
            }
        }

        preview.histogram_256.assign(hist.bins256.begin(), hist.bins256.end());
        preview.histogram_r.assign(hist.bins256_r.begin(), hist.bins256_r.end());
        preview.histogram_g.assign(hist.bins256_g.begin(), hist.bins256_g.end());
        preview.histogram_b.assign(hist.bins256_b.begin(), hist.bins256_b.end());

        if (camera_) {
            preview.actual_fps = camera_->status().actual_fps;
        }

        preview_slot_.put(std::move(preview));
    }
}

void AppController::writer_worker() {
    auto last_speed_check = std::chrono::steady_clock::now();
    uint64_t bytes_written_period = 0;

    while (pipeline_running_.load()) {
        auto opt_frame = record_ring_buffer_.pop(std::chrono::milliseconds(50));
        if (!opt_frame.has_value()) {
            continue;
        }

        const auto& frame = *opt_frame;

        if (is_recording_.load()) {
            std::string ext = "fits";
            if (record_format_ == "tiff" || record_format_ == "tif") ext = "tif";
            else if (record_format_ == "png") ext = "png";
            else if (record_format_ == "jpg" || record_format_ == "jpeg") ext = "jpg";
            else if (record_format_ == "avi") ext = "avi";
            else if (record_format_ == "mp4") ext = "mp4";
            std::string filepath = session_.get_frame_filepath(ext, frame.seq);

            imaging::io::FitsMetadata meta;
            meta.object = session_.info().target;
            meta.observer = session_.info().observer;
            meta.instrument = camera_caps_.model.empty() ? session_.info().camera_model : camera_caps_.model;
            meta.site_latitude = site_.latitude;
            meta.site_longitude = site_.longitude;
            meta.site_elevation_m = site_.elevation_m;

            core::Result<void> res = core::Result<void>::ok();
            if (record_format_ == "avi") {
                std::lock_guard<std::mutex> lk(avi_mutex_);
                if (!avi_writer_) {
                    avi_writer_ = std::make_unique<imaging::io::AviWriter>();
                    std::string avi_path = session_.get_frame_filepath("avi", frame.seq);
                    double fps = measured_fps_.load();
                    if (fps <= 1.0) fps = 30.0;
                    res = avi_writer_->open(avi_path, frame.width, frame.height, fps);
                }
                if (res.is_ok() && avi_writer_ && avi_writer_->is_open()) {
                    res = avi_writer_->write_frame(frame);
                }
            } else if (record_format_ == "mp4") {
                std::lock_guard<std::mutex> lk(avi_mutex_);
                if (!mp4_writer_) {
                    mp4_writer_ = std::make_unique<imaging::io::Mp4Writer>();
                    std::string mp4_path = session_.get_frame_filepath("mp4", frame.seq);
                    double fps = measured_fps_.load();
                    if (fps <= 1.0) fps = 30.0;
                    res = mp4_writer_->open(mp4_path, frame.width, frame.height, fps);
                }
                if (res.is_ok() && mp4_writer_ && mp4_writer_->is_open()) {
                    res = mp4_writer_->write_frame(frame);
                }
            } else if (record_format_ == "tiff" || record_format_ == "tif") {
                res = imaging::io::ImageWriter::write_tiff_16bit(filepath, frame);
            } else if (record_format_ == "png") {
                res = imaging::io::ImageWriter::write_png(filepath, frame);
            } else if (record_format_ == "jpg" || record_format_ == "jpeg") {
                res = imaging::io::ImageWriter::write_jpg(filepath, frame, 95);
            } else {
                res = imaging::io::FitsWriter::write_fits_16bit(filepath, frame, meta);
            }

            if (res.is_ok()) {
                ++frames_recorded_;
                bytes_written_period += frame.byte_size();

                session_.update_counts(
                    frames_captured_.load(),
                    frames_recorded_.load(),
                    detection_logger_ ? detection_logger_->logged_count() : 0
                );
            }

            // Periodic manifest write every 5 seconds (FR-17)
            auto now = std::chrono::steady_clock::now();
            auto manifest_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_manifest_write_time_).count();
            if (manifest_elapsed >= 5) {
                session_.write_manifest();
                last_manifest_write_time_ = now;
                check_disk_space();
            }

            // Calculate write speed
            auto speed_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_speed_check).count();
            if (speed_elapsed >= 1000) {
                disk_write_mb_s_.store((static_cast<double>(bytes_written_period) / 1024.0 / 1024.0) / (speed_elapsed / 1000.0));
                bytes_written_period = 0;
                last_speed_check = now;
            }

            // Target frame limit check
            int target = target_record_count_.load();
            if (target > 0 && static_cast<int>(frames_recorded_.load()) >= target) {
                stop_recording();
            }
        }
    }
}

std::optional<PreviewData> AppController::get_latest_preview() {
    if (!camera_connected_.load()) {
        return std::nullopt;
    }
    return preview_slot_.get_latest();
}

SystemStatus AppController::get_system_status() {
    SystemStatus st{};
    bool conn = camera_connected_.load();
    if (camera_) {
        auto cam_st = camera_->status();
        if (cam_st.is_connected || cam_st.is_streaming) conn = true;
        st.actual_fps = (cam_st.actual_fps > 0.1) ? cam_st.actual_fps : measured_fps_.load();
        st.frames_dropped = cam_st.frames_dropped_hw + record_ring_buffer_.dropped_count();
        st.camera_name = camera_caps_.model;
        if (st.camera_name.empty()) {
            st.camera_name = (camera_caps_.driver == "sim") ? "SIMULATOR" : "KAMERA";
        }
    } else {
        st.actual_fps = 0.0;
        st.camera_name = "";
    }
    st.camera_connected = conn;
    st.frames_captured = frames_captured_.load();
    st.frames_recorded = frames_recorded_.load();
    st.disk_write_mb_s = disk_write_mb_s_.load();
    st.disk_free_bytes = disk_free_bytes_.load();
    st.disk_low_space_warning = disk_low_space_warning_.load();
    st.is_recording = is_recording_.load();
    st.is_ai_active = ai_enabled_.load();
    st.active_session_name = session_.info().folder_name;

    return st;
}

void AppController::set_observer_site(const astro::Site& site) {
    site_ = site;
}

astro::Site AppController::observer_site() const {
    return site_;
}

astro::MoonPhenomena AppController::calculate_ephemeris(const core::Clock::TimePoint& tp) const {
    return astro::EphemerisCalculator::compute_phenomena(site_, tp);
}

astro::EphemerisTimes AppController::calculate_ephemeris_times(int year, int month, int day) const {
    return astro::EphemerisCalculator::compute_times(site_, year, month, day);
}

astro::EphemerisTimes AppController::calculate_ephemeris_times(const core::Clock::TimePoint& tp) const {
    return astro::EphemerisCalculator::compute_times(site_, tp);
}

} // namespace sabit::app
