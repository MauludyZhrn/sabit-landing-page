#ifndef SABIT_CAMERA_ASCOM_CAMERA_H
#define SABIT_CAMERA_ASCOM_CAMERA_H

#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <functional>
#include <future>
#include <string>
#include <vector>
#include <cstdint>
#include "src/camera/icamera.h"

namespace sabit::camera {

[[nodiscard]] uint16_t scale_to_u16(int32_t value, int32_t max_adu);

class AscomCamera : public ICamera {
public:
    AscomCamera();
    ~AscomCamera() override;

    core::Result<CameraCaps> open(const CameraId& id) override;
    void close() override;

    [[nodiscard]] CameraCaps caps() const override;
    [[nodiscard]] CameraStatus status() const override;

    core::Result<CameraSettings> apply(const CameraSettings& settings) override;

    core::Result<void> start(FrameCallback on_frame) override;
    void stop() override;

    void set_disconnected_callback(std::function<void()> cb) { on_disconnected_ = std::move(cb); }

    static std::vector<CameraId> enumerate_cameras(bool include_fallback_defaults = true);
    static core::Result<void> show_driver_setup(const std::string& prog_id);
    static uint16_t scale_to_u16(int32_t value, int32_t max_adu) {
        return sabit::camera::scale_to_u16(value, max_adu);
    }

private:
    struct Command {
        CameraSettings settings;
        std::shared_ptr<std::promise<core::Result<CameraSettings>>> promise;
    };

    void camera_worker_loop(std::shared_ptr<std::promise<core::Result<CameraCaps>>> open_promise);
    core::Result<CameraSettings> execute_apply_com(void* disp_ptr, const CameraSettings& req);

    std::string prog_id_;
    std::string display_name_;

    CameraCaps caps_{};
    CameraSettings active_settings_{};
    bool is_open_{false};
    bool can_abort_exposure_{false};
    int32_t max_adu_{65535};
    std::atomic<bool> worker_running_{false};
    std::atomic<bool> is_streaming_{false};
    std::atomic<bool> is_disconnected_{false};

    std::thread worker_thread_;
    std::queue<Command> cmd_queue_;
    std::mutex cmd_mutex_;
    mutable std::mutex state_mutex_;
    FrameCallback frame_callback_;
    std::function<void()> on_disconnected_;

    uint64_t frames_captured_{0};
    uint64_t frames_dropped_hw_{0};
    double actual_fps_{0.0};
    double sensor_temp_c_{std::numeric_limits<double>::quiet_NaN()};
    int cooler_power_pct_{0};
};

} // namespace sabit::camera

#endif // SABIT_CAMERA_ASCOM_CAMERA_H
