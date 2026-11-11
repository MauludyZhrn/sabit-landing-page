#ifndef SABIT_IMAGING_CALIBRATION_H
#define SABIT_IMAGING_CALIBRATION_H

#include <vector>
#include <memory>
#include <cstdint>
#include "src/core/frame.h"
#include "src/core/result.h"

namespace sabit::imaging {

struct MasterDark {
    int width{0};
    int height{0};
    int channels{1};
    int frame_count{0};
    std::vector<float> data; // average dark ADU per pixel

    [[nodiscard]] bool is_valid() const noexcept {
        return width > 0 && height > 0 && !data.empty();
    }
};

struct MasterFlat {
    int width{0};
    int height{0};
    int channels{1};
    int frame_count{0};
    float mean_val{1.0f};
    std::vector<float> norm_factors; // normalized: pixel / mean_flat

    [[nodiscard]] bool is_valid() const noexcept {
        return width > 0 && height > 0 && !norm_factors.empty();
    }
};

class CalibrationProcessor {
public:
    CalibrationProcessor() = default;

    // Master building helpers
    static core::Result<MasterDark> create_master_dark(const std::vector<core::Frame>& dark_frames);
    static core::Result<MasterFlat> create_master_flat(const std::vector<core::Frame>& flat_frames, const MasterDark* dark = nullptr);

    void set_master_dark(MasterDark dark);
    void set_master_flat(MasterFlat flat);

    void clear_dark();
    void clear_flat();

    void enable_dark(bool enable) noexcept { dark_enabled_ = enable; }
    void enable_flat(bool enable) noexcept { flat_enabled_ = enable; }

    [[nodiscard]] bool is_dark_enabled() const noexcept { return dark_enabled_ && master_dark_.is_valid(); }
    [[nodiscard]] bool is_flat_enabled() const noexcept { return flat_enabled_ && master_flat_.is_valid(); }

    [[nodiscard]] const MasterDark& master_dark() const noexcept { return master_dark_; }
    [[nodiscard]] const MasterFlat& master_flat() const noexcept { return master_flat_; }

    /**
     * Calibrate frame in-place.
     * Computes (raw - dark) / flat in floating point, safely clamped to [0, 65535].
     * Never wraps around or underflows (TC-026).
     */
    void apply(core::Frame& frame) const;

private:
    bool dark_enabled_{false};
    bool flat_enabled_{false};
    MasterDark master_dark_;
    MasterFlat master_flat_;
};

} // namespace sabit::imaging

#endif // SABIT_IMAGING_CALIBRATION_H
