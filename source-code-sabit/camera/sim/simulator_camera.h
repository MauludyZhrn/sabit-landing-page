#ifndef SABIT_CAMERA_SIMULATOR_CAMERA_H
#define SABIT_CAMERA_SIMULATOR_CAMERA_H

#include <thread>
#include <atomic>
#include <mutex>
#include <cmath>
#include "src/camera/icamera.h"

namespace sabit::camera {

enum class SimulatorMode {
    Hilal = 0,    // Twilight sky with crescent Hilal
    Gradient = 1, // Full 0..65535 linear gradient
    StarField = 2 // Starry sky with noise, vignetting, and hot pixels
};

class SimulatorCamera : public ICamera {
public:
    explicit SimulatorCamera(SimulatorMode mode = SimulatorMode::Hilal, int width = 1280, int height = 720);
    ~SimulatorCamera() override;

    core::Result<CameraCaps> open(const CameraId& id) override;
    void close() override;

    [[nodiscard]] CameraCaps caps() const override;
    [[nodiscard]] CameraStatus status() const override;

    core::Result<CameraSettings> apply(const CameraSettings& settings) override;

    core::Result<void> start(FrameCallback on_frame) override;
    void stop() override;

    void set_mode(SimulatorMode mode);
    [[nodiscard]] SimulatorMode mode() const noexcept { return mode_; }

    void set_target_fps(double fps);
    void set_hot_pixel(int x, int y, uint16_t value = 65535);

private:
    void capture_loop();
    core::Frame generate_frame(uint64_t seq);
    void render_hilal(uint16_t* buf, int w, int h);
    void render_gradient(uint16_t* buf, int w, int h);
    void render_starfield(uint16_t* buf, int w, int h);

    SimulatorMode mode_{SimulatorMode::Hilal};
    int base_width_{1280};
    int base_height_{720};

    // Current settings
    int64_t exposure_us_{100000}; // 100 ms default
    double gain_{100.0};
    int binning_{1};
    int bit_depth_{16};
    Cooler cooler_{false, std::nullopt};
    double target_fps_{10.0};

    // Hot pixel for TC-028 testing
    int hot_pixel_x_{-1};
    int hot_pixel_y_{-1};
    uint16_t hot_pixel_val_{65535};

    // State & threading
    bool is_open_{false};
    std::atomic<bool> is_streaming_{false};
    std::thread worker_thread_;
    FrameCallback frame_callback_;
    mutable std::mutex mutex_;

    // Status tracking
    uint64_t frames_captured_{0};
    double actual_fps_{0.0};
    double simulated_temp_c_{24.5};
};

} // namespace sabit::camera

#endif // SABIT_CAMERA_SIMULATOR_CAMERA_H
