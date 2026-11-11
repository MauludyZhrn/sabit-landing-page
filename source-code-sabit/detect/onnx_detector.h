#ifndef SABIT_DETECT_ONNX_DETECTOR_H
#define SABIT_DETECT_ONNX_DETECTOR_H

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <functional>

#include "src/core/frame.h"
#include "src/core/result.h"
#include "src/detect/detection.h"

// Forward declaration of ONNX Runtime C types
struct OrtEnv;
struct OrtSession;
struct OrtSessionOptions;
struct OrtMemoryInfo;
struct OrtApi;

namespace sabit::detect {

struct OnnxDetectorConfig {
    std::string model_path{"models/hilal_v1.onnx"};
    std::string manifest_path{"models/manifest.json"};
    std::string dll_path{"onnxruntime.dll"};
    bool verify_hash{true};
    float conf_threshold{0.25f}; // 0.10 - 0.95
    float nms_threshold{0.45f};  // 0.20 - 0.70
    int num_threads{2};
    double max_fps{3.0};         // 1.0 - 10.0 FPS
};

class OnnxDetector {
public:
    OnnxDetector();
    ~OnnxDetector();

    core::Result<void> init(const OnnxDetectorConfig& config);
    void close();

    [[nodiscard]] bool is_initialized() const;

    // Run direct synchronous inference on a frame
    core::Result<DetectionResult> detect(const core::Frame& frame);

    // Dynamic thresholds (FR-20)
    void set_conf_threshold(float conf) { conf_threshold_.store(conf); }
    [[nodiscard]] float conf_threshold() const { return conf_threshold_.load(); }

    void set_nms_threshold(float nms) { nms_threshold_.store(nms); }
    [[nodiscard]] float nms_threshold() const { return nms_threshold_.load(); }

    void set_max_fps(double fps) { max_fps_.store(fps); }
    [[nodiscard]] double max_fps() const { return max_fps_.load(); }

    // Async Threaded AI Execution (Section 8)
    void start_worker();
    void stop_worker();
    void submit_frame(const core::Frame& frame);

    // Retrieve latest detections for UI overlay (discards if > max_age_ms, default 500ms)
    [[nodiscard]] std::vector<Detection> get_latest_overlay(int64_t max_age_ms = 500) const;

    // Callback on detection
    void set_detection_callback(std::function<void(const core::Frame&, const DetectionResult&)> cb) {
        on_detection_ = std::move(cb);
    }

private:
    bool verify_manifest_hash();
    void worker_loop();

    OnnxDetectorConfig config_{};
    std::atomic<float> conf_threshold_{0.25f};
    std::atomic<float> nms_threshold_{0.45f};
    std::atomic<double> max_fps_{3.0};

    void* dll_handle_{nullptr};
    const OrtApi* ort_{nullptr};
    OrtEnv* env_{nullptr};
    OrtSessionOptions* session_options_{nullptr};
    OrtSession* session_{nullptr};
    OrtMemoryInfo* mem_info_{nullptr};

    std::atomic<bool> is_initialized_{false};

    // Threading & Latest Slot
    std::thread worker_thread_;
    std::atomic<bool> is_running_{false};
    mutable std::mutex frame_mutex_;
    std::condition_variable frame_cv_;
    std::optional<core::Frame> pending_frame_;

    mutable std::mutex result_mutex_;
    DetectionResult latest_result_{};
    std::chrono::steady_clock::time_point latest_result_time_{};

    std::function<void(const core::Frame&, const DetectionResult&)> on_detection_;
};

} // namespace sabit::detect

#endif // SABIT_DETECT_ONNX_DETECTOR_H
