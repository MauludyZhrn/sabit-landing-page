#ifndef SABIT_IMAGING_IO_AVI_WRITER_H
#define SABIT_IMAGING_IO_AVI_WRITER_H

#include <string>
#include <fstream>
#include <vector>
#include <cstdint>
#include "src/core/frame.h"
#include "src/core/result.h"

namespace sabit::imaging::io {

class AviWriter {
public:
    AviWriter() = default;
    ~AviWriter();

    core::Result<void> open(const std::string& file_path, int width, int height, double fps = 30.0);
    core::Result<void> write_frame(const core::Frame& frame);
    void close();

    [[nodiscard]] bool is_open() const { return is_open_; }
    [[nodiscard]] uint32_t frame_count() const { return frame_count_; }

private:
    struct AviIndexEntry {
        uint32_t ckid{0};
        uint32_t flags{0};
        uint32_t offset{0};
        uint32_t size{0};
    };

    std::string file_path_;
    std::ofstream file_;
    int width_{0};
    int height_{0};
    double fps_{30.0};
    uint32_t frame_count_{0};
    uint32_t row_stride_{0};
    uint32_t frame_data_size_{0};
    bool is_open_{false};

    std::streampos pos_riff_size_{0};
    std::streampos pos_avih_frames_{0};
    std::streampos pos_strh_frames_{0};
    std::streampos pos_movi_size_{0};
    std::streampos pos_movi_data_start_{0};

    std::vector<AviIndexEntry> index_entries_;
    std::vector<uint8_t> frame_buffer_;
};

} // namespace sabit::imaging::io

#endif // SABIT_IMAGING_IO_AVI_WRITER_H
