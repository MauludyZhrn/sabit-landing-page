#ifndef SABIT_CAMERA_UVC_CAMERA_H
#define SABIT_CAMERA_UVC_CAMERA_H

#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <functional>
#include <future>
#include <string>
#include <vector>
#include "src/camera/icamera.h"

// Forward declaration of Windows / Media Foundation structures
struct IMFMediaSource;
struct IMFSourceReader;

namespace sabit::camera {

class UvcCamera : public ICamera {
public:
    UvcCamera();
    ~UvcCamera() override;

    core::Result<CameraCaps> open(const CameraId& id) override;
    void close() override;

    [[nodiscard]] CameraCaps caps() const override;
    [[nodiscard]] CameraStatus status() const override;

    core::Result<CameraSettings> apply(const CameraSettings& settings) override;

    core::Result<void> start(FrameCallback on_frame) override;
    void stop() override;

    void set_disconnected_callback(std::function<void()> cb) { on_disconnected_ = std::move(cb); }
    void set_auto_reconnect(bool enable) { auto_reconnect_ = enable; }
    void set_crop_dslr_pillarbox(bool enable) { crop_dslr_pillarbox_ = enable; }
    [[nodiscard]] bool is_crop_dslr_pillarbox() const noexcept { return crop_dslr_pillarbox_; }

    static std::vector<CameraId> enumerate_cameras();

private:
    struct Command {
        CameraSettings settings;
        std::shared_ptr<std::promise<core::Result<CameraSettings>>> promise;
    };

    void worker_loop();
    core::Result<CameraSettings> execute_apply_hardware(const CameraSettings& req);
    bool try_reconnect();
    void release_mf_resources();

    int device_index_{-1};
    std::string device_name_;
    std::string device_sym_link_;

    // Media Foundation pointers stored as void* to keep header clean
    void* mf_source_{nullptr};        // IMFMediaSource*
    void* mf_reader_{nullptr};        // IMFSourceReader*
    int video_format_{0};             // 0=Auto, 1=RGB32, 2=RGB24, 3=YUY2, 4=NV12
    int default_stride_{0};
    bool hw_exp_supported_{false};
    bool hw_gain_supported_{false};
    bool crop_dslr_pillarbox_{false};

    CameraCaps caps_{};
    CameraSettings active_settings_{};
    bool is_open_{false};
    std::atomic<bool> is_streaming_{false};
    std::atomic<bool> is_disconnected_{false};
    bool auto_reconnect_{true};

    // Threading & CMDQ
    std::thread worker_thread_;
    std::queue<Command> cmd_queue_;
    std::mutex cmd_mutex_;
    mutable std::mutex state_mutex_;
    FrameCallback frame_callback_;
    std::function<void()> on_disconnected_;

    // Metrics
    uint64_t frames_captured_{0};
    uint64_t frames_dropped_hw_{0};
    double actual_fps_{0.0};
};

} // namespace sabit::camera

#endif // SABIT_CAMERA_UVC_CAMERA_H
