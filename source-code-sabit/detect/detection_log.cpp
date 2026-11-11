#include "src/detect/detection_log.h"
#include "src/core/clock.h"
#include <iomanip>
#include <filesystem>

namespace sabit::detect {

DetectionLogger::DetectionLogger(const std::string& csv_path) {
    if (!csv_path.empty()) {
        open(csv_path);
    }
}

DetectionLogger::~DetectionLogger() {
    close();
}

bool DetectionLogger::open(const std::string& csv_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    close();
    csv_path_ = csv_path;

    bool exists = std::filesystem::exists(csv_path_);
    bool is_empty = false;
    if (exists) {
        std::error_code ec;
        is_empty = (std::filesystem::file_size(csv_path_, ec) == 0);
    }

    out_.open(csv_path_, std::ios::app);
    if (!out_.is_open()) {
        return false;
    }

    if (!exists || is_empty) {
        out_ << "timestamp_utc,frame_id,label,confidence,x,y,width,height,exposure_us,gain,sensor_temp_c\n";
        out_.flush();
    }

    return true;
}

void DetectionLogger::close() {
    if (out_.is_open()) {
        out_.flush();
        out_.close();
    }
}

void DetectionLogger::log(const core::Frame& frame, const Detection& detection) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!out_.is_open()) return;

    std::string time_str = core::Clock::to_iso8601(frame.timestamp_utc);

    out_ << time_str << ","
         << frame.seq << ","
         << "\"" << detection.label << "\","
         << std::fixed << std::setprecision(4) << detection.confidence << ","
         << std::fixed << std::setprecision(2) << detection.x << ","
         << std::fixed << std::setprecision(2) << detection.y << ","
         << std::fixed << std::setprecision(2) << detection.width << ","
         << std::fixed << std::setprecision(2) << detection.height << ","
         << frame.exposure_us << ","
         << std::fixed << std::setprecision(1) << frame.gain << ","
         << std::fixed << std::setprecision(1) << frame.sensor_temp_c << "\n";

    ++logged_count_;
}

void DetectionLogger::log_batch(const core::Frame& frame, const std::vector<Detection>& detections) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!out_.is_open()) return;

    std::string time_str = core::Clock::to_iso8601(frame.timestamp_utc);

    for (const auto& det : detections) {
        out_ << time_str << ","
             << frame.seq << ","
             << "\"" << det.label << "\","
             << std::fixed << std::setprecision(4) << det.confidence << ","
             << std::fixed << std::setprecision(2) << det.x << ","
             << std::fixed << std::setprecision(2) << det.y << ","
             << std::fixed << std::setprecision(2) << det.width << ","
             << std::fixed << std::setprecision(2) << det.height << ","
             << frame.exposure_us << ","
             << std::fixed << std::setprecision(1) << frame.gain << ","
             << std::fixed << std::setprecision(1) << frame.sensor_temp_c << "\n";

        ++logged_count_;
    }
    out_.flush();
}

void DetectionLogger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (out_.is_open()) {
        out_.flush();
    }
}

} // namespace sabit::detect
