#include "src/imaging/io/fits_writer.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <cmath>
#include "src/core/safe_size.h"

namespace sabit::imaging::io {

static constexpr size_t kFitsBlockSize = 2880;
static constexpr size_t kFitsCardSize = 80;

std::string FitsWriter::format_card(const std::string& key, const std::string& val_str, const std::string& comment) {
    std::string card(kFitsCardSize, ' ');
    // Left-justify key in chars 0..7
    for (size_t i = 0; i < std::min(key.size(), size_t(8)); ++i) {
        card[i] = static_cast<char>(std::toupper(key[i]));
    }
    if (!val_str.empty()) {
        card[8] = '=';
        card[9] = ' ';
        // Value field typically chars 10..29
        size_t v_len = std::min(val_str.size(), size_t(20));
        size_t start_col = 30 - v_len;
        for (size_t i = 0; i < v_len; ++i) {
            card[start_col + i] = val_str[i];
        }
    }
    if (!comment.empty()) {
        size_t c_start = 31;
        card[c_start] = '/';
        card[c_start + 1] = ' ';
        size_t c_len = std::min(comment.size(), kFitsCardSize - (c_start + 2));
        for (size_t i = 0; i < c_len; ++i) {
            card[c_start + 2 + i] = comment[i];
        }
    }
    return card;
}

std::string FitsWriter::format_card_string(const std::string& key, const std::string& str_val, const std::string& comment) {
    std::string card(kFitsCardSize, ' ');
    for (size_t i = 0; i < std::min(key.size(), size_t(8)); ++i) {
        card[i] = static_cast<char>(std::toupper(key[i]));
    }
    card[8] = '=';
    card[9] = ' ';
    card[10] = '\'';
    size_t s_len = std::min(str_val.size(), size_t(18));
    for (size_t i = 0; i < s_len; ++i) {
        card[11 + i] = str_val[i];
    }
    card[11 + s_len] = '\'';

    if (!comment.empty()) {
        size_t c_start = 31;
        card[c_start] = '/';
        card[c_start + 1] = ' ';
        size_t c_len = std::min(comment.size(), kFitsCardSize - (c_start + 2));
        for (size_t i = 0; i < c_len; ++i) {
            card[c_start + 2 + i] = comment[i];
        }
    }
    return card;
}

std::string FitsWriter::format_card_int(const std::string& key, int64_t val, const std::string& comment) {
    return format_card(key, std::to_string(val), comment);
}

std::string FitsWriter::format_card_float(const std::string& key, double val, int precision, const std::string& comment) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << val;
    return format_card(key, ss.str(), comment);
}

std::string FitsWriter::format_card_bool(const std::string& key, bool val, const std::string& comment) {
    return format_card(key, val ? "T" : "F", comment);
}

core::Result<void> FitsWriter::write_fits_16bit(
    const std::string& file_path,
    const core::Frame& frame,
    const FitsMetadata& meta
) {
    if (!frame.is_valid()) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "Cannot write invalid frame to FITS");
    }

    std::filesystem::path final_path(file_path);
    if (final_path.has_parent_path()) {
        std::filesystem::create_directories(final_path.parent_path());
    }

    // Atomic write pattern (NFR-07, TC-044): write to .tmp then rename
    std::filesystem::path tmp_path = file_path + ".tmp";
    std::ofstream ofs(tmp_path, std::ios::binary);
    if (!ofs.is_open()) {
        return core::Result<void>::err(core::ErrorCode::PermissionDenied, "Failed to open temporary FITS file for writing");
    }

    // 1. Generate Header Cards
    std::vector<std::string> cards;
    cards.reserve(36);

    const int out_ch = (frame.channels >= 3) ? 3 : 1;
    cards.push_back(format_card_bool("SIMPLE", true, "file does conform to FITS standard"));
    cards.push_back(format_card_int("BITPIX", 16, "number of bits per data pixel"));
    cards.push_back(format_card_int("NAXIS", (out_ch == 3) ? 3 : 2, "number of data axes"));
    cards.push_back(format_card_int("NAXIS1", frame.width, "length of data axis 1 (width)"));
    cards.push_back(format_card_int("NAXIS2", frame.height, "length of data axis 2 (height)"));
    if (out_ch == 3) {
        cards.push_back(format_card_int("NAXIS3", 3, "length of data axis 3 (RGB planes)"));
    }
    cards.push_back(format_card_int("BZERO", 32768, "offset data range to that of unsigned"));
    cards.push_back(format_card_int("BSCALE", 1, "default scaling factor"));

    // Observation & Hardware Metadata (TC-042)
    std::string date_str = core::Clock::to_iso8601(frame.timestamp_utc);
    cards.push_back(format_card_string("DATE-OBS", date_str, "UTC date of observation"));
    cards.push_back(format_card_float("EXPTIME", static_cast<double>(frame.exposure_us) / 1000000.0, 6, "exposure time in seconds"));
    cards.push_back(format_card_float("GAIN", frame.gain, 2, "camera gain setting"));

    if (!std::isnan(frame.sensor_temp_c)) {
        cards.push_back(format_card_float("CCD-TEMP", frame.sensor_temp_c, 2, "sensor temperature in C"));
    }
    cards.push_back(format_card_int("XBINNING", frame.binning, "binning factor along x-axis"));
    cards.push_back(format_card_int("YBINNING", frame.binning, "binning factor along y-axis"));

    cards.push_back(format_card_string("INSTRUME", meta.instrument, "instrument name"));
    cards.push_back(format_card_string("OBSERVER", meta.observer, "observer name"));
    cards.push_back(format_card_string("OBJECT", meta.object, "observed target"));

    cards.push_back(format_card_float("SITELAT", meta.site_latitude, 5, "observer site latitude (deg)"));
    cards.push_back(format_card_float("SITELONG", meta.site_longitude, 5, "observer site longitude (deg)"));
    cards.push_back(format_card_float("SITEELEV", meta.site_elevation_m, 1, "observer site elevation (m)"));

    cards.push_back(format_card_string("SWCREATE", "Sabit 1.0.0", "software application name"));

    for (const auto& [k, v] : meta.extra_cards) {
        cards.push_back(format_card_string(k, v, ""));
    }

    cards.push_back(format_card("END", "", ""));

    // Write header padded to multiple of 2880 bytes
    size_t header_bytes = cards.size() * kFitsCardSize;
    size_t header_blocks = (header_bytes + kFitsBlockSize - 1) / kFitsBlockSize;
    size_t total_header_bytes = header_blocks * kFitsBlockSize;

    std::string header_buffer(total_header_bytes, ' ');
    for (size_t i = 0; i < cards.size(); ++i) {
        std::copy(cards[i].begin(), cards[i].end(), header_buffer.begin() + i * kFitsCardSize);
    }

    ofs.write(header_buffer.data(), header_buffer.size());

    // 2. Write Pixel Payload in Big-Endian Order
    // FITS requires unsigned 16-bit to be mapped to signed 16-bit by subtracting 32768
    const size_t plane_pixels = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
    const size_t total_samples = plane_pixels * static_cast<size_t>(out_ch);
    const uint16_t* src_px = frame.pixels.get();
    const uint8_t* src_u8 = reinterpret_cast<const uint8_t*>(src_px);
    const bool is_8bit = (frame.bit_depth == 8);
    const int src_ch = std::max(1, frame.channels);

    size_t payload_bytes = total_samples * sizeof(uint16_t);
    size_t payload_blocks = (payload_bytes + kFitsBlockSize - 1) / kFitsBlockSize;
    size_t total_payload_bytes = payload_blocks * kFitsBlockSize;

    std::vector<char> byte_buffer(total_payload_bytes, 0);

    if (out_ch == 3) {
        // Standard 3D FITS RGB Cube: Plane 0 (R), Plane 1 (G), Plane 2 (B)
        for (int c = 0; c < 3; ++c) {
            size_t plane_offset = static_cast<size_t>(c) * plane_pixels;
            for (size_t p = 0; p < plane_pixels; ++p) {
                uint16_t u_val = 0;
                if (is_8bit) {
                    uint8_t v8 = src_u8[p * src_ch + std::min(c, src_ch - 1)];
                    u_val = static_cast<uint16_t>((static_cast<uint16_t>(v8) << 8) | v8);
                } else {
                    u_val = src_px[p * src_ch + std::min(c, src_ch - 1)];
                }
                int16_t s_val = static_cast<int16_t>(static_cast<int32_t>(u_val) - 32768);
                size_t out_idx = (plane_offset + p) * 2;
                byte_buffer[out_idx + 0] = static_cast<char>((s_val >> 8) & 0xFF);
                byte_buffer[out_idx + 1] = static_cast<char>(s_val & 0xFF);
            }
        }
    } else {
        for (size_t i = 0; i < plane_pixels; ++i) {
            uint16_t u_val = is_8bit
                ? static_cast<uint16_t>((static_cast<uint16_t>(src_u8[i]) << 8) | src_u8[i])
                : src_px[i];
            int16_t s_val = static_cast<int16_t>(static_cast<int32_t>(u_val) - 32768);

            // Big-Endian format (MSB first)
            byte_buffer[i * 2 + 0] = static_cast<char>((s_val >> 8) & 0xFF);
            byte_buffer[i * 2 + 1] = static_cast<char>(s_val & 0xFF);
        }
    }

    ofs.write(byte_buffer.data(), byte_buffer.size());
    ofs.flush();

    if (!ofs.good()) {
        ofs.close();
        std::filesystem::remove(tmp_path);
        return core::Result<void>::err(core::ErrorCode::HardwareError, "I/O error while writing FITS data");
    }

    ofs.close();

    // Atomic rename
    std::error_code ec;
    std::filesystem::rename(tmp_path, final_path, ec);
    if (ec) {
        return core::Result<void>::err(core::ErrorCode::HardwareError, "Atomic rename failed: " + ec.message());
    }

    return core::Result<void>::ok();
}

core::Result<core::Frame> FitsWriter::read_fits_16bit(const std::string& file_path, FitsMetadata* out_meta) {
    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs.is_open()) {
        return core::Result<core::Frame>::err(core::ErrorCode::NotFound, "Failed to open FITS file: " + file_path);
    }

    int naxis = 2;
    int width = 0;
    int height = 0;
    int naxis3 = 1;
    int bitpix = 0;
    int32_t bzero = 0;
    int64_t exposure_us = 0;
    double gain = 0.0;
    double ccd_temp = std::numeric_limits<double>::quiet_NaN();
    int binning = 1;
    core::Clock::TimePoint time_obs{};
    FitsMetadata meta;

    bool end_found = false;
    std::vector<char> block(kFitsBlockSize);

    while (!end_found && ifs.read(block.data(), kFitsBlockSize)) {
        for (size_t c = 0; c < kFitsBlockSize / kFitsCardSize; ++c) {
            std::string card(block.data() + c * kFitsCardSize, kFitsCardSize);
            std::string key = card.substr(0, 8);
            // Trim key
            key.erase(key.find_last_not_of(' ') + 1);

            if (key == "END") {
                end_found = true;
                break;
            }

            size_t eq_pos = card.find('=');
            if (eq_pos != std::string::npos) {
                size_t slash_pos = card.find('/', eq_pos);
                std::string val_part = card.substr(eq_pos + 1, slash_pos - (eq_pos + 1));
                val_part.erase(0, val_part.find_first_not_of(" '"));
                val_part.erase(val_part.find_last_not_of(" '") + 1);

                if (key == "NAXIS") naxis = std::stoi(val_part);
                else if (key == "NAXIS1") width = std::stoi(val_part);
                else if (key == "NAXIS2") height = std::stoi(val_part);
                else if (key == "NAXIS3") naxis3 = std::stoi(val_part);
                else if (key == "BITPIX") bitpix = std::stoi(val_part);
                else if (key == "BZERO") bzero = std::stoi(val_part);
                else if (key == "EXPTIME") exposure_us = static_cast<int64_t>(std::stod(val_part) * 1000000.0);
                else if (key == "GAIN") gain = std::stod(val_part);
                else if (key == "CCD-TEMP") ccd_temp = std::stod(val_part);
                else if (key == "XBINNING") binning = std::stoi(val_part);
                else if (key == "DATE-OBS") time_obs = core::Clock::from_iso8601(val_part);
                else if (key == "INSTRUME") meta.instrument = val_part;
                else if (key == "OBSERVER") meta.observer = val_part;
                else if (key == "OBJECT") meta.object = val_part;
                else if (key == "SITELAT") meta.site_latitude = std::stod(val_part);
                else if (key == "SITELONG") meta.site_longitude = std::stod(val_part);
                else if (key == "SITEELEV") meta.site_elevation_m = std::stod(val_part);
            }
        }
    }

    const int channels = (naxis >= 3 && naxis3 >= 3) ? 3 : 1;
    if (width <= 0 || height <= 0 || bitpix != 16) {
        return core::Result<core::Frame>::err(core::ErrorCode::SerializationError, "Invalid FITS header dimensions or BITPIX");
    }
    if (!core::frame_bytes(width, height, channels, 2).has_value()) {
        return core::Result<core::Frame>::err(core::ErrorCode::SerializationError, "FITS dimensions exceed safe allocation limit");
    }

    core::Frame frame = core::Frame::create_allocated(width, height, channels, 16);
    frame.exposure_us = exposure_us;
    frame.gain = gain;
    frame.sensor_temp_c = ccd_temp;
    frame.binning = binning;
    frame.timestamp_utc = time_obs;

    const size_t plane_pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t total_samples = plane_pixels * static_cast<size_t>(channels);
    std::vector<char> raw_bytes(total_samples * 2);
    ifs.read(raw_bytes.data(), static_cast<std::streamsize>(total_samples * 2));

    uint16_t* dst = frame.pixels.get();
    if (channels == 3) {
        for (int c = 0; c < 3; ++c) {
            size_t plane_offset = static_cast<size_t>(c) * plane_pixels;
            for (size_t p = 0; p < plane_pixels; ++p) {
                size_t in_idx = (plane_offset + p) * 2;
                uint8_t b0 = static_cast<uint8_t>(raw_bytes[in_idx + 0]);
                uint8_t b1 = static_cast<uint8_t>(raw_bytes[in_idx + 1]);
                int16_t s_val = static_cast<int16_t>((static_cast<uint16_t>(b0) << 8) | b1);
                uint16_t u_val = static_cast<uint16_t>(static_cast<int32_t>(s_val) + bzero);
                dst[p * 3 + c] = u_val;
            }
        }
    } else {
        for (size_t i = 0; i < plane_pixels; ++i) {
            uint8_t b0 = static_cast<uint8_t>(raw_bytes[i * 2 + 0]);
            uint8_t b1 = static_cast<uint8_t>(raw_bytes[i * 2 + 1]);
            int16_t s_val = static_cast<int16_t>((static_cast<uint16_t>(b0) << 8) | b1);

            uint16_t u_val = static_cast<uint16_t>(static_cast<int32_t>(s_val) + bzero);
            dst[i] = u_val;
        }
    }

    if (out_meta) {
        *out_meta = meta;
    }

    return frame;
}

} // namespace sabit::imaging::io
