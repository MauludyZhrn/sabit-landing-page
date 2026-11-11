#include "src/imaging/stretch.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

namespace sabit::imaging {

ImageStretcher::ImageStretcher(StretchConfig config) : config_(config) {}

void ImageStretcher::set_config(const StretchConfig& config) {
    config_ = config;
}

void ImageStretcher::reset_temporal_smoothing() {
    smooth_initialized_ = false;
    ref_centroid_x_ = -1.0f;
    ref_centroid_y_ = -1.0f;
}

Histogram ImageStretcher::compute_histogram(const core::Frame& frame) {
    Histogram h;
    h.bins65k.assign(65536, 0);
    h.bins256.fill(0);
    h.bins256_r.fill(0);
    h.bins256_g.fill(0);
    h.bins256_b.fill(0);

    if (!frame.is_valid()) return h;

    size_t count = frame.pixel_count();
    h.total_pixels = count;
    if (count == 0) return h;

    const uint16_t* px = frame.pixels.get();
    uint16_t min_v = 65535;
    uint16_t max_v = 0;
    double sum = 0.0;

    for (size_t i = 0; i < count; ++i) {
        uint16_t val = px[i];
        min_v = std::min(min_v, val);
        max_v = std::max(max_v, val);
        sum += val;

        ++h.bins65k[val];
        ++h.bins256[val >> 8];
    }

    if (frame.channels >= 3) {
        size_t num_px = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
        int ch = frame.channels;
        for (size_t p = 0; p < num_px; ++p) {
            size_t base = p * ch;
            ++h.bins256_r[px[base + 0] >> 8];
            ++h.bins256_g[px[base + 1] >> 8];
            ++h.bins256_b[px[base + 2] >> 8];
        }
    } else {
        h.bins256_r = h.bins256;
        h.bins256_g = h.bins256;
        h.bins256_b = h.bins256;
    }

    h.min_val = min_v;
    h.max_val = max_v;
    h.mean_val = sum / static_cast<double>(count);

    // Compute standard deviation and median
    double var_sum = 0.0;
    uint64_t median_threshold = count / 2;
    uint64_t accum = 0;
    bool found_median = false;

    for (size_t i = 0; i < 65536; ++i) {
        uint32_t bin_c = h.bins65k[i];
        if (bin_c > 0) {
            double diff = static_cast<double>(i) - h.mean_val;
            var_sum += bin_c * (diff * diff);

            accum += bin_c;
            if (!found_median && accum >= median_threshold) {
                h.median_val = static_cast<uint16_t>(i);
                found_median = true;
            }
        }
    }

    h.stddev_val = std::sqrt(var_sum / static_cast<double>(count));
    return h;
}

void ImageStretcher::update_auto_points(const Histogram& hist) {
    if (hist.total_pixels == 0) return;

    uint64_t black_thresh = static_cast<uint64_t>(hist.total_pixels * config_.auto_black_percentile);
    uint64_t white_thresh = static_cast<uint64_t>(hist.total_pixels * config_.auto_white_percentile);

    uint64_t accum = 0;
    float raw_bp = 0.0f;
    float raw_wp = 65535.0f;
    bool found_bp = false;

    for (size_t i = 0; i < 65536; ++i) {
        accum += hist.bins65k[i];
        if (!found_bp && accum >= black_thresh) {
            raw_bp = static_cast<float>(i);
            found_bp = true;
        }
        if (accum >= white_thresh) {
            raw_wp = static_cast<float>(i);
            break;
        }
    }

    if (raw_wp <= raw_bp) {
        raw_wp = raw_bp + 1.0f;
    }

    // Temporal smoothing (EMA) to prevent flickering on static scenes (TC-022)
    if (!smooth_initialized_) {
        smooth_black_ = raw_bp;
        smooth_white_ = raw_wp;
        smooth_initialized_ = true;
    } else {
        float alpha = config_.ema_alpha;
        smooth_black_ = alpha * raw_bp + (1.0f - alpha) * smooth_black_;
        smooth_white_ = alpha * raw_wp + (1.0f - alpha) * smooth_white_;
    }
}

std::vector<uint8_t> ImageStretcher::convert_to_preview_8bit(
    const core::Frame& frame,
    Histogram* out_hist,
    core::Frame* out_proc16
) {
    if (!frame.is_valid()) return {};

    const int w = frame.width;
    const int h = frame.height;
    const int ch = std::max(1, frame.channels);
    const size_t count = frame.pixel_count();
    const uint16_t* raw_src = frame.pixels.get();

    // Create mutable 16-bit working buffer if any 16-bit preprocessing / EV shift is active
    const bool has_ev_shift = (std::abs(config_.exp_gain_shift) > 0.01f);
    const bool need_preproc = has_ev_shift ||
                              (config_.background_subtraction > 0) ||
                              (config_.banding_suppression > 0) ||
                              config_.remove_satellite_trails ||
                              config_.planet_stabilization ||
                              config_.ca_reduction;

    std::vector<uint16_t> work16;
    const uint16_t* src = raw_src;

    if (need_preproc) {
        work16.assign(raw_src, raw_src + count);

        // 1. Exposure/Gain Shift (EV stops: multiplier = 2^shift)
        if (has_ev_shift) {
            float ev_mult = std::pow(2.0f, std::clamp(config_.exp_gain_shift, -5.0f, 5.0f));
            for (size_t i = 0; i < count; ++i) {
                float v = static_cast<float>(work16[i]) * ev_mult;
                work16[i] = static_cast<uint16_t>(std::clamp(v, 0.0f, 65535.0f));
            }
        }

        // 2. Background Subtraction (1=Simple Offset, 2=Blended Sky Offset, 3=Linear Twilight Gradient)
        if (config_.background_subtraction > 0 && w >= 4 && h >= 4) {
            // Sample 4 corner regions (each 8x8 or w/8 x h/8) to estimate sky background & gradient
            int cw = std::clamp(w / 10, 2, 32);
            int ch_box = std::clamp(h / 10, 2, 32);
            auto corner_mean = [&](int x0, int y0, int c_idx) -> float {
                double s = 0.0;
                int n = 0;
                for (int y = y0; y < y0 + ch_box && y < h; ++y) {
                    for (int x = x0; x < x0 + cw && x < w; ++x) {
                        s += work16[(static_cast<size_t>(y) * w + x) * ch + c_idx];
                        ++n;
                    }
                }
                return (n > 0) ? static_cast<float>(s / n) : 0.0f;
            };

            for (int c_idx = 0; c_idx < ch; ++c_idx) {
                float tl = corner_mean(0, 0, c_idx);
                float tr = corner_mean(w - cw, 0, c_idx);
                float bl = corner_mean(0, h - ch_box, c_idx);
                float br = corner_mean(w - cw, h - ch_box, c_idx);
                float min_bg = std::min({tl, tr, bl, br});

                if (config_.background_subtraction == 1) {
                    // Simple Offset
                    uint16_t offset = static_cast<uint16_t>(min_bg * 0.85f);
                    for (int y = 0; y < h; ++y) {
                        for (int x = 0; x < w; ++x) {
                            size_t idx = (static_cast<size_t>(y) * w + x) * ch + c_idx;
                            work16[idx] = (work16[idx] > offset) ? (work16[idx] - offset) : 0;
                        }
                    }
                } else if (config_.background_subtraction == 2) {
                    // Blended Offset (preserves bright lunar limb while suppressing sky floor)
                    float bg_floor = min_bg * 0.90f;
                    for (int y = 0; y < h; ++y) {
                        for (int x = 0; x < w; ++x) {
                            size_t idx = (static_cast<size_t>(y) * w + x) * ch + c_idx;
                            float val = static_cast<float>(work16[idx]);
                            float sub = std::max(0.0f, val - bg_floor);
                            work16[idx] = static_cast<uint16_t>(std::clamp(sub * 1.15f, 0.0f, 65535.0f));
                        }
                    }
                } else if (config_.background_subtraction == 3) {
                    // Linear Twilight Gradient (bilinear interpolation of 4 corners)
                    for (int y = 0; y < h; ++y) {
                        float fy = static_cast<float>(y) / std::max(1, h - 1);
                        float left_bg = tl * (1.0f - fy) + bl * fy;
                        float right_bg = tr * (1.0f - fy) + br * fy;
                        for (int x = 0; x < w; ++x) {
                            float fx = static_cast<float>(x) / std::max(1, w - 1);
                            float grad_bg = (left_bg * (1.0f - fx) + right_bg * fx) * 0.88f;
                            size_t idx = (static_cast<size_t>(y) * w + x) * ch + c_idx;
                            float val = static_cast<float>(work16[idx]);
                            work16[idx] = static_cast<uint16_t>(std::clamp(val - grad_bg, 0.0f, 65535.0f));
                        }
                    }
                }
            }
        }

        // 3. Horizontal CMOS Banding Suppression (0..100)
        if (config_.banding_suppression > 0 && h >= 8) {
            float strength = std::clamp(config_.banding_suppression / 100.0f, 0.0f, 1.0f);
            std::vector<float> row_means(h, 0.0f);
            for (int y = 0; y < h; ++y) {
                double sum = 0.0;
                const size_t row_start = static_cast<size_t>(y) * w * ch;
                const size_t row_elems = static_cast<size_t>(w) * ch;
                for (size_t i = 0; i < row_elems; ++i) {
                    sum += work16[row_start + i];
                }
                row_means[y] = static_cast<float>(sum / std::max<size_t>(1, row_elems));
            }
            for (int y = 0; y < h; ++y) {
                float local_sum = 0.0f;
                int local_cnt = 0;
                for (int dy = -6; dy <= 6; ++dy) {
                    int ny = std::clamp(y + dy, 0, h - 1);
                    local_sum += row_means[ny];
                    ++local_cnt;
                }
                float local_avg = local_sum / local_cnt;
                float delta = (row_means[y] - local_avg) * strength;
                const size_t row_start = static_cast<size_t>(y) * w * ch;
                const size_t row_elems = static_cast<size_t>(w) * ch;
                for (size_t i = 0; i < row_elems; ++i) {
                    float v = static_cast<float>(work16[row_start + i]) - delta;
                    work16[row_start + i] = static_cast<uint16_t>(std::clamp(v, 0.0f, 65535.0f));
                }
            }
        }

        // 4. Remove Satellite Trails / Hot Line Outliers
        if (config_.remove_satellite_trails && h >= 3 && w >= 3) {
            std::vector<uint16_t> cleaned = work16;
            for (int y = 1; y + 1 < h; ++y) {
                for (int x = 1; x + 1 < w; ++x) {
                    for (int c_idx = 0; c_idx < ch; ++c_idx) {
                        size_t idx = (static_cast<size_t>(y) * w + x) * ch + c_idx;
                        uint16_t up = work16[(static_cast<size_t>(y - 1) * w + x) * ch + c_idx];
                        uint16_t dn = work16[(static_cast<size_t>(y + 1) * w + x) * ch + c_idx];
                        uint16_t lf = work16[(static_cast<size_t>(y) * w + (x - 1)) * ch + c_idx];
                        uint16_t rt = work16[(static_cast<size_t>(y) * w + (x + 1)) * ch + c_idx];
                        uint32_t avg_nb = (static_cast<uint32_t>(up) + dn + lf + rt) / 4;
                        if (work16[idx] > avg_nb + 12000 && std::abs(static_cast<int>(up) - static_cast<int>(dn)) < 4000) {
                            cleaned[idx] = static_cast<uint16_t>(avg_nb);
                        }
                    }
                }
            }
            work16.swap(cleaned);
        }

        // 5. CA Reduction / Synthetic Blue (reduces atmospheric dispersion blue halo)
        if (config_.ca_reduction && ch >= 3) {
            size_t num_px = static_cast<size_t>(w) * h;
            for (size_t p = 0; p < num_px; ++p) {
                size_t base = p * ch;
                float r = static_cast<float>(work16[base + 0]);
                float g = static_cast<float>(work16[base + 1]);
                float b = static_cast<float>(work16[base + 2]);
                float synth_b = std::clamp(2.0f * g - r, 0.0f, 65535.0f);
                work16[base + 2] = static_cast<uint16_t>(std::clamp(0.55f * b + 0.45f * synth_b, 0.0f, 65535.0f));
            }
        }

        // 6. Planet / Disk Stabilization (Centroid Lock)
        if (config_.planet_stabilization && w >= 16 && h >= 16) {
            double sum_w = 0.0, sum_x = 0.0, sum_y = 0.0;
            for (int y = 0; y < h; y += 2) {
                for (int x = 0; x < w; x += 2) {
                    size_t idx = (static_cast<size_t>(y) * w + x) * ch;
                    double val = work16[idx];
                    if (val > 8000.0) {
                        double weight = val - 8000.0;
                        sum_w += weight;
                        sum_x += weight * x;
                        sum_y += weight * y;
                    }
                }
            }
            if (sum_w > 1000.0) {
                float cx = static_cast<float>(sum_x / sum_w);
                float cy = static_cast<float>(sum_y / sum_w);
                if (ref_centroid_x_ < 0.0f || ref_centroid_y_ < 0.0f) {
                    ref_centroid_x_ = cx;
                    ref_centroid_y_ = cy;
                } else {
                    int dx = std::clamp(static_cast<int>(std::lround(cx - ref_centroid_x_)), -w / 4, w / 4);
                    int dy = std::clamp(static_cast<int>(std::lround(cy - ref_centroid_y_)), -h / 4, h / 4);
                    if (std::abs(dx) >= 1 || std::abs(dy) >= 1) {
                        std::vector<uint16_t> shifted(count, 0);
                        for (int y = 0; y < h; ++y) {
                            int sy = y + dy;
                            if (sy < 0 || sy >= h) continue;
                            for (int x = 0; x < w; ++x) {
                                int sx = x + dx;
                                if (sx < 0 || sx >= w) continue;
                                size_t dst_i = (static_cast<size_t>(y) * w + x) * ch;
                                size_t src_i = (static_cast<size_t>(sy) * w + sx) * ch;
                                for (int c_idx = 0; c_idx < ch; ++c_idx) {
                                    shifted[dst_i + c_idx] = work16[src_i + c_idx];
                                }
                            }
                        }
                        work16.swap(shifted);
                    }
                }
            }
        }

        src = work16.data();
    } else {
        ref_centroid_x_ = -1.0f;
        ref_centroid_y_ = -1.0f;
    }

    // Compute histogram on the active working buffer so the histogram reflects EV Shift & Preprocessing!
    core::Frame hist_frame = frame;
    if (!work16.empty()) {
        hist_frame = core::Frame::create_allocated(w, h, ch, frame.bit_depth);
        std::memcpy(hist_frame.pixels.get(), work16.data(), count * sizeof(uint16_t));
    }
    Histogram hist = compute_histogram(hist_frame);
    if (out_hist) {
        *out_hist = hist;
    }

    float bp = 0.0f;
    float wp = 65535.0f;
    float gamma = 1.0f;

    if (config_.stretch_enabled) {
        bp = config_.black_point;
        wp = config_.white_point;
        gamma = (config_.gamma > 0.05f) ? config_.gamma : 1.0f;

        if (config_.auto_stretch) {
            update_auto_points(hist);
            bp = smooth_black_;
            wp = smooth_white_;
        }
    }

    if (wp <= bp) {
        wp = bp + 1.0f;
    }

    float inv_range = 1.0f / (wp - bp);
    bool need_gamma = std::abs(gamma - 1.0f) > 0.01f;

    // Build 16-bit to 8-bit LUT (and optional 16-bit LUT) for ultra-fast O(1) per-pixel stretch + gamma
    const bool apply_16bit_lut = out_proc16 != nullptr && config_.stretch_enabled &&
                                 (config_.auto_stretch || bp > 0.5f || wp < 65534.5f || need_gamma || config_.invert_enabled);
    std::array<uint8_t, 65536> lut{};
    std::vector<uint16_t> lut16;
    if (apply_16bit_lut) {
        lut16.resize(65536);
    }

    for (int v = 0; v < 65536; ++v) {
        float norm = (static_cast<float>(v) - bp) * inv_range;
        norm = std::clamp(norm, 0.0f, 1.0f);
        if (need_gamma) {
            norm = std::pow(norm, 1.0f / gamma); // gamma > 1 brightens midtones, matching SharpCap
        }
        lut[v] = static_cast<uint8_t>(norm * 255.0f + 0.5f);
        if (apply_16bit_lut) {
            float n16 = config_.invert_enabled ? (1.0f - norm) : norm;
            lut16[v] = static_cast<uint16_t>(std::clamp(n16 * 65535.0f + 0.5f, 0.0f, 65535.0f));
        }
    }

    // Populate out_proc16 if requested (for 16-bit FITS / TIFF captures)
    if (out_proc16 && out_proc16->is_valid() && out_proc16->pixel_count() == count) {
        uint16_t* dst16 = out_proc16->pixels.get();
        if (apply_16bit_lut) {
            for (size_t i = 0; i < count; ++i) {
                dst16[i] = lut16[src[i]];
            }
        } else if (!work16.empty()) {
            std::memcpy(dst16, work16.data(), count * sizeof(uint16_t));
        }

        if (config_.flip_mode > 0 && w >= 2 && h >= 2) {
            const bool flip_h = (config_.flip_mode == 1 || config_.flip_mode == 3);
            const bool flip_v = (config_.flip_mode == 2 || config_.flip_mode == 3);
            std::vector<uint16_t> flipped16(count);
            for (int y = 0; y < h; ++y) {
                int sy = flip_v ? (h - 1 - y) : y;
                for (int x = 0; x < w; ++x) {
                    int sx = flip_h ? (w - 1 - x) : x;
                    size_t dst_i = (static_cast<size_t>(y) * w + x) * ch;
                    size_t src_i = (static_cast<size_t>(sy) * w + sx) * ch;
                    for (int c_idx = 0; c_idx < ch; ++c_idx) {
                        flipped16[dst_i + c_idx] = dst16[src_i + c_idx];
                    }
                }
            }
            std::memcpy(dst16, flipped16.data(), count * sizeof(uint16_t));
        }
    }

    std::vector<uint8_t> dst(count);
    for (size_t i = 0; i < count; ++i) {
        dst[i] = lut[src[i]];
    }

    // Optional preview-only enhancements
    if (config_.clahe_enabled) {
        apply_clahe(dst.data(), w, h, ch);
    }

    if (config_.sharpen_enabled) {
        apply_sharpen(dst.data(), w, h, ch);
    }

    if (config_.invert_enabled) {
        for (size_t i = 0; i < count; ++i) {
            dst[i] = 255 - dst[i];
        }
    }

    // Mask over-exposed pixels (highlight saturated pixels in vivid Magenta/Red)
    if (config_.mask_over_exposed && ch >= 3) {
        size_t num_px = static_cast<size_t>(w) * h;
        for (size_t p = 0; p < num_px; ++p) {
            size_t base = p * ch;
            if (src[base + 0] >= 64800 || src[base + 1] >= 64800 || src[base + 2] >= 64800) {
                dst[base + 0] = 255;
                dst[base + 1] = 0;
                dst[base + 2] = 220;
            }
        }
    }

    // Flip (after dark/flat): 1=Horizontal, 2=Vertical, 3=Both
    if (config_.flip_mode > 0 && w >= 2 && h >= 2) {
        const bool flip_h = (config_.flip_mode == 1 || config_.flip_mode == 3);
        const bool flip_v = (config_.flip_mode == 2 || config_.flip_mode == 3);
        std::vector<uint8_t> flipped(count);
        for (int y = 0; y < h; ++y) {
            int sy = flip_v ? (h - 1 - y) : y;
            for (int x = 0; x < w; ++x) {
                int sx = flip_h ? (w - 1 - x) : x;
                size_t dst_i = (static_cast<size_t>(y) * w + x) * ch;
                size_t src_i = (static_cast<size_t>(sy) * w + sx) * ch;
                for (int c_idx = 0; c_idx < ch; ++c_idx) {
                    flipped[dst_i + c_idx] = dst[src_i + c_idx];
                }
            }
        }
        dst.swap(flipped);
    }

    return dst;
}

void ImageStretcher::apply_clahe(uint8_t* img, int w, int h, int channels) const {
    if (channels >= 3) {
        // Compute luminance channel, run CLAHE on luminance, and scale R/G/B preserving chromaticity
        size_t num_px = static_cast<size_t>(w) * static_cast<size_t>(h);
        std::vector<uint8_t> lum(num_px);
        std::vector<uint8_t> lum_orig(num_px);
        for (size_t i = 0; i < num_px; ++i) {
            size_t base = i * channels;
            uint8_t y_val = static_cast<uint8_t>(
                (static_cast<uint32_t>(img[base + 0]) * 77 +
                 static_cast<uint32_t>(img[base + 1]) * 150 +
                 static_cast<uint32_t>(img[base + 2]) * 29) >> 8);
            lum[i] = y_val;
            lum_orig[i] = y_val;
        }
        apply_clahe(lum.data(), w, h, 1);
        for (size_t i = 0; i < num_px; ++i) {
            size_t base = i * channels;
            uint8_t orig_y = lum_orig[i];
            uint8_t new_y = lum[i];
            if (orig_y > 0) {
                float scale = static_cast<float>(new_y) / static_cast<float>(orig_y);
                img[base + 0] = static_cast<uint8_t>(std::clamp(img[base + 0] * scale, 0.0f, 255.0f));
                img[base + 1] = static_cast<uint8_t>(std::clamp(img[base + 1] * scale, 0.0f, 255.0f));
                img[base + 2] = static_cast<uint8_t>(std::clamp(img[base + 2] * scale, 0.0f, 255.0f));
            } else {
                img[base + 0] = new_y;
                img[base + 1] = new_y;
                img[base + 2] = new_y;
            }
        }
        return;
    }

    int tiles_x = std::max(2, config_.clahe_tiles_x);
    int tiles_y = std::max(2, config_.clahe_tiles_y);

    int tile_w = w / tiles_x;
    int tile_h = h / tiles_y;
    if (tile_w < 4 || tile_h < 4) return;

    int total_tiles = tiles_x * tiles_y;
    std::vector<std::array<uint8_t, 256>> cdfs(total_tiles);

    for (int ty = 0; ty < tiles_y; ++ty) {
        int y0 = ty * tile_h;
        int y1 = (ty == tiles_y - 1) ? h : (ty + 1) * tile_h;

        for (int tx = 0; tx < tiles_x; ++tx) {
            int x0 = tx * tile_w;
            int x1 = (tx == tiles_x - 1) ? w : (tx + 1) * tile_w;
            int tile_area = (x1 - x0) * (y1 - y0);

            std::array<uint32_t, 256> hist{};
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    ++hist[img[y * w + x]];
                }
            }

            // Clip histogram
            uint32_t clip_limit = static_cast<uint32_t>(config_.clahe_clip_limit * (tile_area / 256.0f));
            clip_limit = std::max(1u, clip_limit);

            uint32_t excess = 0;
            for (int i = 0; i < 256; ++i) {
                if (hist[i] > clip_limit) {
                    excess += hist[i] - clip_limit;
                    hist[i] = clip_limit;
                }
            }

            uint32_t bonus = excess / 256;
            uint32_t remainder = excess % 256;
            for (int i = 0; i < 256; ++i) {
                hist[i] += bonus;
                if (static_cast<uint32_t>(i) < remainder) {
                    ++hist[i];
                }
            }

            // Build CDF
            auto& cdf = cdfs[ty * tiles_x + tx];
            uint32_t cum = 0;
            float scale = 255.0f / static_cast<float>(tile_area);
            for (int i = 0; i < 256; ++i) {
                cum += hist[i];
                cdf[i] = static_cast<uint8_t>(std::clamp(cum * scale, 0.0f, 255.0f));
            }
        }
    }

    // Bilinear interpolation between tile centers
    std::vector<uint8_t> out(w * h);
    for (int y = 0; y < h; ++y) {
        float ty_f = (y - tile_h * 0.5f) / tile_h;
        int ty1 = static_cast<int>(std::floor(ty_f));
        int ty2 = ty1 + 1;
        float dy = ty_f - ty1;

        ty1 = std::clamp(ty1, 0, tiles_y - 1);
        ty2 = std::clamp(ty2, 0, tiles_y - 1);

        for (int x = 0; x < w; ++x) {
            float tx_f = (x - tile_w * 0.5f) / tile_w;
            int tx1 = static_cast<int>(std::floor(tx_f));
            int tx2 = tx1 + 1;
            float dx = tx_f - tx1;

            tx1 = std::clamp(tx1, 0, tiles_x - 1);
            tx2 = std::clamp(tx2, 0, tiles_x - 1);

            uint8_t val = img[y * w + x];
            float v11 = cdfs[ty1 * tiles_x + tx1][val];
            float v12 = cdfs[ty1 * tiles_x + tx2][val];
            float v21 = cdfs[ty2 * tiles_x + tx1][val];
            float v22 = cdfs[ty2 * tiles_x + tx2][val];

            float top = (1.0f - dx) * v11 + dx * v12;
            float bot = (1.0f - dx) * v21 + dx * v22;
            float interpolated = (1.0f - dy) * top + dy * bot;

            out[y * w + x] = static_cast<uint8_t>(std::clamp(interpolated, 0.0f, 255.0f));
        }
    }

    std::copy(out.begin(), out.end(), img);
}

void ImageStretcher::apply_sharpen(uint8_t* img, int w, int h, int channels) const {
    if (w < 3 || h < 3 || channels < 1) return;
    float strength = config_.sharpen_strength;
    size_t total_len = static_cast<size_t>(w) * static_cast<size_t>(h) * static_cast<size_t>(channels);
    std::vector<uint8_t> out(img, img + total_len);

    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            size_t c_idx = (static_cast<size_t>(y) * w + x) * channels;
            size_t u_idx = (static_cast<size_t>(y - 1) * w + x) * channels;
            size_t d_idx = (static_cast<size_t>(y + 1) * w + x) * channels;
            size_t l_idx = (static_cast<size_t>(y) * w + (x - 1)) * channels;
            size_t r_idx = (static_cast<size_t>(y) * w + (x + 1)) * channels;
            for (int c = 0; c < channels; ++c) {
                int center = img[c_idx + c];
                int neighbors = img[u_idx + c] + img[d_idx + c] + img[l_idx + c] + img[r_idx + c];
                float high_pass = center - (neighbors * 0.25f);
                float sharpened = center + strength * high_pass;
                out[c_idx + c] = static_cast<uint8_t>(std::clamp(sharpened, 0.0f, 255.0f));
            }
        }
    }

    std::copy(out.begin(), out.end(), img);
}

} // namespace sabit::imaging
