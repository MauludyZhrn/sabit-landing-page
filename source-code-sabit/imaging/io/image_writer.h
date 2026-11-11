#ifndef SABIT_IMAGING_IO_IMAGE_WRITER_H
#define SABIT_IMAGING_IO_IMAGE_WRITER_H

#include <string>
#include <vector>
#include "src/core/frame.h"
#include "src/core/result.h"

namespace sabit::imaging::io {

class ImageWriter {
public:
    /**
     * Atomically writes an uncompressed baseline 16-bit TIFF image (grayscale/RGB).
     */
    static core::Result<void> write_tiff_16bit(const std::string& file_path, const core::Frame& frame);

    /**
     * Reads a baseline uncompressed 16-bit grayscale TIFF.
     */
    static core::Result<core::Frame> read_tiff_16bit(const std::string& file_path);

    /**
     * Atomically writes a 16-bit PGM image (Netpbm P5 format).
     */
    static core::Result<void> write_pgm_16bit(const std::string& file_path, const core::Frame& frame);

    /**
     * Reads a 16-bit PGM image.
     */
    static core::Result<core::Frame> read_pgm_16bit(const std::string& file_path);

    /**
     * Atomically writes an 8-bit PPM/PGM image from 8-bit preview data.
     */
    static core::Result<void> write_ppm_8bit(const std::string& file_path, int width, int height, int channels, const uint8_t* data);

    /**
     * Atomically writes a standard lossless 8-bit PNG image (grayscale or RGB).
     */
    static core::Result<void> write_png_8bit(const std::string& file_path, int width, int height, int channels, const uint8_t* data);

    /**
     * Atomically writes a Frame as a standard PNG image.
     */
    static core::Result<void> write_png(const std::string& file_path, const core::Frame& frame);

    /**
     * Atomically writes a standard baseline JFIF JPEG image (grayscale or 4:4:4 RGB).
     */
    static core::Result<void> write_jpg_8bit(const std::string& file_path, int width, int height, int channels, const uint8_t* data, int quality = 92);

    /**
     * Atomically writes a Frame as a standard JPEG image.
     */
    static core::Result<void> write_jpg(const std::string& file_path, const core::Frame& frame, int quality = 92);
};

} // namespace sabit::imaging::io

#endif // SABIT_IMAGING_IO_IMAGE_WRITER_H
