#ifndef SABIT_DETECT_DETECTION_H
#define SABIT_DETECT_DETECTION_H

#include <string>
#include <vector>
#include <cstdint>

namespace sabit::detect {

struct Detection {
    std::string label{"Hilal"};
    float confidence{0.0f};
    float x{0.0f};       // top-left x in frame coordinates
    float y{0.0f};       // top-left y in frame coordinates
    float width{0.0f};   // width in pixels
    float height{0.0f};  // height in pixels
    int64_t timestamp_utc_ms{0};
};

struct DetectionResult {
    std::vector<Detection> detections;
    int64_t frame_index{0};
    int64_t timestamp_utc_ms{0};
    double inference_time_ms{0.0};
    bool is_valid{false};
};

} // namespace sabit::detect

#endif // SABIT_DETECT_DETECTION_H
