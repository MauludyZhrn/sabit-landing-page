#ifndef SABIT_IMAGING_STRETCH_H
#define SABIT_IMAGING_STRETCH_H

#include <vector>
#include <array>
#include <cstdint>
#include <memory>
#include "src/core/frame.h"

namespace sabit::imaging {

struct Histogram {
    std::vector<uint32_t> bins65k;       // Full 16-bit resolution
    std::array<uint32_t, 256> bins256;   // 256-bin luminance/combined for UI graph (>= 5 Hz)
    std::array<uint32_t, 256> bins256_r; // 256-bin Red channel
    std::array<uint32_t, 256> bins256_g; // 256-bin Green channel
    std::array<uint32_t, 256> bins256_b; // 256-bin Blue channel
    uint16_t min_val{0};
    uint16_t max_val{65535};
    double mean_val{0.0};
    double stddev_val{0.0};
    uint16_t median_val{0};
    uint64_t total_pixels{0};
};

struct StretchConfig {
    bool stretch_enabled{true};
    bool auto_stretch{false};
    float black_point{0.0f};  // 0..65535
    float white_point{65535.0f};
    float gamma{1.0f};        // gamma exponent (< 1.0 brightens shadows)
    float exp_gain_shift{0.0f}; // EV shift (-5.0 to +5.0, multiplier = 2^shift)

    // Auto stretch parameters
    float auto_black_percentile{0.002f}; // 0.2% lowest
    float auto_white_percentile{0.998f}; // 99.8% highest
    float ema_alpha{0.18f}; // Temporal smoothing parameter (TC-022 anti-flicker)

    // Optional visual enhancements & SharpCap-style preprocessing (preview)
    bool clahe_enabled{false};
    float clahe_clip_limit{2.5f};
    int clahe_tiles_x{8};
    int clahe_tiles_y{8};

    bool sharpen_enabled{false};
    float sharpen_strength{0.5f};

    bool invert_enabled{false};

    // SharpCap Reference Image 2 Preprocessing & Image Controls
    bool timestamp_frames{false};
    int banding_suppression{0};      // 0..100
    bool planet_stabilization{false};
    int background_subtraction{0};   // 0=Off, 1=Simple, 2=Blended Offset, 3=Linear Gradient
    bool remove_satellite_trails{false};
    int flip_mode{0};                // 0=None, 1=Horizontal, 2=Vertical, 3=Both
    bool mask_over_exposed{false};
    bool ca_reduction{false};
};

class ImageStretcher {
public:
    explicit ImageStretcher(StretchConfig config = {});
    ~ImageStretcher() = default;

    void set_config(const StretchConfig& config);
    [[nodiscard]] StretchConfig config() const noexcept { return config_; }

    void reset_temporal_smoothing();

    [[nodiscard]] float effective_black_norm() const noexcept {
        if (!config_.stretch_enabled) return 0.0f;
        float bp = config_.auto_stretch ? smooth_black_ : config_.black_point;
        return bp / 65535.0f;
    }
    [[nodiscard]] float effective_white_norm() const noexcept {
        if (!config_.stretch_enabled) return 1.0f;
        float wp = config_.auto_stretch ? smooth_white_ : config_.white_point;
        return wp / 65535.0f;
    }
    [[nodiscard]] float effective_gamma() const noexcept {
        return config_.stretch_enabled ? config_.gamma : 1.0f;
    }

    /**
     * Compute 65k and 256-bin histograms from 16-bit frame.
     */
    static Histogram compute_histogram(const core::Frame& frame);

    /**
     * Convert 16-bit frame to 8-bit preview frame (LatestSlot consumer).
     * Applies stretch (manual or auto EMA), followed by optional CLAHE/Sharpen/Invert.
     * Optionally outputs the 16-bit preprocessed/stretched/flipped frame into out_proc16.
     * Returns 8-bit pixels in std::vector<uint8_t>.
     */
    std::vector<uint8_t> convert_to_preview_8bit(
        const core::Frame& frame,
        Histogram* out_hist = nullptr,
        core::Frame* out_proc16 = nullptr
    );

private:
    void update_auto_points(const Histogram& hist);
    void apply_clahe(uint8_t* img, int w, int h, int channels = 1) const;
    void apply_sharpen(uint8_t* img, int w, int h, int channels = 1) const;

    StretchConfig config_;
    float smooth_black_{0.0f};
    float smooth_white_{65535.0f};
    bool smooth_initialized_{false};
    float ref_centroid_x_{-1.0f};
    float ref_centroid_y_{-1.0f};
};

} // namespace sabit::imaging

#endif // SABIT_IMAGING_STRETCH_H
