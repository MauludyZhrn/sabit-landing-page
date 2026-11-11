#include "src/camera/sim/simulator_camera.h"
#include "src/core/clock.h"
#include <algorithm>
#include <chrono>

namespace sabit::camera {

SimulatorCamera::SimulatorCamera(SimulatorMode mode, int width, int height)
    : mode_(mode), base_width_(width), base_height_(height) {}

SimulatorCamera::~SimulatorCamera() {
    close();
}

core::Result<CameraCaps> SimulatorCamera::open(const CameraId& /*id*/) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_open_) {
        return core::Result<CameraCaps>::err(core::ErrorCode::Busy, "Simulator camera already open");
    }
    is_open_ = true;
    return caps();
}

void SimulatorCamera::close() {
    stop();
    std::lock_guard<std::mutex> lock(mutex_);
    is_open_ = false;
}

CameraCaps SimulatorCamera::caps() const {
    CameraCaps c;
    c.model = "Sabit Synthetic Simulator Camera";
    c.serial = "SIM-2026-001";
    c.driver = "sim";
    c.sdk_version = "1.0.0-native";
    c.max_width = base_width_;
    c.max_height = base_height_;
    c.min_exposure_us = 1000;
    c.max_exposure_us = 60000000LL; // 60s
    c.min_gain = 0.0;
    c.max_gain = 600.0;
    c.supported_bins = {1, 2, 4};
    c.supported_bit_depths = {8, 16};
    c.has_cooler = true;
    c.is_color = false;
    c.bayer = core::BayerPattern::None;

    c.capabilities.can_control_exposure = true;
    c.capabilities.can_control_gain = true;
    c.capabilities.can_control_wb = false;
    c.capabilities.can_control_roi = true;
    c.capabilities.can_control_pixel_format = true;
    c.capabilities.has_hardware_exposure = true;
    c.capabilities.has_hardware_gain = true;
    c.capabilities.has_software_fallback = false;
    c.capabilities.can_auto_exposure = false;
    c.capabilities.can_auto_gain = false;
    c.capabilities.can_crop_dslr_aspect = false;

    return c;
}

CameraStatus SimulatorCamera::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    CameraStatus s;
    s.is_connected = is_open_;
    s.is_streaming = is_streaming_.load();
    s.sensor_temp_c = simulated_temp_c_;
    s.cooler_power_percent = cooler_.enabled ? 45 : 0;
    s.actual_fps = actual_fps_;
    s.frames_captured = frames_captured_;
    s.frames_dropped_hw = 0;
    return s;
}

core::Result<CameraSettings> SimulatorCamera::apply(const CameraSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_open_) {
        return core::Result<CameraSettings>::err(core::ErrorCode::InvalidState, "Camera not opened");
    }

    if (settings.exposure_us.has_value()) {
        int64_t exp = *settings.exposure_us;
        if (exp < 1000 || exp > 60000000LL) {
            return core::Result<CameraSettings>::err(core::ErrorCode::OutOfRange, "Exposure out of range [1000, 60000000] us");
        }
        exposure_us_ = exp;
    }

    if (settings.gain.has_value()) {
        double g = *settings.gain;
        if (g < 0.0 || g > 600.0) {
            return core::Result<CameraSettings>::err(core::ErrorCode::OutOfRange, "Gain out of range [0, 600]");
        }
        gain_ = g;
    }

    if (settings.binning.has_value()) {
        int b = *settings.binning;
        if (b != 1 && b != 2 && b != 4) {
            return core::Result<CameraSettings>::err(core::ErrorCode::Unsupported, "Binning must be 1, 2, or 4");
        }
        binning_ = b;
    }

    if (settings.bit_depth.has_value()) {
        int bd = *settings.bit_depth;
        if (bd != 8 && bd != 16) {
            return core::Result<CameraSettings>::err(core::ErrorCode::Unsupported, "Bit depth must be 8 or 16");
        }
        bit_depth_ = bd;
    }

    if (settings.cooler.has_value()) {
        cooler_ = *settings.cooler;
        if (cooler_.enabled && cooler_.target_c.has_value()) {
            simulated_temp_c_ = *cooler_.target_c;
        } else {
            simulated_temp_c_ = 24.5;
        }
    }

    CameraSettings applied;
    applied.exposure_us = exposure_us_;
    applied.gain = gain_;
    applied.binning = binning_;
    applied.bit_depth = bit_depth_;
    applied.cooler = cooler_;
    return applied;
}

core::Result<void> SimulatorCamera::start(FrameCallback on_frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_open_) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "Camera is not open");
    }
    if (is_streaming_.load()) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "Streaming already active");
    }

    frame_callback_ = std::move(on_frame);
    is_streaming_.store(true);
    worker_thread_ = std::thread(&SimulatorCamera::capture_loop, this);

    return core::Result<void>::ok();
}

void SimulatorCamera::stop() {
    if (is_streaming_.exchange(false)) {
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
}

void SimulatorCamera::set_mode(SimulatorMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    mode_ = mode;
}

void SimulatorCamera::set_target_fps(double fps) {
    std::lock_guard<std::mutex> lock(mutex_);
    target_fps_ = (fps > 0.1 && fps <= 120.0) ? fps : 10.0;
}

void SimulatorCamera::set_hot_pixel(int x, int y, uint16_t value) {
    std::lock_guard<std::mutex> lock(mutex_);
    hot_pixel_x_ = x;
    hot_pixel_y_ = y;
    hot_pixel_val_ = value;
}

void SimulatorCamera::capture_loop() {
    uint64_t seq = 0;
    auto last_fps_time = std::chrono::steady_clock::now();
    uint64_t frames_in_second = 0;

    while (is_streaming_.load()) {
        auto start_time = std::chrono::steady_clock::now();

        core::Frame frame = generate_frame(++seq);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++frames_captured_;
            ++frames_in_second;

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time).count();
            if (elapsed >= 1000) {
                actual_fps_ = (static_cast<double>(frames_in_second) * 1000.0) / elapsed;
                frames_in_second = 0;
                last_fps_time = now;
            }
        }

        if (frame_callback_) {
            frame_callback_(frame);
        }

        // Frame interval pacing
        double target_fps = 10.0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            target_fps = target_fps_;
        }
        int interval_ms = static_cast<int>(1000.0 / target_fps);
        auto work_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        if (work_time < interval_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms - work_time));
        }
    }
}

core::Frame SimulatorCamera::generate_frame(uint64_t seq) {
    int w = 0, h = 0, b = 1, bd = 16;
    int64_t exp = 0;
    double g = 0.0, temp = 0.0;
    SimulatorMode cur_mode = SimulatorMode::Hilal;
    int hpx = -1, hpy = -1;
    uint16_t hpval = 65535;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        b = binning_;
        w = base_width_ / b;
        h = base_height_ / b;
        bd = bit_depth_;
        exp = exposure_us_;
        g = gain_;
        temp = simulated_temp_c_;
        cur_mode = mode_;
        hpx = hot_pixel_x_;
        hpy = hot_pixel_y_;
        hpval = hot_pixel_val_;
    }

    core::Frame frame = core::Frame::create_allocated(w, h, 1, bd);
    frame.seq = seq;
    frame.exposure_us = exp;
    frame.gain = g;
    frame.sensor_temp_c = temp;
    frame.binning = b;
    frame.bit_depth = bd;
    frame.bayer = core::BayerPattern::None;
    frame.timestamp_utc = core::Clock::now_utc();
    frame.clock_sync = core::Clock::get_sync();

    uint16_t* buf = frame.pixels.get();
    if (!buf) return frame;

    switch (cur_mode) {
        case SimulatorMode::Gradient:
            render_gradient(buf, w, h);
            break;
        case SimulatorMode::StarField:
            render_starfield(buf, w, h);
            break;
        case SimulatorMode::Hilal:
        default:
            render_hilal(buf, w, h);
            break;
    }

    // Apply exposure and gain brightness scaling so simulator responds dynamically to controls!
    // Nominal reference: 100ms (100000us) and 100 dB gain
    if (cur_mode != SimulatorMode::Gradient) {
        double exp_factor = std::sqrt(static_cast<double>(exp) / 100000.0);
        double gain_factor = 1.0 + (g - 100.0) / 200.0;
        double total_scale = std::clamp(exp_factor * gain_factor, 0.1, 5.0);
        if (std::abs(total_scale - 1.0) > 0.02) {
            size_t total = static_cast<size_t>(w) * h;
            for (size_t i = 0; i < total; ++i) {
                buf[i] = static_cast<uint16_t>(std::clamp(buf[i] * total_scale, 0.0, 65535.0));
            }
        }
    }

    // Apply hot pixel if set (scaled if binned)
    if (hpx >= 0 && hpy >= 0) {
        int bx = hpx / b;
        int by = hpy / b;
        if (bx < w && by < h) {
            buf[by * w + bx] = hpval;
        }
    }

    // If 8-bit mode requested, scale values to 8-bit range (0..255) then shift back to uint16
    if (bd == 8) {
        size_t total = static_cast<size_t>(w) * h;
        for (size_t i = 0; i < total; ++i) {
            buf[i] = (buf[i] >> 8) << 8;
        }
    }

    return frame;
}

void SimulatorCamera::render_gradient(uint16_t* buf, int w, int h) {
    size_t total = static_cast<size_t>(w) * h;
    if (total == 0) return;
    if (total == 1) {
        buf[0] = 0;
        return;
    }

    for (size_t i = 0; i < total; ++i) {
        buf[i] = static_cast<uint16_t>((static_cast<uint64_t>(i) * 65535ULL) / (total - 1));
    }
}

void SimulatorCamera::render_hilal(uint16_t* buf, int w, int h) {
    // Crescent Moon center & dimensions
    double cx = w * 0.50;
    double cy = h * 0.42;
    double r_outer = h * 0.14;
    double r_inner = h * 0.135;
    double inner_offset_x = -h * 0.022;
    double inner_offset_y = -h * 0.005;

    for (int y = 0; y < h; ++y) {
        // Twilight sky gradient: brighter at horizon (bottom), darker at top
        double y_ratio = static_cast<double>(y) / static_cast<double>(h);
        double sky_intensity = 3500.0 + 12000.0 * (y_ratio * y_ratio);

        for (int x = 0; x < w; ++x) {
            double dx_out = x - cx;
            double dy_out = y - cy;
            double d_out_sq = dx_out * dx_out + dy_out * dy_out;

            double dx_in = x - (cx + inner_offset_x);
            double dy_in = y - (cy + inner_offset_y);
            double d_in_sq = dx_in * dx_in + dy_in * dy_in;

            double val = sky_intensity;

            // Thin crescent: inside outer circle, outside inner circle, right side
            if (d_out_sq <= (r_outer * r_outer) && d_in_sq >= (r_inner * r_inner) && dx_out > -r_outer * 0.2) {
                // Bright hilal arc
                val += 42000.0;
            }

            // Pseudo-random mild noise
            uint32_t seed = static_cast<uint32_t>(y * w + x);
            seed = (seed ^ 61) ^ (seed >> 16);
            seed = seed + (seed << 3);
            seed = seed ^ (seed >> 4);
            seed = seed * 0x27d4eb2d;
            seed = seed ^ (seed >> 15);
            double noise = static_cast<double>(seed % 100) - 50.0;

            val = std::clamp(val + noise, 0.0, 65535.0);
            buf[y * w + x] = static_cast<uint16_t>(val);
        }
    }
}

void SimulatorCamera::render_starfield(uint16_t* buf, int w, int h) {
    // 1. Base dark background with slight vignette
    double cx = w * 0.5;
    double cy = h * 0.5;
    double max_dist_sq = cx * cx + cy * cy;

    for (int y = 0; y < h; ++y) {
        double dy = y - cy;
        for (int x = 0; x < w; ++x) {
            double dx = x - cx;
            double dist_sq = dx * dx + dy * dy;
            double vignette = 1.0 - 0.25 * (dist_sq / max_dist_sq); // 25% falloff at corners

            uint32_t hash = static_cast<uint32_t>(y * 31337 + x * 7919);
            hash ^= (hash >> 13);
            hash *= 0x5bd1e995;
            hash ^= (hash >> 15);
            double noise = 500.0 + (hash % 120);

            buf[y * w + x] = static_cast<uint16_t>(std::clamp(noise * vignette, 0.0, 65535.0));
        }
    }

    // 2. Fixed stars
    static const struct Star {
        double rx, ry;
        uint16_t brightness;
    } kStars[] = {
        {0.20, 0.30, 48000},
        {0.35, 0.15, 32000},
        {0.75, 0.25, 54000},
        {0.80, 0.70, 29000},
        {0.50, 0.60, 62000},
        {0.15, 0.80, 36000},
        {0.65, 0.45, 41000},
        {0.85, 0.10, 22000},
        {0.40, 0.85, 18000},
        {0.28, 0.55, 39000}
    };

    for (const auto& s : kStars) {
        int sx = static_cast<int>(s.rx * w);
        int sy = static_cast<int>(s.ry * h);
        if (sx >= 1 && sx < w - 1 && sy >= 1 && sy < h - 1) {
            buf[sy * w + sx] = s.brightness;
            // Halo around star
            buf[(sy - 1) * w + sx] = static_cast<uint16_t>(s.brightness * 0.5);
            buf[(sy + 1) * w + sx] = static_cast<uint16_t>(s.brightness * 0.5);
            buf[sy * w + (sx - 1)] = static_cast<uint16_t>(s.brightness * 0.5);
            buf[sy * w + (sx + 1)] = static_cast<uint16_t>(s.brightness * 0.5);
        }
    }
}

} // namespace sabit::camera
