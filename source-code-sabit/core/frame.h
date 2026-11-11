#ifndef SABIT_CORE_FRAME_H
#define SABIT_CORE_FRAME_H

#include <cstdint>
#include <memory>
#include <limits>
#include <vector>
#include <mutex>
#include "src/core/clock.h"

namespace sabit::core {

enum class BayerPattern {
    None = 0,
    RGGB,
    BGGR,
    GRBG,
    GBRG
};

inline const char* bayer_pattern_to_string(BayerPattern pattern) {
    switch (pattern) {
        case BayerPattern::RGGB: return "RGGB";
        case BayerPattern::BGGR: return "BGGR";
        case BayerPattern::GRBG: return "GRBG";
        case BayerPattern::GBRG: return "GBRG";
        case BayerPattern::None:
        default: return "None";
    }
}

class FramePool;

struct Frame {
    uint64_t seq{0};
    int width{0};
    int height{0};
    int channels{1}; // 1 = mono/raw, 3 = RGB
    int bit_depth{16}; // 8, 10, 12, 14, 16
    BayerPattern bayer{BayerPattern::None};
    std::shared_ptr<uint16_t[]> pixels{nullptr};

    // Hardware metadata
    int64_t exposure_us{0};
    double gain{0.0};
    double sensor_temp_c{std::numeric_limits<double>::quiet_NaN()};
    int binning{1};
    Clock::TimePoint timestamp_utc{};
    ClockSync clock_sync{};

    [[nodiscard]] size_t pixel_count() const noexcept {
        return static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels);
    }

    [[nodiscard]] size_t byte_size() const noexcept {
        return pixel_count() * sizeof(uint16_t);
    }

    [[nodiscard]] bool is_valid() const noexcept {
        return width > 0 && height > 0 && channels > 0 && pixels != nullptr;
    }

    static Frame create_allocated(int width, int height, int channels = 1, int bit_depth = 16);
    static Frame create_from_pool(FramePool& pool, int width, int height, int channels = 1, int bit_depth = 16);
};

class FramePool : public std::enable_shared_from_this<FramePool> {
public:
    explicit FramePool(size_t buffer_element_count, size_t initial_pool_size = 16, size_t max_pool_size = 64);
    ~FramePool() = default;

    std::shared_ptr<uint16_t[]> acquire();

    [[nodiscard]] size_t buffer_element_count() const noexcept { return element_count_; }
    [[nodiscard]] size_t available_count() const;
    [[nodiscard]] size_t total_count() const;

private:
    void release(uint16_t* ptr);

    size_t element_count_;
    size_t max_size_;
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<uint16_t[]>> free_list_;
    size_t total_allocated_{0};
};

} // namespace sabit::core

#endif // SABIT_CORE_FRAME_H
