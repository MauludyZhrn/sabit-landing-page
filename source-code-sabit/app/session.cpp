#include "src/app/session.h"
#include "src/core/clock.h"
#include "src/core/logger.h"

#include <iomanip>
#include <sstream>
#include <chrono>

namespace sabit::app {

namespace {

std::string sanitize_filename(const std::string& input) {
    std::string clean = input;
    for (char& c : clean) {
        if (c == ' ' || c == ':' || c == '/' || c == '\\' || c == '?' || c == '*' || c == '|' || c == '<' || c == '>') {
            c = '_';
        }
    }
    return clean;
}

std::string get_date_str(const core::Clock::TimePoint& tp) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm gm{};
#if defined(_WIN32)
    gmtime_s(&gm, &tt);
#else
    gmtime_r(&tt, &gm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &gm);
    return std::string(buf);
}

} // namespace

Session::Session() = default;

Session::~Session() {
    finish();
}

bool Session::start(
    const std::string& base_captures_dir,
    const std::string& location,
    const std::string& target,
    const camera::CameraCaps& caps,
    const std::string& observer
) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (info_.is_active) {
        finish();
    }

    auto now = core::Clock::now_utc();
    std::string date_str = get_date_str(now);
    std::string clean_loc = sanitize_filename(location.empty() ? "Observation" : location);
    std::string clean_target = sanitize_filename(target.empty() ? "Hilal" : target);

    // TH-13: synthetic simulator sessions are explicitly tagged in folder name
    const std::string sim_prefix = (caps.driver == "sim") ? "[SIMULATOR]_" : "";
    std::string folder_name = sim_prefix + date_str + "_" + clean_loc + "_" + clean_target;
    std::filesystem::path base_path = base_captures_dir.empty() ? "Captures" : base_captures_dir;
    std::filesystem::path session_dir = base_path / folder_name;

    // Handle collision if folder already exists on the same day
    int counter = 1;
    while (std::filesystem::exists(session_dir)) {
        folder_name = sim_prefix + date_str + "_" + clean_loc + "_" + clean_target + "_" + std::to_string(counter++);
        session_dir = base_path / folder_name;
    }

    std::error_code ec;
    std::filesystem::create_directories(session_dir, ec);
    if (ec) {
        SABIT_LOG_ERROR("Failed to create session directory: ", session_dir.string(), " (", ec.message(), ")");
        return false;
    }

    info_.session_id = folder_name;
    info_.folder_name = folder_name;
    info_.folder_path = session_dir.string();
    info_.location = location;
    info_.target = target;
    info_.observer = observer;
    info_.start_time_utc = core::Clock::to_iso8601(now);
    info_.end_time_utc = "";
    info_.frames_captured = 0;
    info_.frames_recorded = 0;
    info_.detections_count = 0;
    info_.disk_usage_bytes = 0;
    info_.camera_driver = caps.driver;
    info_.camera_model = caps.model;
    info_.camera_serial = caps.serial;
    info_.is_active = true;

    // Open session.log
    std::filesystem::path log_path = session_dir / "session.log";
    log_stream_.open(log_path, std::ios::app);
    if (log_stream_.is_open()) {
        log_stream_ << "=== Sabit Session Log: " << info_.session_id << " ===\n";
        log_stream_ << "Started (UTC): " << info_.start_time_utc << "\n";
        log_stream_ << "Location: " << info_.location << ", Target: " << info_.target << ", Observer: " << info_.observer << "\n";
        log_stream_ << "Camera: " << info_.camera_model << " [" << info_.camera_driver << "] SN: " << info_.camera_serial << "\n";
        log_stream_ << "----------------------------------------\n";
        log_stream_.flush();
    }

    write_manifest();
    SABIT_LOG_INFO("Session started: ", info_.folder_path);
    return true;
}

void Session::finish() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!info_.is_active) return;

    info_.end_time_utc = core::Clock::to_iso8601(core::Clock::now_utc());
    info_.is_active = false;

    calculate_disk_usage();

    if (log_stream_.is_open()) {
        log_stream_ << "----------------------------------------\n";
        log_stream_ << "Finished (UTC): " << info_.end_time_utc << "\n";
        log_stream_ << "Total Captured: " << info_.frames_captured
                    << ", Recorded: " << info_.frames_recorded
                    << ", Detections: " << info_.detections_count << "\n";
        log_stream_ << "Total Disk Usage: " << (info_.disk_usage_bytes / 1024 / 1024) << " MB\n";
        log_stream_.flush();
        log_stream_.close();
    }

    write_manifest();
    SABIT_LOG_INFO("Session finished: ", info_.folder_path);
}

void Session::update_counts(uint64_t captured, uint64_t recorded, uint64_t detections) {
    std::lock_guard<std::mutex> lock(mutex_);
    info_.frames_captured = captured;
    info_.frames_recorded = recorded;
    info_.detections_count = detections;
}

void Session::calculate_disk_usage() {
    uint64_t total = 0;
    try {
        if (!info_.folder_path.empty() && std::filesystem::exists(info_.folder_path)) {
            for (const auto& entry : std::filesystem::directory_iterator(info_.folder_path)) {
                if (entry.is_regular_file()) {
                    total += entry.file_size();
                }
            }
        }
    } catch (...) {}
    info_.disk_usage_bytes = total;
}

void Session::write_manifest() {
    if (info_.folder_path.empty()) return;

    calculate_disk_usage();

    std::filesystem::path final_path = std::filesystem::path(info_.folder_path) / "session.json";
    std::filesystem::path temp_path = std::filesystem::path(info_.folder_path) / "session.json.tmp";

    std::ostringstream ss;
    ss << "{\n"
       << "  \"schema\": 2,\n"
       << "  \"session_id\": \"" << info_.session_id << "\",\n"
       << "  \"location\": \"" << info_.location << "\",\n"
       << "  \"target\": \"" << info_.target << "\",\n"
       << "  \"observer\": \"" << info_.observer << "\",\n"
       << "  \"start_time_utc\": \"" << info_.start_time_utc << "\",\n"
       << "  \"end_time_utc\": \"" << info_.end_time_utc << "\",\n"
       << "  \"is_active\": " << (info_.is_active ? "true" : "false") << ",\n"
       << "  \"data_source\": \"" << (info_.camera_driver == "sim" ? "[SIMULATOR] SYNTHETIC DATA - NOT A REAL OBSERVATION" : "hardware") << "\",\n"
       << "  \"is_simulator\": " << (info_.camera_driver == "sim" ? "true" : "false") << ",\n"
       << "  \"metrics\": {\n"
       << "    \"frames_captured\": " << info_.frames_captured << ",\n"
       << "    \"frames_recorded\": " << info_.frames_recorded << ",\n"
       << "    \"detections_count\": " << info_.detections_count << ",\n"
       << "    \"disk_usage_bytes\": " << info_.disk_usage_bytes << "\n"
       << "  },\n"
       << "  \"camera\": {\n"
       << "    \"driver\": \"" << info_.camera_driver << "\",\n"
       << "    \"model\": \"" << info_.camera_model << "\",\n"
       << "    \"serial\": \"" << info_.camera_serial << "\"\n"
       << "  }\n"
       << "}\n";

    std::ofstream out(temp_path, std::ios::trunc);
    if (out.is_open()) {
        out << ss.str();
        out.flush();
        out.close();

        std::error_code ec;
        std::filesystem::rename(temp_path, final_path, ec);
        if (ec) {
            std::filesystem::copy_file(temp_path, final_path, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(temp_path, ec);
        }
    }
}

void Session::log_event(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!log_stream_.is_open()) return;

    std::string time_str = core::Clock::to_iso8601(core::Clock::now_utc());
    log_stream_ << "[" << time_str << "] " << message << "\n";
    log_stream_.flush();
}

std::string Session::get_frame_filepath(const std::string& ext, uint64_t seq) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string safe_target = sanitize_filename(info_.target.empty() ? "Hilal" : info_.target);
    std::ostringstream ss;
    ss << info_.folder_path << "/"
       << safe_target << "_"
       << std::setw(6) << std::setfill('0') << seq
       << "." << ext;
    return ss.str();
}

std::string Session::get_log_filepath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return (std::filesystem::path(info_.folder_path) / "session.log").string();
}

std::string Session::get_detections_csv_filepath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return (std::filesystem::path(info_.folder_path) / "detections.csv").string();
}

std::string Session::get_manifest_filepath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return (std::filesystem::path(info_.folder_path) / "session.json").string();
}

SessionInfo Session::info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return info_;
}

bool Session::is_active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return info_.is_active;
}

bool Session::export_diagnostic_zip(const std::string& zip_dest_path, bool mask_coordinates) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (info_.folder_path.empty()) return false;

    // Create diagnostic archive by copying vital files to a diagnostic directory
    std::filesystem::path dest_dir = std::filesystem::path(zip_dest_path).replace_extension("");
    std::error_code ec;
    std::filesystem::create_directories(dest_dir, ec);

    std::filesystem::path dest_manifest = dest_dir / "session.json";
    if (std::filesystem::exists(get_manifest_filepath())) {
        std::filesystem::copy_file(get_manifest_filepath(), dest_manifest, std::filesystem::copy_options::overwrite_existing, ec);
        if (mask_coordinates) {
            std::ifstream in(dest_manifest);
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();
            // Redact location field
            size_t pos = content.find("\"location\":");
            if (pos != std::string::npos) {
                size_t start = content.find("\"", pos + 11);
                size_t end = (start != std::string::npos) ? content.find("\"", start + 1) : std::string::npos;
                if (start != std::string::npos && end != std::string::npos) {
                    content.replace(start + 1, end - start - 1, "[REDACTED]");
                }
            }
            std::ofstream out(dest_manifest, std::ios::trunc);
            out << content;
        }
    }

    if (std::filesystem::exists(get_log_filepath())) {
        std::filesystem::copy_file(get_log_filepath(), dest_dir / "session.log", std::filesystem::copy_options::overwrite_existing, ec);
    }
    if (std::filesystem::exists(get_detections_csv_filepath())) {
        std::filesystem::copy_file(get_detections_csv_filepath(), dest_dir / "detections.csv", std::filesystem::copy_options::overwrite_existing, ec);
    }
    if (std::filesystem::exists("settings.json")) {
        std::filesystem::path dest_settings = dest_dir / "settings.json";
        std::filesystem::copy_file("settings.json", dest_settings, std::filesystem::copy_options::overwrite_existing, ec);
        if (mask_coordinates) {
            std::ifstream in(dest_settings);
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();
            // Mask lat/lon
            size_t lat_pos = content.find("\"latitude\":");
            if (lat_pos != std::string::npos) {
                size_t comma = content.find_first_of(",\n\r}", lat_pos);
                if (comma != std::string::npos) {
                    content.replace(lat_pos, comma - lat_pos, "\"latitude\": 0.0");
                }
            }
            size_t lon_pos = content.find("\"longitude\":");
            if (lon_pos != std::string::npos) {
                size_t comma = content.find_first_of(",\n\r}", lon_pos);
                if (comma != std::string::npos) {
                    content.replace(lon_pos, comma - lon_pos, "\"longitude\": 0.0");
                }
            }
            std::ofstream out(dest_settings, std::ios::trunc);
            out << content;
        }
    }

    return true;
}

} // namespace sabit::app
