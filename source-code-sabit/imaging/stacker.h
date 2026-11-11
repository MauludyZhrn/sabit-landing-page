#ifndef SABIT_IMAGING_STACKER_H
#define SABIT_IMAGING_STACKER_H

#include <deque>
#include <vector>
#include <memory>
#include <cstdint>
#include <mutex>
#include "src/core/frame.h"
#include "src/core/result.h"

namespace sabit::imaging {

enum class StackMethod {
    Mean = 0,
    Sum = 1,
    Median = 2,
    SigmaClip = 3
};

struct StackerConfig {
    StackMethod method{StackMethod::Mean};
    int max_frames{8};
    float k_sigma{2.0f};
    size_t stack_ram_mb{2048};
    bool align_enabled{false};
};

struct StackerStatus {
    int frames_in_window{0};
    uint64_t total_frames_processed{0};
    size_t current_ram_bytes{0};
    bool ram_limit_reached{false};
    int align_offset_x{0};
    int align_offset_y{0};
};

class LiveStacker {
public:
    explicit LiveStacker(StackerConfig config = {});
    ~LiveStacker() = default;

    void set_config(const StackerConfig& config);
    [[nodiscard]] StackerConfig config() const;

    void reset();

    /**
     * Feed a new frame into the stacker and compute the updated stacked frame.
     * Enforces stack_ram_mb limit by dropping oldest frames when memory exceeded.
     * Returns the stacked 16-bit frame.
     */
    core::Frame process_frame(const core::Frame& incoming);

    [[nodiscard]] StackerStatus status() const;

    /**
     * Perform batch / offline stacking on a collection of frames.
     */
    static core::Result<core::Frame> stack_frames(
        const std::vector<core::Frame>& frames,
        StackMethod method,
        float k_sigma = 2.0f,
        bool align_enabled = true
    );

private:
    void enforce_memory_limit();
    std::pair<int, int> estimate_alignment(const core::Frame& ref, const core::Frame& cur);

    // Stacking compute kernels
    core::Frame compute_mean(int w, int h, int ch);
    core::Frame compute_sum(int w, int h, int ch);
    core::Frame compute_median(int w, int h, int ch);
    core::Frame compute_sigma_clip(int w, int h, int ch, float k);

    StackerConfig config_;
    std::deque<core::Frame> window_;
    std::vector<std::pair<int, int>> offsets_; // Translation offset per window frame
    uint64_t total_frames_{0};
    bool ram_limit_hit_{false};
    mutable std::mutex mutex_;
};

} // namespace sabit::imaging

#endif // SABIT_IMAGING_STACKER_H
