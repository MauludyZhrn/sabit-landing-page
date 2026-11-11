#ifndef SABIT_IMAGING_IO_FITS_WRITER_H
#define SABIT_IMAGING_IO_FITS_WRITER_H

#include <string>
#include <map>
#include "src/core/frame.h"
#include "src/core/result.h"

namespace sabit::imaging::io {

struct FitsMetadata {
    std::string object{"Hilal"};
    std::string instrument{"Sabit Observation System"};
    std::string observer{"Sabit Team"};
    double site_latitude{0.0};
    double site_longitude{0.0};
    double site_elevation_m{0.0};
    std::map<std::string, std::string> extra_cards;
};

class FitsWriter {
public:
    /**
     * Atomically writes a 16-bit NASA/IAU Standard FITS file.
     * Writes to <path>.tmp and renames on completion (NFR-07, TC-044).
     */
    static core::Result<void> write_fits_16bit(
        const std::string& file_path,
        const core::Frame& frame,
        const FitsMetadata& meta = {}
    );

    /**
     * Reads a 16-bit FITS file back into a Frame, extracting header cards.
     * Used for lossless verification and inspection (TC-024, TC-042).
     */
    static core::Result<core::Frame> read_fits_16bit(
        const std::string& file_path,
        FitsMetadata* out_meta = nullptr
    );

private:
    static std::string format_card(const std::string& key, const std::string& val_str, const std::string& comment = "");
    static std::string format_card_string(const std::string& key, const std::string& str_val, const std::string& comment = "");
    static std::string format_card_int(const std::string& key, int64_t val, const std::string& comment = "");
    static std::string format_card_float(const std::string& key, double val, int precision, const std::string& comment = "");
    static std::string format_card_bool(const std::string& key, bool val, const std::string& comment = "");
};

} // namespace sabit::imaging::io

#endif // SABIT_IMAGING_IO_FITS_WRITER_H
