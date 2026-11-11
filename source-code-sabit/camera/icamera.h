#ifndef SABIT_CAMERA_ICAMERA_H
#define SABIT_CAMERA_ICAMERA_H

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <future>
#include <cstdint>
#include <limits>
#include "src/core/result.h"
#include "src/core/frame.h"

namespace sabit::camera {

struct CameraId {
    std::string driver; // "zwo", "uvc", "sim", etc.
    std::string id;     // hardware ID or index or simulator mode
    std::string name;   // human-readable display name
};

struct Roi {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

struct Cooler {
    bool enabled{false};
    std::optional<double> target_c;
};

struct CameraCapabilities {
    bool can_control_exposure{true};
    bool can_control_gain{true};
    bool can_control_wb{true};
    bool can_control_roi{true};
    bool can_control_pixel_format{true};
    bool has_hardware_exposure{true};
    bool has_hardware_gain{true};
    bool has_software_fallback{true};
    bool can_auto_exposure{true};
    bool can_auto_gain{true};
    bool can_crop_dslr_aspect{false};
};

struct CameraCaps {
    std::string model;
    std::string serial;
    std::string driver;
    std::string sdk_version;
    int max_width{1920};
    int max_height{1080};
    int64_t min_exposure_us{100};
    int64_t max_exposure_us{3600000000LL}; // 1 hr
    double min_gain{0.0};
    double max_gain{600.0};
    std::vector<int> supported_bins{1, 2, 4};
    std::vector<int> supported_bit_depths{8, 16};
    bool has_cooler{false};
    bool is_color{false};
    core::BayerPattern bayer{core::BayerPattern::None};
    CameraCapabilities capabilities{};
};

struct CameraSettings {
    std::optional<int64_t> exposure_us;
    std::optional<double>  gain;
    std::optional<int>     binning;
    std::optional<int>     bit_depth;
    std::optional<Roi>     roi;
    std::optional<Cooler>  cooler;
    std::optional<int>     wb_red;
    std::optional<int>     wb_blue;
    std::optional<bool>    auto_exposure;
    std::optional<bool>    auto_gain;
    std::optional<bool>    crop_dslr_pillarbox;
};

struct CameraStatus {
    bool is_connected{false};
    bool is_streaming{false};
    double sensor_temp_c{std::numeric_limits<double>::quiet_NaN()};
    int cooler_power_percent{0};
    double actual_fps{0.0};
    uint64_t frames_captured{0};
    uint64_t frames_dropped_hw{0};
};

using FrameCallback = std::function<void(const core::Frame&)>;

class ICamera {
public:
    virtual ~ICamera() = default;

    virtual core::Result<CameraCaps> open(const CameraId& id) = 0;
    virtual void close() = 0;

    [[nodiscard]] virtual CameraCaps caps() const = 0;
    [[nodiscard]] virtual CameraCapabilities capabilities() const { return caps().capabilities; }
    [[nodiscard]] virtual CameraStatus status() const = 0;

    virtual core::Result<CameraSettings> apply(const CameraSettings& settings) = 0;

    virtual core::Result<void> start(FrameCallback on_frame) = 0;
    virtual void stop() = 0;
};

} // namespace sabit::camera

#endif // SABIT_CAMERA_ICAMERA_H
