#ifndef SABIT_CAMERA_ZWO_CAMERA_H
#define SABIT_CAMERA_ZWO_CAMERA_H

#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <functional>
#include <future>
#include "src/camera/icamera.h"
#include "vendor/zwo/ASICamera2.h"

namespace sabit::camera {

class ZwoCamera : public ICamera {
public:
    explicit ZwoCamera(const std::string& dll_path = "vendor/zwo/ASICamera2.dll");
    ~ZwoCamera() override;

    core::Result<CameraCaps> open(const CameraId& id) override;
    void close() override;

    [[nodiscard]] CameraCaps caps() const override;
    [[nodiscard]] CameraStatus status() const override;

    core::Result<CameraSettings> apply(const CameraSettings& settings) override;

    core::Result<void> start(FrameCallback on_frame) override;
    void stop() override;

    // Optional event hooks
    void set_disconnected_callback(std::function<void()> cb) { on_disconnected_ = std::move(cb); }
    void set_auto_reconnect(bool enable) { auto_reconnect_ = enable; }

    static int get_connected_camera_count(const std::string& dll_path = "vendor/zwo/ASICamera2.dll");
    static std::vector<CameraId> enumerate_cameras(const std::string& dll_path = "vendor/zwo/ASICamera2.dll");

private:
    struct Command {
        CameraSettings settings;
        std::shared_ptr<std::promise<core::Result<CameraSettings>>> promise;
    };

    bool load_sdk();
    void unload_sdk();
    void camera_worker_loop();
    core::Result<CameraSettings> execute_apply_hardware(const CameraSettings& req);
    bool try_reconnect();

    std::string dll_path_;
    void* dll_handle_{nullptr};
    int camera_index_{-1};
    int camera_id_{-1};
    ASI_CAMERA_INFO camera_info_{};

    // Current state & settings
    CameraCaps caps_{};
    CameraSettings active_settings_{};
    bool is_open_{false};
    std::atomic<bool> is_streaming_{false};
    std::atomic<bool> is_disconnected_{false};
    bool auto_reconnect_{true};

    // CMDQ & Threading
    std::thread worker_thread_;
    std::queue<Command> cmd_queue_;
    std::mutex cmd_mutex_;
    mutable std::mutex state_mutex_;
    FrameCallback frame_callback_;
    std::function<void()> on_disconnected_;

    // Performance & metrics
    uint64_t frames_captured_{0};
    uint64_t frames_dropped_hw_{0};
    double actual_fps_{0.0};
    double sensor_temp_c_{std::numeric_limits<double>::quiet_NaN()};

    // ZWO Function Pointers
    typedef int (ASICAM_API *Fn_ASIGetNumOfConnectedCameras)();
    typedef ASI_ERROR_CODE (ASICAM_API *Fn_ASIGetCameraProperty)(ASI_CAMERA_INFO*, int);
    typedef ASI_ERROR_CODE (ASICAM_API *Fn_ASIOpenCamera)(int);
    typedef ASI_ERROR_CODE (ASICAM_API *Fn_ASIInitCamera)(int);
    typedef ASI_ERROR_CODE (ASICAM_API *Fn_ASICloseCamera)(int);
    typedef ASI_ERROR_CODE (ASICAM_API *Fn_ASIGetControlValue)(int, int, long*, ASI_BOOL*);
    typedef ASI_ERROR_CODE (ASICAM_API *Fn_ASISetControlValue)(int, int, long, ASI_BOOL);
    typedef ASI_ERROR_CODE (ASICAM_API *Fn_ASIGetROIFormat)(int, int*, int*, int*, ASI_IMG_TYPE*);
    typedef ASI_ERROR_CODE (ASICAM_API *Fn_ASISetROIFormat)(int, int, int, int, ASI_IMG_TYPE);
    typedef ASI_ERROR_CODE (ASICAM_API *Fn_ASIGetDroppedFrames)(int, int*);
    typedef ASI_ERROR_CODE (ASICAM_API *Fn_ASIStartVideoCapture)(int);
    typedef ASI_ERROR_CODE (ASICAM_API *Fn_ASIStopVideoCapture)(int);
    typedef ASI_ERROR_CODE (ASICAM_API *Fn_ASIGetVideoData)(int, unsigned char*, long, int);
    typedef const char* (ASICAM_API *Fn_ASIGetSDKVersion)();

    Fn_ASIGetNumOfConnectedCameras p_ASIGetNumOfConnectedCameras{nullptr};
    Fn_ASIGetCameraProperty        p_ASIGetCameraProperty{nullptr};
    Fn_ASIOpenCamera               p_ASIOpenCamera{nullptr};
    Fn_ASIInitCamera               p_ASIInitCamera{nullptr};
    Fn_ASICloseCamera              p_ASICloseCamera{nullptr};
    Fn_ASIGetControlValue          p_ASIGetControlValue{nullptr};
    Fn_ASISetControlValue          p_ASISetControlValue{nullptr};
    Fn_ASIGetROIFormat             p_ASIGetROIFormat{nullptr};
    Fn_ASISetROIFormat             p_ASISetROIFormat{nullptr};
    Fn_ASIGetDroppedFrames         p_ASIGetDroppedFrames{nullptr};
    Fn_ASIStartVideoCapture        p_ASIStartVideoCapture{nullptr};
    Fn_ASIStopVideoCapture         p_ASIStopVideoCapture{nullptr};
    Fn_ASIGetVideoData             p_ASIGetVideoData{nullptr};
    Fn_ASIGetSDKVersion            p_ASIGetSDKVersion{nullptr};
};

} // namespace sabit::camera

#endif // SABIT_CAMERA_ZWO_CAMERA_H
