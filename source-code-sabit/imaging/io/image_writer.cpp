#include "src/imaging/io/image_writer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "src/core/safe_size.h"

namespace sabit::imaging::io {

// Little-endian writing helpers
static inline void write_u16_le(std::ostream& os, uint16_t v) {
    char b[2] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF)};
    os.write(b, 2);
}

static inline void write_u32_le(std::ostream& os, uint32_t v) {
    char b[4] = {
        static_cast<char>(v & 0xFF),
        static_cast<char>((v >> 8) & 0xFF),
        static_cast<char>((v >> 16) & 0xFF),
        static_cast<char>((v >> 24) & 0xFF)
    };
    os.write(b, 4);
}

static inline void write_u32_be(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

static inline uint16_t read_u16_le(const char* p) {
    return static_cast<uint8_t>(p[0]) | (static_cast<uint16_t>(static_cast<uint8_t>(p[1])) << 8);
}

static inline uint32_t read_u32_le(const char* p) {
    return static_cast<uint8_t>(p[0]) |
           (static_cast<uint32_t>(static_cast<uint8_t>(p[1])) << 8) |
           (static_cast<uint32_t>(static_cast<uint8_t>(p[2])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(p[3])) << 24);
}

core::Result<void> ImageWriter::write_tiff_16bit(const std::string& file_path, const core::Frame& frame) {
    if (!frame.is_valid()) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "Cannot write invalid frame to TIFF");
    }

    std::filesystem::path final_path(file_path);
    if (final_path.has_parent_path()) {
        std::filesystem::create_directories(final_path.parent_path());
    }

    std::string tmp_path = file_path + ".tmp";
    std::ofstream ofs(tmp_path, std::ios::binary);
    if (!ofs.is_open()) {
        return core::Result<void>::err(core::ErrorCode::PermissionDenied, "Failed to open temporary TIFF file");
    }

    const uint32_t w = static_cast<uint32_t>(frame.width);
    const uint32_t h = static_cast<uint32_t>(frame.height);
    const uint16_t spp = (frame.channels >= 3) ? 3 : 1;
    const uint16_t photometric = (spp == 3) ? 2 : 1; // 2 = RGB, 1 = BlackIsZero
    const uint32_t num_samples = w * h * spp;
    const uint32_t strip_bytes = num_samples * 2;

    // Header: 'II' (little endian) + 42 + offset to IFD (8)
    ofs.write("II", 2);
    write_u16_le(ofs, 42);
    write_u32_le(ofs, 8); // IFD offset immediately follows header

    // IFD: 11 tags (includes PlanarConfiguration=1 for full RGB compatibility)
    uint16_t num_tags = 11;
    write_u16_le(ofs, num_tags);

    uint32_t ifd_size = 2 + num_tags * 12 + 4; // 2 + 132 + 4 = 138
    uint32_t extra_data_offset = 8 + ifd_size; // 146
    // RATIONAL XRes (8) + YRes (8) = 16 bytes; if spp==3, BitsPerSample array = 6 bytes (padded to 8)
    uint32_t bps_offset = extra_data_offset + 16;
    uint32_t pixel_data_offset = (spp == 3) ? (bps_offset + 6) : bps_offset;

    auto write_tag = [&](uint16_t tag, uint16_t type, uint32_t count, uint32_t val_or_offset) {
        write_u16_le(ofs, tag);
        write_u16_le(ofs, type);
        write_u32_le(ofs, count);
        write_u32_le(ofs, val_or_offset);
    };

    // Tag 256: ImageWidth
    write_tag(256, 4, 1, w);
    // Tag 257: ImageLength
    write_tag(257, 4, 1, h);
    // Tag 258: BitsPerSample (16 for 1ch, or offset to [16,16,16] for 3ch)
    if (spp == 1) {
        write_tag(258, 3, 1, 16);
    } else {
        write_tag(258, 3, 3, bps_offset);
    }
    // Tag 259: Compression (1 = uncompressed)
    write_tag(259, 3, 1, 1);
    // Tag 262: PhotometricInterpretation (1 = BlackIsZero, 2 = RGB)
    write_tag(262, 3, 1, photometric);
    // Tag 273: StripOffsets
    write_tag(273, 4, 1, pixel_data_offset);
    // Tag 277: SamplesPerPixel (1 or 3)
    write_tag(277, 3, 1, spp);
    // Tag 278: RowsPerStrip (h)
    write_tag(278, 4, 1, h);
    // Tag 279: StripByteCounts
    write_tag(279, 4, 1, strip_bytes);
    // Tag 284: PlanarConfiguration (1 = Chunky/Interleaved RGB)
    write_tag(284, 3, 1, 1);
    // Tag 296: ResolutionUnit (2 = inch)
    write_tag(296, 3, 1, 2);

    // Next IFD offset: 0 (no more IFDs)
    write_u32_le(ofs, 0);

    // Extra data: XResolution & YResolution (72 / 1)
    write_u32_le(ofs, 72);
    write_u32_le(ofs, 1);
    write_u32_le(ofs, 72);
    write_u32_le(ofs, 1);

    if (spp == 3) {
        write_u16_le(ofs, 16);
        write_u16_le(ofs, 16);
        write_u16_le(ofs, 16);
    }

    // Pixel data (16-bit Little-Endian)
    if (frame.bit_depth == 8) {
        const uint8_t* src8 = reinterpret_cast<const uint8_t*>(frame.pixels.get());
        std::vector<uint16_t> up16(num_samples);
        for (uint32_t i = 0; i < num_samples; ++i) {
            uint16_t v = src8[i];
            up16[i] = static_cast<uint16_t>((v << 8) | v);
        }
        ofs.write(reinterpret_cast<const char*>(up16.data()), strip_bytes);
    } else {
        const uint16_t* px = frame.pixels.get();
        ofs.write(reinterpret_cast<const char*>(px), strip_bytes);
    }
    ofs.flush();
    ofs.close();

    std::error_code ec;
    std::filesystem::rename(tmp_path, final_path, ec);
    if (ec) {
        return core::Result<void>::err(core::ErrorCode::HardwareError, "Atomic rename failed: " + ec.message());
    }

    return core::Result<void>::ok();
}

core::Result<core::Frame> ImageWriter::read_tiff_16bit(const std::string& file_path) {
    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs.is_open()) {
        return core::Result<core::Frame>::err(core::ErrorCode::NotFound, "Failed to open TIFF file");
    }

    char hdr[8];
    if (!ifs.read(hdr, 8) || (hdr[0] != 'I' || hdr[1] != 'I')) {
        return core::Result<core::Frame>::err(core::ErrorCode::SerializationError, "Not a valid little-endian TIFF file");
    }

    uint32_t ifd_offset = read_u32_le(hdr + 4);
    ifs.seekg(ifd_offset);

    char num_tags_buf[2];
    if (!ifs.read(num_tags_buf, 2)) {
        return core::Result<core::Frame>::err(core::ErrorCode::SerializationError, "Failed to read IFD tag count");
    }
    uint16_t num_tags = read_u16_le(num_tags_buf);

    uint32_t w = 0, h = 0, strip_offset = 0, spp = 1;
    for (uint16_t i = 0; i < num_tags; ++i) {
        char tag_buf[12];
        ifs.read(tag_buf, 12);
        uint16_t tag = read_u16_le(tag_buf);
        uint16_t type = read_u16_le(tag_buf + 2);
        uint32_t val = (type == 3) ? static_cast<uint32_t>(read_u16_le(tag_buf + 8)) : read_u32_le(tag_buf + 8);

        if (tag == 256) w = val;
        else if (tag == 257) h = val;
        else if (tag == 273) strip_offset = val;
        else if (tag == 277) spp = (val >= 3) ? 3 : 1;
    }

    if (w == 0 || h == 0 || strip_offset == 0) {
        return core::Result<core::Frame>::err(core::ErrorCode::SerializationError, "Missing required TIFF tags");
    }
    if (!core::frame_bytes(w, h, spp, 2).has_value()) {
        return core::Result<core::Frame>::err(core::ErrorCode::SerializationError, "TIFF dimensions exceed safe allocation limit");
    }

    core::Frame frame = core::Frame::create_allocated(w, h, static_cast<int>(spp), 16);
    ifs.seekg(strip_offset);
    ifs.read(reinterpret_cast<char*>(frame.pixels.get()), static_cast<std::streamsize>(w) * h * spp * 2);

    return frame;
}

core::Result<void> ImageWriter::write_pgm_16bit(const std::string& file_path, const core::Frame& frame) {
    if (!frame.is_valid()) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "Cannot write invalid frame to PGM");
    }

    std::filesystem::path final_path(file_path);
    if (final_path.has_parent_path()) {
        std::filesystem::create_directories(final_path.parent_path());
    }

    std::string tmp_path = file_path + ".tmp";
    std::ofstream ofs(tmp_path, std::ios::binary);
    if (!ofs.is_open()) {
        return core::Result<void>::err(core::ErrorCode::PermissionDenied, "Failed to open temporary PGM file");
    }

    // PGM header (P5 = binary grayscale)
    ofs << "P5\n" << frame.width << " " << frame.height << "\n65535\n";

    // PGM 16-bit requires 1-channel Big-Endian bytes
    const size_t num_px = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
    const int ch = std::max(1, frame.channels);
    const uint16_t* px = frame.pixels.get();
    std::vector<char> buf(num_px * 2);

    for (size_t i = 0; i < num_px; ++i) {
        uint16_t v = 0;
        if (ch >= 3) {
            size_t base = i * ch;
            v = static_cast<uint16_t>(
                (static_cast<uint32_t>(px[base + 0]) * 77 +
                 static_cast<uint32_t>(px[base + 1]) * 150 +
                 static_cast<uint32_t>(px[base + 2]) * 29) >> 8);
        } else {
            v = px[i];
        }
        buf[i * 2 + 0] = static_cast<char>((v >> 8) & 0xFF);
        buf[i * 2 + 1] = static_cast<char>(v & 0xFF);
    }

    ofs.write(buf.data(), buf.size());
    ofs.flush();
    ofs.close();

    std::error_code ec;
    std::filesystem::rename(tmp_path, final_path, ec);
    if (ec) {
        return core::Result<void>::err(core::ErrorCode::HardwareError, "Atomic rename failed: " + ec.message());
    }

    return core::Result<void>::ok();
}

core::Result<core::Frame> ImageWriter::read_pgm_16bit(const std::string& file_path) {
    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs.is_open()) {
        return core::Result<core::Frame>::err(core::ErrorCode::NotFound, "Failed to open PGM file");
    }

    std::string magic;
    int w = 0, h = 0, max_val = 0;
    ifs >> magic >> w >> h >> max_val;
    ifs.get(); // skip single newline/whitespace char after max_val

    if (magic != "P5" || w <= 0 || h <= 0 || max_val <= 0) {
        return core::Result<core::Frame>::err(core::ErrorCode::SerializationError, "Invalid PGM header");
    }
    if (!core::frame_bytes(w, h, 1, 2).has_value()) {
        return core::Result<core::Frame>::err(core::ErrorCode::SerializationError, "PGM dimensions exceed safe allocation limit");
    }

    core::Frame frame = core::Frame::create_allocated(w, h, 1, 16);
    size_t count = static_cast<size_t>(w) * h;
    std::vector<char> buf(count * 2);
    ifs.read(buf.data(), count * 2);

    uint16_t* dst = frame.pixels.get();
    for (size_t i = 0; i < count; ++i) {
        uint8_t b0 = static_cast<uint8_t>(buf[i * 2 + 0]);
        uint8_t b1 = static_cast<uint8_t>(buf[i * 2 + 1]);
        dst[i] = (static_cast<uint16_t>(b0) << 8) | b1;
    }

    return frame;
}

core::Result<void> ImageWriter::write_ppm_8bit(const std::string& file_path, int width, int height, int channels, const uint8_t* data) {
    if (width <= 0 || height <= 0 || !data) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "Invalid arguments for PPM write");
    }

    std::filesystem::path final_path(file_path);
    if (final_path.has_parent_path()) {
        std::filesystem::create_directories(final_path.parent_path());
    }

    std::string tmp_path = file_path + ".tmp";
    std::ofstream ofs(tmp_path, std::ios::binary);
    if (!ofs.is_open()) {
        return core::Result<void>::err(core::ErrorCode::PermissionDenied, "Failed to open temporary PPM file");
    }

    if (channels == 1) {
        ofs << "P5\n" << width << " " << height << "\n255\n";
        ofs.write(reinterpret_cast<const char*>(data), width * height);
    } else {
        ofs << "P6\n" << width << " " << height << "\n255\n";
        ofs.write(reinterpret_cast<const char*>(data), width * height * 3);
    }
    ofs.flush();
    ofs.close();

    std::error_code ec;
    std::filesystem::rename(tmp_path, final_path, ec);
    if (ec) {
        return core::Result<void>::err(core::ErrorCode::HardwareError, "Atomic rename failed: " + ec.message());
    }

    return core::Result<void>::ok();
}

// ============================================================================
// Standard PNG Writer (RFC 2083 / ISO/IEC 15948)
// ============================================================================
static uint32_t png_crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(static_cast<int32_t>(crc & 1))));
        }
    }
    return ~crc;
}

static void write_png_chunk(std::ostream& os, const char type[4], const uint8_t* payload, size_t len) {
    std::vector<uint8_t> buf;
    buf.reserve(4 + len);
    buf.push_back(static_cast<uint8_t>(type[0]));
    buf.push_back(static_cast<uint8_t>(type[1]));
    buf.push_back(static_cast<uint8_t>(type[2]));
    buf.push_back(static_cast<uint8_t>(type[3]));
    if (len > 0 && payload) {
        buf.insert(buf.end(), payload, payload + len);
    }
    uint32_t crc = png_crc32(buf.data(), buf.size());

    uint8_t len_be[4] = {
        static_cast<uint8_t>((len >> 24) & 0xFF),
        static_cast<uint8_t>((len >> 16) & 0xFF),
        static_cast<uint8_t>((len >> 8) & 0xFF),
        static_cast<uint8_t>(len & 0xFF)
    };
    uint8_t crc_be[4] = {
        static_cast<uint8_t>((crc >> 24) & 0xFF),
        static_cast<uint8_t>((crc >> 16) & 0xFF),
        static_cast<uint8_t>((crc >> 8) & 0xFF),
        static_cast<uint8_t>(crc & 0xFF)
    };
    os.write(reinterpret_cast<const char*>(len_be), 4);
    os.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    os.write(reinterpret_cast<const char*>(crc_be), 4);
}

core::Result<void> ImageWriter::write_png_8bit(const std::string& file_path, int width, int height, int channels, const uint8_t* data) {
    if (width <= 0 || height <= 0 || !data) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "Invalid arguments for PNG write");
    }

    std::filesystem::path final_path(file_path);
    if (final_path.has_parent_path()) {
        std::filesystem::create_directories(final_path.parent_path());
    }

    std::string tmp_path = file_path + ".tmp";
    std::ofstream ofs(tmp_path, std::ios::binary);
    if (!ofs.is_open()) {
        return core::Result<void>::err(core::ErrorCode::PermissionDenied, "Failed to open temporary PNG file");
    }

    const int out_ch = (channels >= 3) ? 3 : 1;
    const uint8_t color_type = (out_ch == 3) ? 2 : 0; // 2 = Truecolor RGB, 0 = Grayscale

    // 1. PNG 8-byte signature
    static const uint8_t kPngSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    ofs.write(reinterpret_cast<const char*>(kPngSig), 8);

    // 2. IHDR chunk (13 bytes)
    std::vector<uint8_t> ihdr;
    ihdr.reserve(13);
    write_u32_be(ihdr, static_cast<uint32_t>(width));
    write_u32_be(ihdr, static_cast<uint32_t>(height));
    ihdr.push_back(8);          // Bit depth: 8
    ihdr.push_back(color_type); // Color type: 0 (Gray) or 2 (RGB)
    ihdr.push_back(0);          // Compression method: 0 (Deflate)
    ihdr.push_back(0);          // Filter method: 0 (Adaptive)
    ihdr.push_back(0);          // Interlace method: 0 (None)
    write_png_chunk(ofs, "IHDR", ihdr.data(), ihdr.size());

    // 3. Build filtered scanlines (Filter 0 = None per row)
    const size_t row_bytes = static_cast<size_t>(width) * out_ch;
    const size_t raw_size = static_cast<size_t>(height) * (1 + row_bytes);
    std::vector<uint8_t> raw_scanlines(raw_size);

    uint32_t s1 = 1;
    uint32_t s2 = 0;
    size_t dst_pos = 0;

    for (int y = 0; y < height; ++y) {
        raw_scanlines[dst_pos++] = 0; // Filter type 0 (None)
        s2 = (s2 + s1) % 65521u;

        const uint8_t* src_row = data + static_cast<size_t>(y) * width * channels;
        if (channels == out_ch) {
            std::memcpy(raw_scanlines.data() + dst_pos, src_row, row_bytes);
            for (size_t i = 0; i < row_bytes; ++i) {
                s1 = (s1 + src_row[i]) % 65521u;
                s2 = (s2 + s1) % 65521u;
            }
            dst_pos += row_bytes;
        } else {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < out_ch; ++c) {
                    uint8_t v = src_row[x * channels + std::min(c, channels - 1)];
                    raw_scanlines[dst_pos++] = v;
                    s1 = (s1 + v) % 65521u;
                    s2 = (s2 + s1) % 65521u;
                }
            }
        }
    }

    // 4. Wrap in Zlib container (RFC 1950) with uncompressed Deflate blocks (RFC 1951)
    const size_t max_block = 65535;
    const size_t num_blocks = (raw_size + max_block - 1) / max_block;
    std::vector<uint8_t> zlib_stream;
    zlib_stream.reserve(2 + raw_size + num_blocks * 5 + 4);

    // Zlib header: CMF=0x78 (Deflate, 32K window), FLG=0x01 (no dict, check bits)
    zlib_stream.push_back(0x78);
    zlib_stream.push_back(0x01);

    size_t offset = 0;
    while (offset < raw_size) {
        size_t chunk_len = std::min(max_block, raw_size - offset);
        bool is_final = (offset + chunk_len >= raw_size);
        zlib_stream.push_back(is_final ? 0x01 : 0x00); // BFINAL | BTYPE=00
        uint16_t len16 = static_cast<uint16_t>(chunk_len);
        uint16_t nlen16 = ~len16;
        zlib_stream.push_back(static_cast<uint8_t>(len16 & 0xFF));
        zlib_stream.push_back(static_cast<uint8_t>((len16 >> 8) & 0xFF));
        zlib_stream.push_back(static_cast<uint8_t>(nlen16 & 0xFF));
        zlib_stream.push_back(static_cast<uint8_t>((nlen16 >> 8) & 0xFF));
        zlib_stream.insert(zlib_stream.end(),
                           raw_scanlines.begin() + static_cast<std::ptrdiff_t>(offset),
                           raw_scanlines.begin() + static_cast<std::ptrdiff_t>(offset + chunk_len));
        offset += chunk_len;
    }

    uint32_t adler = (s2 << 16) | s1;
    write_u32_be(zlib_stream, adler);

    write_png_chunk(ofs, "IDAT", zlib_stream.data(), zlib_stream.size());
    write_png_chunk(ofs, "IEND", nullptr, 0);

    ofs.flush();
    ofs.close();

    std::error_code ec;
    std::filesystem::rename(tmp_path, final_path, ec);
    if (ec) {
        return core::Result<void>::err(core::ErrorCode::HardwareError, "Atomic rename failed: " + ec.message());
    }

    return core::Result<void>::ok();
}

core::Result<void> ImageWriter::write_png(const std::string& file_path, const core::Frame& frame) {
    if (!frame.is_valid()) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "Cannot write invalid frame to PNG");
    }
    const int w = frame.width;
    const int h = frame.height;
    const int ch = std::max(1, frame.channels);
    const size_t count = frame.pixel_count();

    if (frame.bit_depth == 8) {
        const uint8_t* src8 = reinterpret_cast<const uint8_t*>(frame.pixels.get());
        return write_png_8bit(file_path, w, h, ch, src8);
    }

    std::vector<uint8_t> buf8(count);
    const uint16_t* src16 = frame.pixels.get();
    for (size_t i = 0; i < count; ++i) {
        buf8[i] = static_cast<uint8_t>(src16[i] >> 8);
    }
    return write_png_8bit(file_path, w, h, ch, buf8.data());
}

// ============================================================================
// Standard Baseline JFIF JPEG Encoder (ITU-T T.81 / ISO/IEC 10918-1)
// ============================================================================
namespace {

static const uint8_t kZigZag[64] = {
     0,  1,  5,  6, 14, 15, 27, 28,
     2,  4,  7, 13, 16, 26, 29, 42,
     3,  8, 12, 17, 25, 30, 41, 43,
     9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

static const uint8_t kStdLumQuant[64] = {
    16, 11, 10, 16,  24,  40,  51,  61,
    12, 12, 14, 19,  26,  58,  60,  55,
    14, 13, 16, 24,  40,  57,  69,  56,
    14, 17, 22, 29,  51,  87,  80,  62,
    18, 22, 37, 56,  68, 109, 103,  77,
    24, 35, 55, 64,  81, 104, 113,  92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103,  99
};

static const uint8_t kStdChrQuant[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

static const uint8_t kDcLumBits[16] = {0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};
static const uint8_t kDcLumVal[12]  = {0,1,2,3,4,5,6,7,8,9,10,11};

static const uint8_t kDcChrBits[16] = {0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0};
static const uint8_t kDcChrVal[12]  = {0,1,2,3,4,5,6,7,8,9,10,11};

static const uint8_t kAcLumBits[16] = {0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d};
static const uint8_t kAcLumVal[162] = {
    0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
    0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,
    0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,
    0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
    0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
    0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
    0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
    0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,
    0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,
    0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa
};

static const uint8_t kAcChrBits[16] = {0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,0x77};
static const uint8_t kAcChrVal[162] = {
    0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
    0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,
    0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,
    0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,
    0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,
    0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,
    0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,
    0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,
    0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,
    0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa
};

struct HuffCode {
    uint16_t code{0};
    uint8_t len{0};
};

static void build_huff_table(const uint8_t bits[16], const uint8_t* vals, HuffCode out_table[256]) {
    for (int i = 0; i < 256; ++i) {
        out_table[i] = HuffCode{};
    }
    uint16_t code = 0;
    int idx = 0;
    for (int len = 1; len <= 16; ++len) {
        for (int i = 0; i < bits[len - 1]; ++i) {
            uint8_t sym = vals[idx++];
            out_table[sym].code = code;
            out_table[sym].len = static_cast<uint8_t>(len);
            ++code;
        }
        code <<= 1;
    }
}

struct JpegBitWriter {
    std::vector<uint8_t>& out;
    uint32_t bit_buf{0};
    int bit_cnt{0};

    explicit JpegBitWriter(std::vector<uint8_t>& buf) : out(buf) {}

    void put_bits(uint16_t bits, int len) {
        bit_buf = (bit_buf << len) | (bits & ((1u << len) - 1u));
        bit_cnt += len;
        while (bit_cnt >= 8) {
            bit_cnt -= 8;
            uint8_t b = static_cast<uint8_t>((bit_buf >> bit_cnt) & 0xFF);
            out.push_back(b);
            if (b == 0xFF) {
                out.push_back(0x00); // Byte stuffing
            }
        }
    }

    void flush() {
        if (bit_cnt > 0) {
            uint16_t pad = static_cast<uint16_t>((1u << (8 - bit_cnt)) - 1u);
            put_bits(pad, 8 - bit_cnt);
        }
    }
};

static void fdct_8x8(float block[64]) {
    static const float kInvSqrt2 = 0.7071067811865475f;
    static const float kPi = 3.14159265358979323846f;
    float tmp[64];

    // 1D DCT on rows
    for (int y = 0; y < 8; ++y) {
        for (int u = 0; u < 8; ++u) {
            float sum = 0.0f;
            for (int x = 0; x < 8; ++x) {
                sum += block[y * 8 + x] * std::cos(((2 * x + 1) * u * kPi) / 16.0f);
            }
            float cu = (u == 0) ? kInvSqrt2 : 1.0f;
            tmp[y * 8 + u] = 0.5f * cu * sum;
        }
    }

    // 1D DCT on columns
    for (int x = 0; x < 8; ++x) {
        for (int v = 0; v < 8; ++v) {
            float sum = 0.0f;
            for (int y = 0; y < 8; ++y) {
                sum += tmp[y * 8 + x] * std::cos(((2 * y + 1) * v * kPi) / 16.0f);
            }
            float cv = (v == 0) ? kInvSqrt2 : 1.0f;
            block[v * 8 + x] = 0.5f * cv * sum;
        }
    }
}

static inline void encode_value_bits(int val, uint16_t& out_bits, uint8_t& out_mag) {
    int abs_v = std::abs(val);
    uint8_t mag = 0;
    while (abs_v > 0) {
        abs_v >>= 1;
        ++mag;
    }
    out_mag = mag;
    if (val >= 0) {
        out_bits = static_cast<uint16_t>(val);
    } else {
        out_bits = static_cast<uint16_t>(val + (1 << mag) - 1);
    }
}

static int encode_block(
    JpegBitWriter& bw,
    float block[64],
    const uint8_t qtable_zz[64],
    int prev_dc,
    const HuffCode dc_huff[256],
    const HuffCode ac_huff[256]
) {
    fdct_8x8(block);

    int quant[64];
    for (int i = 0; i < 64; ++i) {
        uint8_t z = kZigZag[i];
        quant[z] = static_cast<int>(std::lround(block[i] / static_cast<float>(qtable_zz[z])));
    }

    // Encode DC coefficient
    int dc_diff = quant[0] - prev_dc;
    uint16_t bits = 0;
    uint8_t mag = 0;
    encode_value_bits(dc_diff, bits, mag);
    bw.put_bits(dc_huff[mag].code, dc_huff[mag].len);
    if (mag > 0) {
        bw.put_bits(bits, mag);
    }

    // Encode AC coefficients
    int zero_run = 0;
    for (int k = 1; k < 64; ++k) {
        int ac = quant[k];
        if (ac == 0) {
            ++zero_run;
        } else {
            while (zero_run >= 16) {
                bw.put_bits(ac_huff[0xF0].code, ac_huff[0xF0].len); // ZRL (16 zeros)
                zero_run -= 16;
            }
            encode_value_bits(ac, bits, mag);
            uint8_t sym = static_cast<uint8_t>((zero_run << 4) | mag);
            bw.put_bits(ac_huff[sym].code, ac_huff[sym].len);
            bw.put_bits(bits, mag);
            zero_run = 0;
        }
    }
    if (zero_run > 0) {
        bw.put_bits(ac_huff[0x00].code, ac_huff[0x00].len); // EOB
    }

    return quant[0];
}

} // namespace

core::Result<void> ImageWriter::write_jpg_8bit(
    const std::string& file_path,
    int width,
    int height,
    int channels,
    const uint8_t* data,
    int quality
) {
    if (width <= 0 || height <= 0 || !data) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "Invalid arguments for JPEG write");
    }

    std::filesystem::path final_path(file_path);
    if (final_path.has_parent_path()) {
        std::filesystem::create_directories(final_path.parent_path());
    }

    quality = std::clamp(quality, 1, 100);
    int scale = (quality < 50) ? (5000 / quality) : (200 - quality * 2);

    uint8_t q_lum[64];
    uint8_t q_chr[64];
    for (int i = 0; i < 64; ++i) {
        int ql = (static_cast<int>(kStdLumQuant[i]) * scale + 50) / 100;
        int qc = (static_cast<int>(kStdChrQuant[i]) * scale + 50) / 100;
        q_lum[kZigZag[i]] = static_cast<uint8_t>(std::clamp(ql, 1, 255));
        q_chr[kZigZag[i]] = static_cast<uint8_t>(std::clamp(qc, 1, 255));
    }

    HuffCode dc_lum[256], ac_lum[256], dc_chr[256], ac_chr[256];
    build_huff_table(kDcLumBits, kDcLumVal, dc_lum);
    build_huff_table(kAcLumBits, kAcLumVal, ac_lum);
    build_huff_table(kDcChrBits, kDcChrVal, dc_chr);
    build_huff_table(kAcChrBits, kAcChrVal, ac_chr);

    const bool is_color = (channels >= 3);
    std::vector<uint8_t> jpg;
    jpg.reserve(static_cast<size_t>(width) * height);

    auto push_u16_be = [&](uint16_t v) {
        jpg.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        jpg.push_back(static_cast<uint8_t>(v & 0xFF));
    };

    // 1. SOI + APP0 (JFIF 1.01)
    static const uint8_t kJfifHeader[20] = {
        0xFF, 0xD8,
        0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0x00,
        0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00
    };
    jpg.insert(jpg.end(), kJfifHeader, kJfifHeader + 20);

    // 2. DQT (Quantization Tables)
    jpg.push_back(0xFF); jpg.push_back(0xDB);
    push_u16_be(is_color ? 132 : 67);
    jpg.push_back(0x00); // Table 0 (8-bit Lum)
    jpg.insert(jpg.end(), q_lum, q_lum + 64);
    if (is_color) {
        jpg.push_back(0x01); // Table 1 (8-bit Chr)
        jpg.insert(jpg.end(), q_chr, q_chr + 64);
    }

    // 3. SOF0 (Baseline DCT)
    jpg.push_back(0xFF); jpg.push_back(0xC0);
    push_u16_be(is_color ? 17 : 11);
    jpg.push_back(8); // 8-bit precision
    push_u16_be(static_cast<uint16_t>(height));
    push_u16_be(static_cast<uint16_t>(width));
    jpg.push_back(is_color ? 3 : 1);
    // Component 1: Y (1x1 sampling, QTable 0)
    jpg.push_back(1); jpg.push_back(0x11); jpg.push_back(0);
    if (is_color) {
        // Component 2: Cb (1x1 sampling, QTable 1)
        jpg.push_back(2); jpg.push_back(0x11); jpg.push_back(1);
        // Component 3: Cr (1x1 sampling, QTable 1)
        jpg.push_back(3); jpg.push_back(0x11); jpg.push_back(1);
    }

    // 4. DHT (Huffman Tables)
    auto write_dht = [&](uint8_t tc_th, const uint8_t bits[16], const uint8_t* vals, size_t nvals) {
        jpg.push_back(0xFF); jpg.push_back(0xC4);
        push_u16_be(static_cast<uint16_t>(3 + 16 + nvals));
        jpg.push_back(tc_th);
        jpg.insert(jpg.end(), bits, bits + 16);
        jpg.insert(jpg.end(), vals, vals + nvals);
    };
    write_dht(0x00, kDcLumBits, kDcLumVal, 12);
    write_dht(0x10, kAcLumBits, kAcLumVal, 162);
    if (is_color) {
        write_dht(0x01, kDcChrBits, kDcChrVal, 12);
        write_dht(0x11, kAcChrBits, kAcChrVal, 162);
    }

    // 5. SOS (Start of Scan)
    jpg.push_back(0xFF); jpg.push_back(0xDA);
    push_u16_be(is_color ? 12 : 8);
    jpg.push_back(is_color ? 3 : 1);
    jpg.push_back(1); jpg.push_back(0x00); // Y: DC 0, AC 0
    if (is_color) {
        jpg.push_back(2); jpg.push_back(0x11); // Cb: DC 1, AC 1
        jpg.push_back(3); jpg.push_back(0x11); // Cr: DC 1, AC 1
    }
    jpg.push_back(0x00); jpg.push_back(0x3F); jpg.push_back(0x00);

    // 6. Entropy-Coded Scan Data (8x8 MCUs)
    JpegBitWriter bw(jpg);
    int dc_y = 0, dc_cb = 0, dc_cr = 0;
    float blk_y[64], blk_cb[64], blk_cr[64];

    for (int by = 0; by < height; by += 8) {
        for (int bx = 0; bx < width; bx += 8) {
            for (int dy = 0; dy < 8; ++dy) {
                int sy = std::min(by + dy, height - 1);
                const uint8_t* row = data + static_cast<size_t>(sy) * width * channels;
                for (int dx = 0; dx < 8; ++dx) {
                    int sx = std::min(bx + dx, width - 1);
                    int i = dy * 8 + dx;
                    if (is_color) {
                        float r = row[sx * channels + 0];
                        float g = row[sx * channels + 1];
                        float b = row[sx * channels + 2];
                        blk_y[i]  =  0.29900f * r + 0.58700f * g + 0.11400f * b - 128.0f;
                        blk_cb[i] = -0.16874f * r - 0.33126f * g + 0.50000f * b;
                        blk_cr[i] =  0.50000f * r - 0.41869f * g - 0.08131f * b;
                    } else {
                        blk_y[i] = static_cast<float>(row[sx * channels]) - 128.0f;
                    }
                }
            }

            dc_y = encode_block(bw, blk_y, q_lum, dc_y, dc_lum, ac_lum);
            if (is_color) {
                dc_cb = encode_block(bw, blk_cb, q_chr, dc_cb, dc_chr, ac_chr);
                dc_cr = encode_block(bw, blk_cr, q_chr, dc_cr, dc_chr, ac_chr);
            }
        }
    }
    bw.flush();

    // 7. EOI
    jpg.push_back(0xFF);
    jpg.push_back(0xD9);

    std::string tmp_path = file_path + ".tmp";
    std::ofstream ofs(tmp_path, std::ios::binary);
    if (!ofs.is_open()) {
        return core::Result<void>::err(core::ErrorCode::PermissionDenied, "Failed to open temporary JPEG file");
    }
    ofs.write(reinterpret_cast<const char*>(jpg.data()), static_cast<std::streamsize>(jpg.size()));
    ofs.flush();
    ofs.close();

    std::error_code ec;
    std::filesystem::rename(tmp_path, final_path, ec);
    if (ec) {
        return core::Result<void>::err(core::ErrorCode::HardwareError, "Atomic rename failed: " + ec.message());
    }

    return core::Result<void>::ok();
}

core::Result<void> ImageWriter::write_jpg(const std::string& file_path, const core::Frame& frame, int quality) {
    if (!frame.is_valid()) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "Cannot write invalid frame to JPEG");
    }
    const int w = frame.width;
    const int h = frame.height;
    const int ch = std::max(1, frame.channels);
    const size_t count = frame.pixel_count();

    if (frame.bit_depth == 8) {
        const uint8_t* src8 = reinterpret_cast<const uint8_t*>(frame.pixels.get());
        return write_jpg_8bit(file_path, w, h, ch, src8, quality);
    }

    std::vector<uint8_t> buf8(count);
    const uint16_t* src16 = frame.pixels.get();
    for (size_t i = 0; i < count; ++i) {
        buf8[i] = static_cast<uint8_t>(src16[i] >> 8);
    }
    return write_jpg_8bit(file_path, w, h, ch, buf8.data(), quality);
}

} // namespace sabit::imaging::io
