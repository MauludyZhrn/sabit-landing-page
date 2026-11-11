#include "src/imaging/calibration.h"
#include <algorithm>
#include <numeric>

namespace sabit::imaging {

core::Result<MasterDark> CalibrationProcessor::create_master_dark(const std::vector<core::Frame>& dark_frames) {
    if (dark_frames.empty()) {
        return core::Result<MasterDark>::err(core::ErrorCode::OutOfRange, "No dark frames provided");
    }

    const auto& first = dark_frames[0];
    if (!first.is_valid()) {
        return core::Result<MasterDark>::err(core::ErrorCode::InvalidState, "Invalid initial dark frame");
    }

    int w = first.width;
    int h = first.height;
    int ch = first.channels;
    size_t count = first.pixel_count();

    MasterDark dark;
    dark.width = w;
    dark.height = h;
    dark.channels = ch;
    dark.frame_count = static_cast<int>(dark_frames.size());
    dark.data.assign(count, 0.0f);

    for (const auto& f : dark_frames) {
        if (f.width != w || f.height != h || f.channels != ch || !f.pixels) {
            return core::Result<MasterDark>::err(core::ErrorCode::InvalidState, "Mismatched dark frame dimension");
        }
        const uint16_t* px = f.pixels.get();
        for (size_t i = 0; i < count; ++i) {
            dark.data[i] += static_cast<float>(px[i]);
        }
    }

    float inv_n = 1.0f / static_cast<float>(dark.frame_count);
    for (size_t i = 0; i < count; ++i) {
        dark.data[i] *= inv_n;
    }

    return dark;
}

core::Result<MasterFlat> CalibrationProcessor::create_master_flat(const std::vector<core::Frame>& flat_frames, const MasterDark* dark) {
    if (flat_frames.empty()) {
        return core::Result<MasterFlat>::err(core::ErrorCode::OutOfRange, "No flat frames provided");
    }

    const auto& first = flat_frames[0];
    if (!first.is_valid()) {
        return core::Result<MasterFlat>::err(core::ErrorCode::InvalidState, "Invalid initial flat frame");
    }

    int w = first.width;
    int h = first.height;
    int ch = first.channels;
    size_t count = first.pixel_count();

    if (dark && dark->is_valid()) {
        if (dark->width != w || dark->height != h || dark->channels != ch) {
            return core::Result<MasterFlat>::err(core::ErrorCode::InvalidState, "Dark and Flat dimension mismatch");
        }
    }

    std::vector<float> accum(count, 0.0f);
    int valid_frames = 0;

    for (const auto& f : flat_frames) {
        if (f.width != w || f.height != h || f.channels != ch || !f.pixels) {
            return core::Result<MasterFlat>::err(core::ErrorCode::InvalidState, "Mismatched flat frame dimension");
        }
        const uint16_t* px = f.pixels.get();
        for (size_t i = 0; i < count; ++i) {
            float val = static_cast<float>(px[i]);
            if (dark && dark->is_valid()) {
                val -= dark->data[i];
                if (val < 0.0f) val = 0.0f;
            }
            accum[i] += val;
        }
        ++valid_frames;
    }

    float inv_n = 1.0f / static_cast<float>(valid_frames);
    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        accum[i] *= inv_n;
        sum += accum[i];
    }

    float mean_val = static_cast<float>(sum / static_cast<double>(count));
    if (mean_val <= 0.0f) {
        mean_val = 1.0f; // Prevent division by zero
    }

    MasterFlat flat;
    flat.width = w;
    flat.height = h;
    flat.channels = ch;
    flat.frame_count = valid_frames;
    flat.mean_val = mean_val;
    flat.norm_factors.resize(count);

    float inv_mean = 1.0f / mean_val;
    for (size_t i = 0; i < count; ++i) {
        float factor = accum[i] * inv_mean;
        flat.norm_factors[i] = (factor > 1e-4f) ? factor : 1e-4f;
    }

    return flat;
}

void CalibrationProcessor::set_master_dark(MasterDark dark) {
    master_dark_ = std::move(dark);
    dark_enabled_ = master_dark_.is_valid();
}

void CalibrationProcessor::set_master_flat(MasterFlat flat) {
    master_flat_ = std::move(flat);
    flat_enabled_ = master_flat_.is_valid();
}

void CalibrationProcessor::clear_dark() {
    master_dark_ = MasterDark{};
    dark_enabled_ = false;
}

void CalibrationProcessor::clear_flat() {
    master_flat_ = MasterFlat{};
    flat_enabled_ = false;
}

void CalibrationProcessor::apply(core::Frame& frame) const {
    if (!frame.is_valid()) return;

    bool use_dark = is_dark_enabled() && (frame.width == master_dark_.width && frame.height == master_dark_.height);
    bool use_flat = is_flat_enabled() && (frame.width == master_flat_.width && frame.height == master_flat_.height);

    if (!use_dark && !use_flat) return;

    size_t count = frame.pixel_count();
    uint16_t* px = frame.pixels.get();

    for (size_t i = 0; i < count; ++i) {
        float val = static_cast<float>(px[i]);

        // 1. Dark subtraction (TC-026: clamp to 0, no wrap-around underflow)
        if (use_dark) {
            val -= master_dark_.data[i];
            if (val < 0.0f) val = 0.0f;
        }

        // 2. Flat field normalization (TC-025: flatten vignetting)
        if (use_flat) {
            float factor = master_flat_.norm_factors[i];
            if (factor > 1e-4f) {
                val /= factor;
            }
        }

        // 3. Final 16-bit clamp
        if (val > 65535.0f) val = 65535.0f;
        px[i] = static_cast<uint16_t>(val + 0.5f);
    }
}

} // namespace sabit::imaging
