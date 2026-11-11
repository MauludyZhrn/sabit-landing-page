#ifndef SABIT_CORE_RESULT_H
#define SABIT_CORE_RESULT_H

#include "src/core/error.h"
#include <variant>
#include <stdexcept>
#include <utility>

namespace sabit::core {

template <typename T>
class Result {
public:
    Result(const T& val) : data_(val) {}
    Result(T&& val) : data_(std::move(val)) {}
    Result(Error err) : data_(std::move(err)) {}
    Result(ErrorCode code) : data_(Error(code)) {}

    static Result<T> ok(T val) {
        return Result<T>(std::move(val));
    }

    static Result<T> err(Error error) {
        return Result<T>(std::move(error));
    }

    static Result<T> err(ErrorCode code, std::string msg = "") {
        return Result<T>(Error(code, std::move(msg)));
    }

    [[nodiscard]] bool is_ok() const noexcept {
        return std::holds_alternative<T>(data_);
    }

    [[nodiscard]] bool is_err() const noexcept {
        return std::holds_alternative<Error>(data_);
    }

    explicit operator bool() const noexcept {
        return is_ok();
    }

    [[nodiscard]] T& value() & {
        if (is_err()) {
            throw std::runtime_error("Attempted to access value of an Error Result: " + error().message);
        }
        return std::get<T>(data_);
    }

    [[nodiscard]] const T& value() const & {
        if (is_err()) {
            throw std::runtime_error("Attempted to access value of an Error Result: " + error().message);
        }
        return std::get<T>(data_);
    }

    [[nodiscard]] T&& value() && {
        if (is_err()) {
            throw std::runtime_error("Attempted to access value of an Error Result: " + error().message);
        }
        return std::get<T>(std::move(data_));
    }

    [[nodiscard]] const Error& error() const noexcept {
        static const Error default_ok(ErrorCode::Ok);
        if (is_ok()) {
            return default_ok;
        }
        return std::get<Error>(data_);
    }

    [[nodiscard]] T value_or(T default_val) const {
        if (is_ok()) {
            return std::get<T>(data_);
        }
        return default_val;
    }

private:
    std::variant<T, Error> data_;
};

// Specialization for void
template <>
class Result<void> {
public:
    Result() : error_(ErrorCode::Ok) {}
    Result(Error err) : error_(std::move(err)) {}
    Result(ErrorCode code) : error_(code) {}

    static Result<void> ok() {
        return Result<void>();
    }

    static Result<void> err(Error error) {
        return Result<void>(std::move(error));
    }

    static Result<void> err(ErrorCode code, std::string msg = "") {
        return Result<void>(Error(code, std::move(msg)));
    }

    [[nodiscard]] bool is_ok() const noexcept {
        return error_.ok();
    }

    [[nodiscard]] bool is_err() const noexcept {
        return !error_.ok();
    }

    explicit operator bool() const noexcept {
        return is_ok();
    }

    [[nodiscard]] const Error& error() const noexcept {
        return error_;
    }

private:
    Error error_;
};

} // namespace sabit::core

#endif // SABIT_CORE_RESULT_H
