#ifndef SABIT_IMAGING_IO_MP4_WRITER_H
#define SABIT_IMAGING_IO_MP4_WRITER_H

#include <string>
#include <fstream>
#include <vector>
#include <cstdint>
#include "src/core/frame.h"
#include "src/core/result.h"

namespace sabit::imaging::io {

/**
 * Native ISO Base Media File Format (.mp4 / MPEG-4 Part 14) video container writer.
 * Writes ftyp, mdat, and moov atoms with full sample tables (stsd, stts, stsc, stsz, stco).
 */
class Mp4Writer {
public:
    Mp4Writer() = default;
    ~Mp4Writer();

    core::Result<void> open(const std::string& file_path, int width, int height, double fps = 30.0);
    core::Result<void> write_frame(const core::Frame& frame);
    void close();

    [[nodiscard]] bool is_open() const { return is_open_; }
    [[nodiscard]] uint32_t frame_count() const { return frame_count_; }

private:
    std::string file_path_;
    std::ofstream file_;
    int width_{0};
    int height_{0};
    double fps_{30.0};
    uint32_t frame_count_{0};
    uint32_t frame_data_size_{0};
    bool is_open_{false};

    std::streampos pos_mdat_size_{0};
    std::streampos pos_mdat_data_start_{0};

    std::vector<uint32_t> chunk_offsets_;
    std::vector<uint8_t> frame_buffer_;

    // Windows Media Foundation H.264 Sink Writer state (opaque pointer)
    void* mf_sink_writer_{nullptr};
    uint32_t mf_stream_index_{0};
    bool use_mf_h264_{false};
};

} // namespace sabit::imaging::io

#endif // SABIT_IMAGING_IO_MP4_WRITER_H
