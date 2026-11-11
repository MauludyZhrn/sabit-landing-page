#ifndef SABIT_CORE_ERROR_H
#define SABIT_CORE_ERROR_H

#include <string>
#include <string_view>

namespace sabit::core {

enum class ErrorCode {
    Ok = 0,
    NotFound,
    Busy,
    DriverMissing,
    PermissionDenied,
    OutOfRange,
    Unsupported,
    HardwareError,
    Timeout,
    BufferFull,
    DiskFull,
    InvalidState,
    SerializationError,
    Unknown
};

inline std::string_view error_code_to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::Ok: return "Ok";
        case ErrorCode::NotFound: return "NotFound";
        case ErrorCode::Busy: return "Busy";
        case ErrorCode::DriverMissing: return "DriverMissing";
        case ErrorCode::PermissionDenied: return "PermissionDenied";
        case ErrorCode::OutOfRange: return "OutOfRange";
        case ErrorCode::Unsupported: return "Unsupported";
        case ErrorCode::HardwareError: return "HardwareError";
        case ErrorCode::Timeout: return "Timeout";
        case ErrorCode::BufferFull: return "BufferFull";
        case ErrorCode::DiskFull: return "DiskFull";
        case ErrorCode::InvalidState: return "InvalidState";
        case ErrorCode::SerializationError: return "SerializationError";
        default: return "Unknown";
    }
}

struct Error {
    ErrorCode code{ErrorCode::Unknown};
    std::string message;

    Error() = default;
    Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}
    Error(ErrorCode c) : code(c), message(std::string(error_code_to_string(c))) {}

    [[nodiscard]] bool ok() const { return code == ErrorCode::Ok; }
};

} // namespace sabit::core

#endif // SABIT_CORE_ERROR_H
