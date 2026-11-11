#ifndef SABIT_APP_APP_CONTROLLER_H
#define SABIT_APP_APP_CONTROLLER_H

#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>
#include <optional>

#include "src/core/frame.h"
#include "src/core/result.h"
#include "src/core/ring_buffer.h"
#include "src/core/latest_slot.h"
#include "src/camera/icamera.h"
#include "src/camera/sim/simulator_camera.h"
#include "src/camera/zwo/zwo_camera.h"
#include "src/camera/qhy/qhy_camera.h"
#include "src/camera/ascom/ascom_camera.h"
#include "src/camera/uvc/uvc_camera.h"
#include "src/imaging/calibration.h"
#include "src/imaging/stacker.h"
#include "src/imaging/stretch.h"
#include "src/imaging/io/avi_writer.h"
#include "src/imaging/io/mp4_writer.h"
#include "src/detect/onnx_detector.h"
#include "src/detect/detection_log.h"
#include "src/astro/site.h"
#include "src/astro/ephemeris.h"
#include "src/astro/criteria.h"
#include "src/app/session.h"

namespace sabit::app {

struct PreviewData {
    int width{0};
    int height{0};
    std::vector<uint8_t> rgb_pixels; // 8-bit RGB preview
    std::vector<uint32_t> histogram_256;
    std::vector<uint32_t> histogram_r;
    std::vector<uint32_t> histogram_g;
    std::vector<uint32_t> histogram_b;
    uint64_t seq{0};
    int64_t exposure_us{0};
    double gain{0.0};
    double sensor_temp_c{0.0};
    double actual_fps{0.0};
    int64_t timestamp_utc_ms{0};
    float stretch_black_pt{0.0f};
    float stretch_white_pt{1.0f};
    float stretch_gamma{1.0f};
    bool stretch_enabled{true};
    bool timestamp_frames{false};
};

struct SystemStatus {
    bool camera_connected{false};
    std::string camera_name;
    double actual_fps{0.0};
    uint64_t frames_captured{0};
    uint64_t frames_recorded{0};
    uint64_t frames_dropped{0};
    double disk_write_mb_s{0.0};
    uint64_t disk_free_bytes{0};
    bool disk_low_space_warning{false}; // < 2 GB
    bool is_recording{false};
    bool is_ai_active{false};
    std::string active_session_name;
};

class AppController {
public:
    AppController();
    ~AppController();

    // Sesi Pengamatan
    bool start_session(const std::string& location, const std::string& target, const std::string& observer = "Sabit Observer");
    void end_session();
    [[nodiscard]] bool is_session_active() const;
    [[nodiscard]] SessionInfo session_info() const;
    void log_session_event(const std::string& event);
    Session& session() { return session_; }
    [[nodiscard]] const Session& session() const { return session_; }

    // Kamera Hardware & Simulator
    std::vector<camera::CameraId> enumerate_cameras(bool force_rescan = false);
    core::Result<camera::CameraCaps> connect_camera(const camera::CameraId& id);
    void disconnect_camera();
    [[nodiscard]] std::optional<camera::CameraId> last_connected_camera() const;
    core::Result<camera::CameraSettings> apply_camera_settings(const camera::CameraSettings& settings);
    [[nodiscard]] camera::CameraCaps camera_caps() const;
    [[nodiscard]] camera::CameraStatus camera_status() const;

    // Pemrosesan & Kalibrasi
    void set_master_dark(imaging::MasterDark dark);
    void set_master_flat(imaging::MasterFlat flat);
    bool load_dark_file(const std::string& path);
    bool load_flat_file(const std::string& path);
    void enable_dark_subtraction(bool enable);
    void enable_flat_correction(bool enable);
    [[nodiscard]] bool has_master_dark() const;
    [[nodiscard]] bool has_master_flat() const;
    void clear_master_dark();
    void clear_master_flat();

    // Diagnostik & Ekspor
    bool export_diagnostic(const std::string& zip_dest_path, bool mask_coordinates = false);

    // Stacking
    void enable_stacking(bool enable);
    void enable_stack_alignment(bool enable);
    void set_stack_method(imaging::StackMethod method);
    void reset_stacker();

    // Stretch & Visual Peningkatan
    void set_stretch_enabled(bool enable);
    void set_auto_stretch(bool enable);
    void set_manual_stretch(double black_point, double white_point);
    void set_manual_stretch_full(double black_point, double white_point, double gamma);
    void set_gamma(double gamma);
    void set_exp_gain_shift(double shift_ev);
    void set_fps_limit(double max_fps);
    void enable_clahe(bool enable);
    void enable_unsharp_mask(bool enable);
    void enable_invert(bool enable);

    // Image Controls & Preprocessing (SharpCap-style)
    void set_timestamp_frames(bool enable);
    void set_banding_suppression(int level);
    void set_planet_stabilization(bool enable);
    void set_background_subtraction(int mode);
    void set_remove_satellite_trails(bool enable);
    void set_flip_mode(int mode);
    void set_mask_over_exposed(bool enable);
    void set_ca_reduction(bool enable);

    // AI Deteksi
    core::Result<void> init_ai(const std::string& model_path = "models/hilal_v1.onnx", const std::string& manifest_path = "models/manifest.json");
    void enable_ai(bool enable);
    [[nodiscard]] bool is_ai_enabled() const;
    void set_ai_conf_threshold(float conf);
    void set_ai_nms_threshold(float nms);
    std::vector<detect::Detection> get_latest_overlay(int64_t max_age_ms = 500) const;

    // Perekaman Berkas & Folder Tangkapan
    void set_captures_dir(const std::string& dir);
    [[nodiscard]] std::string captures_dir() const;
    void set_active_target(const std::string& target);
    [[nodiscard]] std::string active_target() const;
    bool start_recording(const std::string& format = "fits", int count = -1, const std::string& target_override = ""); // -1 = continuous
    void stop_recording();
    [[nodiscard]] bool is_recording() const;

    // Preview UI & Status
    std::optional<PreviewData> get_latest_preview();
    SystemStatus get_system_status();

    // Astronomi & Falak
    void set_observer_site(const astro::Site& site);
    [[nodiscard]] astro::Site observer_site() const;
    [[nodiscard]] astro::MoonPhenomena calculate_ephemeris(const core::Clock::TimePoint& tp = core::Clock::now_utc()) const;
    [[nodiscard]] astro::EphemerisTimes calculate_ephemeris_times(int year, int month, int day) const;
    [[nodiscard]] astro::EphemerisTimes calculate_ephemeris_times(const core::Clock::TimePoint& tp = core::Clock::now_utc()) const;

    // Event callbacks
    void set_status_message_callback(std::function<void(const std::string&)> cb) {
        status_msg_callback_ = std::move(cb);
    }

private:
    void on_camera_frame(const core::Frame& frame);
    void processing_worker();
    void writer_worker();
    void update_power_management(bool recording);
    void check_disk_space();

    // Sesi
    Session session_;
    std::string captures_dir_{"Captures"};
    std::string active_target_{"Hilal"};

    // Kamera
    std::unique_ptr<camera::ICamera> camera_;
    camera::CameraCaps camera_caps_{};
    std::atomic<bool> camera_connected_{false};
    std::optional<camera::CameraId> last_connected_camera_;
    std::vector<camera::CameraId> cached_cameras_;
    mutable std::mutex enum_mutex_;
    std::atomic<double> max_fps_limit_{0.0};
    std::chrono::steady_clock::time_point last_frame_accept_time_{};

    // Threading & Pipeline Slots
    core::LatestSlot<PreviewData> preview_slot_;
    core::RingBuffer<core::Frame> record_ring_buffer_{256}; // 256-frame drop-oldest buffer (NFR-03)

    // Threads
    std::thread processing_thread_;
    std::thread writer_thread_;
    std::atomic<bool> pipeline_running_{false};
    std::condition_variable frame_cv_;
    std::mutex frame_mutex_;
    std::optional<core::Frame> next_process_frame_;

    // Kalibrasi
    imaging::CalibrationProcessor calibration_;

    // Stacker
    std::unique_ptr<imaging::LiveStacker> stacker_;
    std::atomic<bool> stacking_enabled_{false};

    // Stretch
    imaging::ImageStretcher stretcher_;
    std::mutex stretcher_mutex_;

    // AI
    detect::OnnxDetector detector_;
    std::unique_ptr<detect::DetectionLogger> detection_logger_;
    std::atomic<bool> ai_enabled_{false};

    // Recording
    std::atomic<bool> is_recording_{false};
    std::string record_format_{"fits"};
    std::atomic<int> target_record_count_{-1};
    std::atomic<uint64_t> frames_recorded_{0};
    std::atomic<double> disk_write_mb_s_{0.0};
    std::chrono::steady_clock::time_point last_manifest_write_time_{};
    std::unique_ptr<imaging::io::AviWriter> avi_writer_;
    std::unique_ptr<imaging::io::Mp4Writer> mp4_writer_;
    std::mutex avi_mutex_;

    // Astronomi
    astro::Site site_{"Semarang", -6.9932, 110.4203, 25.0, 7.0, true};

    // Status & Metrics
    std::atomic<uint64_t> frames_captured_{0};
    std::atomic<uint64_t> frames_dropped_{0};
    std::atomic<uint64_t> disk_free_bytes_{0};
    std::atomic<bool> disk_low_space_warning_{false};
    std::atomic<double> measured_fps_{0.0};
    std::chrono::steady_clock::time_point last_fps_time_{};
    uint64_t fps_counter_{0};
    mutable std::mutex fps_mutex_;

    std::function<void(const std::string&)> status_msg_callback_;
};

} // namespace sabit::app

#endif // SABIT_APP_APP_CONTROLLER_H
