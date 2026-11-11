#ifndef SABIT_CORE_SAFE_SIZE_H
#define SABIT_CORE_SAFE_SIZE_H

#include <cstdint>
#include <cstddef>
#include <optional>

namespace sabit::core {

// 09b-Security-Plan.md §c: anti-overflow size check before any allocation
// derived from untrusted file headers (FITS/TIFF/PGM/PNG calibration files).
inline std::optional<size_t> frame_bytes(int64_t w, int64_t h, int64_t ch, int64_t bytes_per_px) {
    constexpr uint64_t kMax = 1ull << 31; // 2 GiB
    if (w <= 0 || h <= 0 || ch <= 0 || bytes_per_px <= 0) return std::nullopt;
    if (w > 32768 || h > 32768 || ch > 4 || bytes_per_px > 8) return std::nullopt;
    const uint64_t n = uint64_t(w) * uint64_t(h) * uint64_t(ch) * uint64_t(bytes_per_px);
    if (n > kMax) return std::nullopt;
    return static_cast<size_t>(n);
}

} // namespace sabit::core

#endif // SABIT_CORE_SAFE_SIZE_H
