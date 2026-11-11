#include "src/imaging/stacker.h"
#include <algorithm>
#include <cmath>
#include <complex>

namespace sabit::imaging {

namespace {

constexpr double kPi = 3.14159265358979323846;

// In-place 1D Cooley-Tukey Radix-2 FFT
void fft_1d(std::complex<double>* a, int n, bool invert) {
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        double ang = 2.0 * kPi / len * (invert ? 1.0 : -1.0);
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (int j = 0; j < len / 2; ++j) {
                std::complex<double> u = a[i + j];
                std::complex<double> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (invert) {
        for (int i = 0; i < n; ++i) {
            a[i] /= static_cast<double>(n);
        }
    }
}

// In-place 2D Radix-2 FFT on an N x N buffer
void fft_2d(std::vector<std::complex<double>>& data, int n, bool invert) {
    // Rows
    for (int y = 0; y < n; ++y) {
        fft_1d(&data[static_cast<size_t>(y) * n], n, invert);
    }
    // Columns
    std::vector<std::complex<double>> col(n);
    for (int x = 0; x < n; ++x) {
        for (int y = 0; y < n; ++y) {
            col[y] = data[static_cast<size_t>(y) * n + x];
        }
        fft_1d(col.data(), n, invert);
        for (int y = 0; y < n; ++y) {
            data[static_cast<size_t>(y) * n + x] = col[y];
        }
    }
}

inline double sample_luminance(const core::Frame& f, int x, int y) {
    const uint16_t* p = f.pixels.get();
    size_t idx = (static_cast<size_t>(y) * f.width + x) * f.channels;
    if (f.channels >= 3) {
        return 0.299 * p[idx + 0] + 0.587 * p[idx + 1] + 0.114 * p[idx + 2];
    }
    return static_cast<double>(p[idx]);
}

} // namespace

LiveStacker::LiveStacker(StackerConfig config) : config_(config) {}

void LiveStacker::set_config(const StackerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    enforce_memory_limit();
}

StackerConfig LiveStacker::config() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void LiveStacker::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    window_.clear();
    offsets_.clear();
    total_frames_ = 0;
    ram_limit_hit_ = false;
}

std::pair<int, int> LiveStacker::estimate_alignment(const core::Frame& ref, const core::Frame& cur) {
    if (!config_.align_enabled || !ref.is_valid() || !cur.is_valid() ||
        ref.width != cur.width || ref.height != cur.height) {
        return {0, 0};
    }

    int min_dim = std::min(ref.width, ref.height);
    int fft_n = 0;
    for (int candidate : {128, 64, 32, 16, 8}) {
        if (min_dim >= candidate) {
            fft_n = candidate;
            break;
        }
    }

    // 1. 2D FFT Phase Correlation on central N x N patch
    if (fft_n >= 8) {
        int x0 = (ref.width - fft_n) / 2;
        int y0 = (ref.height - fft_n) / 2;

        std::vector<double> ref_patch(static_cast<size_t>(fft_n) * fft_n, 0.0);
        std::vector<double> cur_patch(static_cast<size_t>(fft_n) * fft_n, 0.0);
        double ref_mean = 0.0;
        double cur_mean = 0.0;

        for (int y = 0; y < fft_n; ++y) {
            for (int x = 0; x < fft_n; ++x) {
                double rv = sample_luminance(ref, x0 + x, y0 + y);
                double cv = sample_luminance(cur, x0 + x, y0 + y);
                ref_patch[static_cast<size_t>(y) * fft_n + x] = rv;
                cur_patch[static_cast<size_t>(y) * fft_n + x] = cv;
                ref_mean += rv;
                cur_mean += cv;
            }
        }

        double inv_area = 1.0 / static_cast<double>(fft_n * fft_n);
        ref_mean *= inv_area;
        cur_mean *= inv_area;

        std::vector<double> hann(fft_n);
        for (int i = 0; i < fft_n; ++i) {
            hann[i] = 0.5 * (1.0 - std::cos(2.0 * kPi * i / (fft_n - 1)));
        }

        std::vector<std::complex<double>> f_ref(static_cast<size_t>(fft_n) * fft_n);
        std::vector<std::complex<double>> f_cur(static_cast<size_t>(fft_n) * fft_n);
        double ref_energy = 0.0;
        double cur_energy = 0.0;

        for (int y = 0; y < fft_n; ++y) {
            for (int x = 0; x < fft_n; ++x) {
                size_t idx = static_cast<size_t>(y) * fft_n + x;
                double w = hann[y] * hann[x];
                double r_zero = (ref_patch[idx] - ref_mean) * w;
                double c_zero = (cur_patch[idx] - cur_mean) * w;
                f_ref[idx] = std::complex<double>(r_zero, 0.0);
                f_cur[idx] = std::complex<double>(c_zero, 0.0);
                ref_energy += r_zero * r_zero;
                cur_energy += c_zero * c_zero;
            }
        }

        if (ref_energy > 1e-6 && cur_energy > 1e-6) {
            fft_2d(f_ref, fft_n, false);
            fft_2d(f_cur, fft_n, false);

            // Normalized Cross-Power Spectrum: C = (F_cur * conj(F_ref)) / |F_cur * conj(F_ref)|
            std::vector<std::complex<double>> cps(static_cast<size_t>(fft_n) * fft_n);
            for (size_t i = 0; i < cps.size(); ++i) {
                std::complex<double> prod = f_cur[i] * std::conj(f_ref[i]);
                double mag = std::abs(prod);
                cps[i] = (mag > 1e-9) ? (prod / mag) : std::complex<double>(0.0, 0.0);
            }

            fft_2d(cps, fft_n, true);

            double max_peak = -1.0;
            int peak_x = 0;
            int peak_y = 0;
            for (int y = 0; y < fft_n; ++y) {
                for (int x = 0; x < fft_n; ++x) {
                    double val = cps[static_cast<size_t>(y) * fft_n + x].real();
                    if (val > max_peak) {
                        max_peak = val;
                        peak_x = x;
                        peak_y = y;
                    }
                }
            }

            int dx = (peak_x <= fft_n / 2) ? peak_x : (peak_x - fft_n);
            int dy = (peak_y <= fft_n / 2) ? peak_y : (peak_y - fft_n);
            return {std::clamp(dx, -50, 50), std::clamp(dy, -50, 50)};
        }
    }

    // 2. Fallback centroid on central ROI
    int roi_x0 = ref.width / 4;
    int roi_x1 = (ref.width * 3) / 4;
    int roi_y0 = ref.height / 4;
    int roi_y1 = (ref.height * 3) / 4;

    double ref_sum = 0.0, ref_cx = 0.0, ref_cy = 0.0;
    double cur_sum = 0.0, cur_cx = 0.0, cur_cy = 0.0;

    for (int y = roi_y0; y < roi_y1; ++y) {
        for (int x = roi_x0; x < roi_x1; ++x) {
            double r_val = sample_luminance(ref, x, y);
            double c_val = sample_luminance(cur, x, y);

            ref_sum += r_val;
            ref_cx += x * r_val;
            ref_cy += y * r_val;

            cur_sum += c_val;
            cur_cx += x * c_val;
            cur_cy += y * c_val;
        }
    }

    if (ref_sum <= 0.0 || cur_sum <= 0.0) return {0, 0};

    ref_cx /= ref_sum;
    ref_cy /= ref_sum;
    cur_cx /= cur_sum;
    cur_cy /= cur_sum;

    int dx = std::clamp(static_cast<int>(std::round(cur_cx - ref_cx)), -50, 50);
    int dy = std::clamp(static_cast<int>(std::round(cur_cy - ref_cy)), -50, 50);
    return {dx, dy};
}

void LiveStacker::enforce_memory_limit() {
    size_t max_bytes = config_.stack_ram_mb * 1024ULL * 1024ULL;
    size_t current_bytes = 0;
    for (const auto& f : window_) {
        current_bytes += f.byte_size();
    }

    while (!window_.empty() && (current_bytes > max_bytes || window_.size() > static_cast<size_t>(config_.max_frames))) {
        current_bytes -= window_.front().byte_size();
        window_.pop_front();
        if (!offsets_.empty()) {
            offsets_.erase(offsets_.begin());
        }
        ram_limit_hit_ = true;
    }
}

core::Frame LiveStacker::process_frame(const core::Frame& incoming) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!incoming.is_valid()) {
        return incoming;
    }

    // Verify dimension consistency with existing window
    if (!window_.empty()) {
        if (incoming.width != window_.front().width || incoming.height != window_.front().height ||
            incoming.channels != window_.front().channels) {
            window_.clear();
            offsets_.clear();
        }
    }

    // Deep copy incoming frame data into window
    core::Frame copy_frame = core::Frame::create_allocated(incoming.width, incoming.height, incoming.channels, incoming.bit_depth);
    copy_frame.seq = incoming.seq;
    copy_frame.exposure_us = incoming.exposure_us;
    copy_frame.gain = incoming.gain;
    copy_frame.sensor_temp_c = incoming.sensor_temp_c;
    copy_frame.binning = incoming.binning;
    copy_frame.timestamp_utc = incoming.timestamp_utc;
    copy_frame.clock_sync = incoming.clock_sync;

    size_t count = incoming.pixel_count();
    std::copy_n(incoming.pixels.get(), count, copy_frame.pixels.get());

    std::pair<int, int> offset{0, 0};
    if (!window_.empty()) {
        offset = estimate_alignment(window_.front(), copy_frame);
    }

    window_.push_back(std::move(copy_frame));
    offsets_.push_back(offset);
    ++total_frames_;

    enforce_memory_limit();

    int w = incoming.width;
    int h = incoming.height;
    int ch = incoming.channels;

    core::Frame result;
    switch (config_.method) {
        case StackMethod::Sum:
            result = compute_sum(w, h, ch);
            break;
        case StackMethod::Median:
            result = compute_median(w, h, ch);
            break;
        case StackMethod::SigmaClip:
            result = compute_sigma_clip(w, h, ch, config_.k_sigma);
            break;
        case StackMethod::Mean:
        default:
            result = compute_mean(w, h, ch);
            break;
    }

    result.seq = incoming.seq;
    result.exposure_us = incoming.exposure_us * static_cast<int64_t>(window_.size());
    result.gain = incoming.gain;
    result.sensor_temp_c = incoming.sensor_temp_c;
    result.binning = incoming.binning;
    result.bit_depth = incoming.bit_depth;
    result.timestamp_utc = incoming.timestamp_utc;
    result.clock_sync = incoming.clock_sync;
    return result;
}

core::Frame LiveStacker::compute_mean(int w, int h, int ch) {
    core::Frame out = core::Frame::create_allocated(w, h, ch, 16);
    size_t total_px = out.pixel_count();
    uint16_t* dst = out.pixels.get();

    size_t n = window_.size();
    if (n == 0) return out;
    if (n == 1) {
        std::copy_n(window_.front().pixels.get(), total_px, dst);
        return out;
    }

    std::vector<float> accum(total_px, 0.0f);
    for (size_t f = 0; f < n; ++f) {
        const uint16_t* src = window_[f].pixels.get();
        int ox = offsets_[f].first;
        int oy = offsets_[f].second;

        if (ox == 0 && oy == 0) {
            for (size_t i = 0; i < total_px; ++i) {
                accum[i] += src[i];
            }
        } else {
            for (int y = 0; y < h; ++y) {
                int sy = std::clamp(y + oy, 0, h - 1);
                for (int x = 0; x < w; ++x) {
                    int sx = std::clamp(x + ox, 0, w - 1);
                    size_t dst_base = (static_cast<size_t>(y) * w + x) * ch;
                    size_t src_base = (static_cast<size_t>(sy) * w + sx) * ch;
                    for (int c = 0; c < ch; ++c) {
                        accum[dst_base + c] += src[src_base + c];
                    }
                }
            }
        }
    }

    float inv_n = 1.0f / static_cast<float>(n);
    for (size_t i = 0; i < total_px; ++i) {
        float val = accum[i] * inv_n;
        dst[i] = static_cast<uint16_t>(std::clamp(val + 0.5f, 0.0f, 65535.0f));
    }

    return out;
}

core::Frame LiveStacker::compute_sum(int w, int h, int ch) {
    core::Frame out = core::Frame::create_allocated(w, h, ch, 16);
    size_t total_px = out.pixel_count();
    uint16_t* dst = out.pixels.get();

    size_t n = window_.size();
    if (n == 0) return out;

    std::vector<double> accum(total_px, 0.0);
    for (size_t f = 0; f < n; ++f) {
        const uint16_t* src = window_[f].pixels.get();
        int ox = offsets_[f].first;
        int oy = offsets_[f].second;

        if (ox == 0 && oy == 0) {
            for (size_t i = 0; i < total_px; ++i) {
                accum[i] += src[i];
            }
        } else {
            for (int y = 0; y < h; ++y) {
                int sy = std::clamp(y + oy, 0, h - 1);
                for (int x = 0; x < w; ++x) {
                    int sx = std::clamp(x + ox, 0, w - 1);
                    size_t dst_base = (static_cast<size_t>(y) * w + x) * ch;
                    size_t src_base = (static_cast<size_t>(sy) * w + sx) * ch;
                    for (int c = 0; c < ch; ++c) {
                        accum[dst_base + c] += src[src_base + c];
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < total_px; ++i) {
        dst[i] = static_cast<uint16_t>(std::min(accum[i], 65535.0));
    }

    return out;
}

core::Frame LiveStacker::compute_median(int w, int h, int ch) {
    core::Frame out = core::Frame::create_allocated(w, h, ch, 16);
    size_t total_px = out.pixel_count();
    uint16_t* dst = out.pixels.get();

    size_t n = window_.size();
    if (n == 0) return out;
    if (n == 1) {
        std::copy_n(window_.front().pixels.get(), total_px, dst);
        return out;
    }

    std::vector<uint16_t> sample(n);
    size_t mid = n / 2;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t dst_base = (static_cast<size_t>(y) * w + x) * ch;
            for (int c = 0; c < ch; ++c) {
                for (size_t f = 0; f < n; ++f) {
                    int sy = std::clamp(y + offsets_[f].second, 0, h - 1);
                    int sx = std::clamp(x + offsets_[f].first, 0, w - 1);
                    sample[f] = window_[f].pixels[(static_cast<size_t>(sy) * w + sx) * ch + c];
                }
                std::nth_element(sample.begin(), sample.begin() + mid, sample.end());
                dst[dst_base + c] = sample[mid];
            }
        }
    }

    return out;
}

core::Frame LiveStacker::compute_sigma_clip(int w, int h, int ch, float k) {
    core::Frame out = core::Frame::create_allocated(w, h, ch, 16);
    uint16_t* dst = out.pixels.get();

    size_t n = window_.size();
    if (n == 0) return out;
    if (n <= 2) {
        return compute_mean(w, h, ch);
    }

    std::vector<float> sample(n);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t dst_base = (static_cast<size_t>(y) * w + x) * ch;
            for (int c = 0; c < ch; ++c) {
                double sum = 0.0;
                for (size_t f = 0; f < n; ++f) {
                    int sy = std::clamp(y + offsets_[f].second, 0, h - 1);
                    int sx = std::clamp(x + offsets_[f].first, 0, w - 1);
                    float val = static_cast<float>(window_[f].pixels[(static_cast<size_t>(sy) * w + sx) * ch + c]);
                    sample[f] = val;
                    sum += val;
                }

                double mean = sum / static_cast<double>(n);
                double variance_sum = 0.0;
                for (size_t f = 0; f < n; ++f) {
                    double diff = sample[f] - mean;
                    variance_sum += diff * diff;
                }
                double sigma = std::sqrt(variance_sum / static_cast<double>(n));

                if (sigma < 1e-4) {
                    dst[dst_base + c] = static_cast<uint16_t>(std::clamp(mean + 0.5, 0.0, 65535.0));
                    continue;
                }

                double inlier_sum = 0.0;
                int inliers = 0;
                double threshold = k * sigma;

                for (size_t f = 0; f < n; ++f) {
                    if (std::abs(sample[f] - mean) <= threshold) {
                        inlier_sum += sample[f];
                        ++inliers;
                    }
                }

                if (inliers > 0) {
                    double clipped_mean = inlier_sum / static_cast<double>(inliers);
                    dst[dst_base + c] = static_cast<uint16_t>(std::clamp(clipped_mean + 0.5, 0.0, 65535.0));
                } else {
                    dst[dst_base + c] = static_cast<uint16_t>(std::clamp(mean + 0.5, 0.0, 65535.0));
                }
            }
        }
    }

    return out;
}

StackerStatus LiveStacker::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    StackerStatus s;
    s.frames_in_window = static_cast<int>(window_.size());
    s.total_frames_processed = total_frames_;
    s.ram_limit_reached = ram_limit_hit_;
    s.current_ram_bytes = 0;
    for (const auto& f : window_) {
        s.current_ram_bytes += f.byte_size();
    }
    if (!offsets_.empty()) {
        s.align_offset_x = offsets_.back().first;
        s.align_offset_y = offsets_.back().second;
    }
    return s;
}

core::Result<core::Frame> LiveStacker::stack_frames(
    const std::vector<core::Frame>& frames,
    StackMethod method,
    float k_sigma,
    bool align_enabled
) {
    if (frames.empty()) {
        return core::Result<core::Frame>::err(core::ErrorCode::InvalidState, "Tidak ada frame untuk di-stack");
    }

    StackerConfig cfg;
    cfg.method = method;
    cfg.max_frames = static_cast<int>(frames.size());
    cfg.k_sigma = k_sigma;
    cfg.stack_ram_mb = 8192;
    cfg.align_enabled = align_enabled;

    LiveStacker stacker(cfg);
    core::Frame stacked;
    for (const auto& f : frames) {
        if (!f.is_valid()) continue;
        stacked = stacker.process_frame(f);
    }

    if (!stacked.is_valid()) {
        return core::Result<core::Frame>::err(core::ErrorCode::InvalidState, "Gagal melakukan batch stacking frame");
    }

    return core::Result<core::Frame>::ok(std::move(stacked));
}

} // namespace sabit::imaging
