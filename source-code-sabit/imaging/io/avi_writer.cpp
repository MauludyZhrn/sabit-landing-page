#include "src/imaging/io/avi_writer.h"
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace sabit::imaging::io {

static inline void write_fourcc(std::ostream& os, const char* fcc) {
    os.write(fcc, 4);
}

static inline void write_u32_le(std::ostream& os, uint32_t val) {
    char b[4] = {
        static_cast<char>(val & 0xFF),
        static_cast<char>((val >> 8) & 0xFF),
        static_cast<char>((val >> 16) & 0xFF),
        static_cast<char>((val >> 24) & 0xFF)
    };
    os.write(b, 4);
}

static inline void write_u16_le(std::ostream& os, uint16_t val) {
    char b[2] = {
        static_cast<char>(val & 0xFF),
        static_cast<char>((val >> 8) & 0xFF)
    };
    os.write(b, 2);
}

AviWriter::~AviWriter() {
    close();
}

core::Result<void> AviWriter::open(const std::string& file_path, int width, int height, double fps) {
    close();

    if (width <= 0 || height <= 0) {
        return core::Result<void>::err(core::ErrorCode::OutOfRange, "Dimensi video tidak valid untuk AVI");
    }

    try {
        std::filesystem::path p(file_path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (const std::exception& e) {
        return core::Result<void>::err(core::ErrorCode::SerializationError, std::string("Gagal membuat direktori AVI: ") + e.what());
    }

    file_.open(file_path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        return core::Result<void>::err(core::ErrorCode::SerializationError, "Gagal membuka file AVI untuk penulisan");
    }

    file_path_ = file_path;
    width_ = width;
    height_ = height;
    fps_ = (fps > 0.0) ? fps : 30.0;
    frame_count_ = 0;
    row_stride_ = ((width_ * 3 + 3) / 4) * 4;
    frame_data_size_ = row_stride_ * height_;
    frame_buffer_.assign(frame_data_size_, 0);
    index_entries_.clear();

    uint32_t us_per_frame = static_cast<uint32_t>(1000000.0 / fps_);

    // 1. RIFF Header
    write_fourcc(file_, "RIFF");
    pos_riff_size_ = file_.tellp();
    write_u32_le(file_, 0); // Placeholder for total file size - 8
    write_fourcc(file_, "AVI ");

    // 2. LIST 'hdrl'
    write_fourcc(file_, "LIST");
    // Size of hdrl: 4 ('hdrl') + 8 + 56 ('avih') + 12 + 8 + 56 ('strh') + 8 + 40 ('strf') = 192 bytes
    uint32_t hdrl_size = 4 + (8 + 56) + (8 + 4 + (8 + 56) + (8 + 40));
    write_u32_le(file_, hdrl_size);
    write_fourcc(file_, "hdrl");

    // 2.1 'avih' chunk (Main AVI Header - 56 bytes)
    write_fourcc(file_, "avih");
    write_u32_le(file_, 56);
    write_u32_le(file_, us_per_frame);       // dwMicroSecPerFrame
    write_u32_le(file_, frame_data_size_ * static_cast<uint32_t>(fps_)); // dwMaxBytesPerSec
    write_u32_le(file_, 0);                  // dwPaddingGranularity
    write_u32_le(file_, 0x10);               // dwFlags: AVIF_HASINDEX
    pos_avih_frames_ = file_.tellp();
    write_u32_le(file_, 0);                  // dwTotalFrames (placeholder)
    write_u32_le(file_, 0);                  // dwInitialFrames
    write_u32_le(file_, 1);                  // dwStreams (video only)
    write_u32_le(file_, frame_data_size_);   // dwSuggestedBufferSize
    write_u32_le(file_, static_cast<uint32_t>(width_));  // dwWidth
    write_u32_le(file_, static_cast<uint32_t>(height_)); // dwHeight
    write_u32_le(file_, 0);                  // dwReserved[0]
    write_u32_le(file_, 0);                  // dwReserved[1]
    write_u32_le(file_, 0);                  // dwReserved[2]
    write_u32_le(file_, 0);                  // dwReserved[3]

    // 2.2 LIST 'strl' (Stream Header List)
    write_fourcc(file_, "LIST");
    uint32_t strl_size = 4 + (8 + 56) + (8 + 40);
    write_u32_le(file_, strl_size);
    write_fourcc(file_, "strl");

    // 2.2.1 'strh' chunk (Stream Header - 56 bytes)
    write_fourcc(file_, "strh");
    write_u32_le(file_, 56);
    write_fourcc(file_, "vids");             // fccType: video stream
    write_fourcc(file_, "DIB ");             // fccHandler: uncompressed DIB
    write_u32_le(file_, 0);                  // dwFlags
    write_u16_le(file_, 0);                  // wPriority
    write_u16_le(file_, 0);                  // wLanguage
    write_u32_le(file_, 0);                  // dwInitialFrames
    write_u32_le(file_, 1000);               // dwScale
    write_u32_le(file_, static_cast<uint32_t>(fps_ * 1000.0)); // dwRate (fps = dwRate / dwScale)
    write_u32_le(file_, 0);                  // dwStart
    pos_strh_frames_ = file_.tellp();
    write_u32_le(file_, 0);                  // dwLength (placeholder)
    write_u32_le(file_, frame_data_size_);   // dwSuggestedBufferSize
    write_u32_le(file_, 0xFFFFFFFF);         // dwQuality (-1 default)
    write_u32_le(file_, frame_data_size_);   // dwSampleSize
    write_u16_le(file_, 0);                  // rcFrame.left
    write_u16_le(file_, 0);                  // rcFrame.top
    write_u16_le(file_, static_cast<uint16_t>(width_));  // rcFrame.right
    write_u16_le(file_, static_cast<uint16_t>(height_)); // rcFrame.bottom

    // 2.2.2 'strf' chunk (BITMAPINFOHEADER - 40 bytes)
    write_fourcc(file_, "strf");
    write_u32_le(file_, 40);
    write_u32_le(file_, 40);                 // biSize
    write_u32_le(file_, static_cast<uint32_t>(width_));
    write_u32_le(file_, static_cast<uint32_t>(height_));
    write_u16_le(file_, 1);                  // biPlanes
    write_u16_le(file_, 24);                 // biBitCount (24-bit BGR)
    write_u32_le(file_, 0);                  // biCompression: BI_RGB (uncompressed)
    write_u32_le(file_, frame_data_size_);   // biSizeImage
    write_u32_le(file_, 0);                  // biXPelsPerMeter
    write_u32_le(file_, 0);                  // biYPelsPerMeter
    write_u32_le(file_, 0);                  // biClrUsed
    write_u32_le(file_, 0);                  // biClrImportant

    // 3. LIST 'movi'
    write_fourcc(file_, "LIST");
    pos_movi_size_ = file_.tellp();
    write_u32_le(file_, 0); // Placeholder for movi size
    write_fourcc(file_, "movi");
    pos_movi_data_start_ = file_.tellp(); // Base for offsets in index

    is_open_ = true;
    return core::Result<void>::ok();
}

core::Result<void> AviWriter::write_frame(const core::Frame& frame) {
    if (!is_open_) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "File AVI belum dibuka");
    }

    if (frame.width != width_ || frame.height != height_) {
        return core::Result<void>::err(core::ErrorCode::OutOfRange, "Dimensi frame tidak cocok dengan konfigurasi AVI");
    }

    // Windows DIB expects bottom-up row order: row y=height-1 is at the start of the buffer
    std::fill(frame_buffer_.begin(), frame_buffer_.end(), 0);

    const uint16_t* src_u16 = frame.pixels ? frame.pixels.get() : nullptr;
    const bool is_16bit = (frame.bit_depth > 8);
    const int ch = std::max(1, frame.channels);

    auto sample_to_u8 = [is_16bit](uint16_t v) -> uint8_t {
        if (is_16bit || v > 255) {
            return static_cast<uint8_t>(v >> 8);
        }
        return static_cast<uint8_t>(v);
    };

    for (int y = 0; y < height_; ++y) {
        int src_y = y; // Source row (top-to-bottom)
        int dst_y = (height_ - 1 - y); // DIB row (bottom-to-top)
        uint8_t* dst_row = frame_buffer_.data() + (dst_y * row_stride_);

        for (int x = 0; x < width_; ++x) {
            uint8_t b = 0, g = 0, r = 0;
            size_t px_idx = (static_cast<size_t>(src_y) * static_cast<size_t>(width_) + static_cast<size_t>(x)) * static_cast<size_t>(ch);

            if (src_u16) {
                if (ch == 1) {
                    uint8_t val = sample_to_u8(src_u16[px_idx]);
                    b = g = r = val;
                } else if (ch >= 3) {
                    r = sample_to_u8(src_u16[px_idx + 0]);
                    g = sample_to_u8(src_u16[px_idx + 1]);
                    b = sample_to_u8(src_u16[px_idx + 2]);
                }
            }

            // Write 24-bit BGR
            dst_row[x * 3 + 0] = b;
            dst_row[x * 3 + 1] = g;
            dst_row[x * 3 + 2] = r;
        }
    }

    std::streampos chunk_pos = file_.tellp();
    uint32_t rel_offset = static_cast<uint32_t>(static_cast<std::streamoff>(chunk_pos) - static_cast<std::streamoff>(pos_movi_data_start_));

    write_fourcc(file_, "00db");
    write_u32_le(file_, frame_data_size_);
    file_.write(reinterpret_cast<const char*>(frame_buffer_.data()), frame_data_size_);

    AviIndexEntry entry;
    entry.ckid = 0x62643030; // '00db' in little-endian
    entry.flags = 0x10;       // AVIIF_KEYFRAME
    entry.offset = rel_offset;
    entry.size = frame_data_size_;
    index_entries_.push_back(entry);

    ++frame_count_;
    return core::Result<void>::ok();
}

void AviWriter::close() {
    if (!is_open_) return;

    std::streampos movi_end = file_.tellp();
    uint32_t movi_size = static_cast<uint32_t>(static_cast<std::streamoff>(movi_end) - static_cast<std::streamoff>(pos_movi_size_) - 4);

    // 4. Write 'idx1' chunk
    write_fourcc(file_, "idx1");
    uint32_t idx_size = static_cast<uint32_t>(index_entries_.size() * 16);
    write_u32_le(file_, idx_size);
    for (const auto& entry : index_entries_) {
        write_u32_le(file_, entry.ckid);
        write_u32_le(file_, entry.flags);
        write_u32_le(file_, entry.offset);
        write_u32_le(file_, entry.size);
    }

    std::streampos file_end = file_.tellp();
    uint32_t riff_size = static_cast<uint32_t>(static_cast<std::streamoff>(file_end) - 8);

    // Seek back and update header placeholders
    file_.seekp(pos_riff_size_);
    write_u32_le(file_, riff_size);

    file_.seekp(pos_avih_frames_);
    write_u32_le(file_, frame_count_);

    file_.seekp(pos_strh_frames_);
    write_u32_le(file_, frame_count_);

    file_.seekp(pos_movi_size_);
    write_u32_le(file_, movi_size);

    file_.flush();
    file_.close();
    is_open_ = false;
}

} // namespace sabit::imaging::io
