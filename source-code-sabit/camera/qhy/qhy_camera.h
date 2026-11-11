#ifndef SABIT_CAMERA_QHY_CAMERA_H
#define SABIT_CAMERA_QHY_CAMERA_H

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

enum QhyControlId : int {
    QHY_CONTROL_BRIGHTNESS = 0,
    QHY_CONTROL_CONTRAST = 1,
    QHY_CONTROL_WBR = 2,
    QHY_CONTROL_WBB = 3,
    QHY_CONTROL_WBG = 4,
    QHY_CONTROL_GAMMA = 5,
    QHY_CONTROL_GAIN = 6,
    QHY_CONTROL_OFFSET = 7,
    QHY_CONTROL_EXPOSURE = 8,
    QHY_CONTROL_SPEED = 9,
    QHY_CONTROL_TRANSFERBIT = 10,
    QHY_CONTROL_CHANNELS = 11,
    QHY_CONTROL_USBTRAFFIC = 12,
    QHY_CONTROL_ROWNOISERE = 13,
    QHY_CONTROL_CURTEMP = 14,
    QHY_CONTROL_CURPWM = 15,
    QHY_CONTROL_MANULPWM = 16,
    QHY_CONTROL_CFWPORT = 17,
    QHY_CONTROL_COOLER = 18,
    QHY_CONTROL_ST4PORT = 19,
    QHY_CAM_COLOR = 20,
    QHY_CAM_BIN1X1MODE = 21,
    QHY_CAM_BIN2X2MODE = 22,
    QHY_CAM_BIN3X3MODE = 23,
    QHY_CAM_BIN4X4MODE = 24,
    QHY_CAM_8BITS = 34,
    QHY_CAM_16BITS = 35,
    QHY_3A_AUTOBALANCE = 38,
    QHY_3A_AUTOEXPOSURE = 39,
    QHY_CAM_SINGLEFRAMEMODE = 57,
    QHY_CAM_LIVEVIDEOMODE = 58,
    QHY_CAM_IS_COLOR = 59
};

class QhyCamera : public ICamera {
public:
    explicit QhyCamera(const std::string& dll_path = "vendor/qhy/qhyccd.dll");
    ~QhyCamera() override;

    core::Result<CameraCaps> open(const CameraId& id) override;
    void close() override;

    [[nodiscard]] CameraCaps caps() const override;
    [[nodiscard]] CameraStatus status() const override;

    core::Result<CameraSettings> apply(const CameraSettings& settings) override;

    core::Result<void> start(FrameCallback on_frame) override;
    void stop() override;

    void set_disconnected_callback(std::function<void()> cb) { on_disconnected_ = std::move(cb); }
    void set_auto_reconnect(bool enable) { auto_reconnect_ = enable; }

    static int get_connected_camera_count(const std::string& dll_path = "vendor/qhy/qhyccd.dll");
    static std::vector<CameraId> enumerate_cameras(const std::string& dll_path = "vendor/qhy/qhyccd.dll");

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
    void* cam_handle_{nullptr};
    std::string hw_id_;
    bool resource_initialized_{false};

    // Chip info
    double chip_w_mm_{0.0};
    double chip_h_mm_{0.0};
    uint32_t max_w_{1920};
    uint32_t max_h_{1080};
    double pixel_w_um_{3.75};
    double pixel_h_um_{3.75};
    uint32_t bpp_{16};
    uint32_t roi_w_{1920};
    uint32_t roi_h_{1080};

    // State & settings
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

    // Metrics
    uint64_t frames_captured_{0};
    uint64_t frames_dropped_hw_{0};
    double actual_fps_{0.0};
    double sensor_temp_c_{std::numeric_limits<double>::quiet_NaN()};
    int cooler_power_pct_{0};

    // QHYCCD C-SDK Function Pointers
    using Fn_InitQHYCCDResource       = uint32_t (*)();
    using Fn_ReleaseQHYCCDResource    = uint32_t (*)();
    using Fn_ScanQHYCCD               = uint32_t (*)();
    using Fn_GetQHYCCDId              = uint32_t (*)(uint32_t, char*);
    using Fn_OpenQHYCCD               = void* (*)(char*);
    using Fn_CloseQHYCCD              = uint32_t (*)(void*);
    using Fn_SetQHYCCDStreamMode      = uint32_t (*)(void*, uint8_t);
    using Fn_InitQHYCCD               = uint32_t (*)(void*);
    using Fn_GetQHYCCDChipInfo        = uint32_t (*)(void*, double*, double*, uint32_t*, uint32_t*, double*, double*, uint32_t*);
    using Fn_IsQHYCCDControlAvailable = uint32_t (*)(void*, int);
    using Fn_SetQHYCCDParam           = uint32_t (*)(void*, int, double);
    using Fn_GetQHYCCDParam           = double (*)(void*, int);
    using Fn_GetQHYCCDParamMinMaxStep = uint32_t (*)(void*, int, double*, double*, double*);
    using Fn_SetQHYCCDBitsMode        = uint32_t (*)(void*, uint32_t);
    using Fn_SetQHYCCDBinMode         = uint32_t (*)(void*, uint32_t, uint32_t);
    using Fn_SetQHYCCDResolution      = uint32_t (*)(void*, uint32_t, uint32_t, uint32_t, uint32_t);
    using Fn_GetQHYCCDMemLength       = uint32_t (*)(void*);
    using Fn_BeginQHYCCDLive          = uint32_t (*)(void*);
    using Fn_GetQHYCCDLiveFrame       = uint32_t (*)(void*, uint32_t*, uint32_t*, uint32_t*, uint32_t*, uint8_t*);
    using Fn_StopQHYCCDLive           = uint32_t (*)(void*);
    using Fn_ExpQHYCCDSingleFrame     = uint32_t (*)(void*);
    using Fn_GetQHYCCDSingleFrame     = uint32_t (*)(void*, uint32_t*, uint32_t*, uint32_t*, uint32_t*, uint8_t*);
    using Fn_ControlQHYCCDTemp        = uint32_t (*)(void*, double);

    Fn_InitQHYCCDResource       p_InitQHYCCDResource{nullptr};
    Fn_ReleaseQHYCCDResource    p_ReleaseQHYCCDResource{nullptr};
    Fn_ScanQHYCCD               p_ScanQHYCCD{nullptr};
    Fn_GetQHYCCDId              p_GetQHYCCDId{nullptr};
    Fn_OpenQHYCCD               p_OpenQHYCCD{nullptr};
    Fn_CloseQHYCCD              p_CloseQHYCCD{nullptr};
    Fn_SetQHYCCDStreamMode      p_SetQHYCCDStreamMode{nullptr};
    Fn_InitQHYCCD               p_InitQHYCCD{nullptr};
    Fn_GetQHYCCDChipInfo        p_GetQHYCCDChipInfo{nullptr};
    Fn_IsQHYCCDControlAvailable p_IsQHYCCDControlAvailable{nullptr};
    Fn_SetQHYCCDParam           p_SetQHYCCDParam{nullptr};
    Fn_GetQHYCCDParam           p_GetQHYCCDParam{nullptr};
    Fn_GetQHYCCDParamMinMaxStep p_GetQHYCCDParamMinMaxStep{nullptr};
    Fn_SetQHYCCDBitsMode        p_SetQHYCCDBitsMode{nullptr};
    Fn_SetQHYCCDBinMode         p_SetQHYCCDBinMode{nullptr};
    Fn_SetQHYCCDResolution      p_SetQHYCCDResolution{nullptr};
    Fn_GetQHYCCDMemLength       p_GetQHYCCDMemLength{nullptr};
    Fn_BeginQHYCCDLive          p_BeginQHYCCDLive{nullptr};
    Fn_GetQHYCCDLiveFrame       p_GetQHYCCDLiveFrame{nullptr};
    Fn_StopQHYCCDLive           p_StopQHYCCDLive{nullptr};
    Fn_ExpQHYCCDSingleFrame     p_ExpQHYCCDSingleFrame{nullptr};
    Fn_GetQHYCCDSingleFrame     p_GetQHYCCDSingleFrame{nullptr};
    Fn_ControlQHYCCDTemp        p_ControlQHYCCDTemp{nullptr};
};

} // namespace sabit::camera

#endif // SABIT_CAMERA_QHY_CAMERA_H
