#ifndef SABIT_DETECT_DETECTION_LOG_H
#define SABIT_DETECT_DETECTION_LOG_H

#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include "src/core/frame.h"
#include "src/detect/detection.h"

namespace sabit::detect {

class DetectionLogger {
public:
    explicit DetectionLogger(const std::string& csv_path = "detections.csv");
    ~DetectionLogger();

    bool open(const std::string& csv_path);
    void close();

    void log(const core::Frame& frame, const Detection& detection);
    void log_batch(const core::Frame& frame, const std::vector<Detection>& detections);

    void flush();

    [[nodiscard]] size_t logged_count() const { return logged_count_; }
    [[nodiscard]] std::string path() const { return csv_path_; }

private:
    void ensure_header();

    std::string csv_path_;
    std::ofstream out_;
    mutable std::mutex mutex_;
    size_t logged_count_{0};
};

} // namespace sabit::detect

#endif // SABIT_DETECT_DETECTION_LOG_H
