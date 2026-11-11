#ifndef SABIT_CORE_LATEST_SLOT_H
#define SABIT_CORE_LATEST_SLOT_H

#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <cstdint>

namespace sabit::core {

template <typename T>
class LatestSlot {
public:
    LatestSlot() = default;
    ~LatestSlot() {
        shutdown();
    }

    LatestSlot(const LatestSlot&) = delete;
    LatestSlot& operator=(const LatestSlot&) = delete;

    /**
     * Put an item into the slot.
     * Always replaces any previous item (never blocks producer).
     */
    void put(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            item_ = std::move(item);
            has_new_ = true;
            ++version_;
        }
        cv_.notify_all();
    }

    /**
     * Get a copy of the latest item if present.
     * Does not clear the slot. Marks has_new as false.
     */
    std::optional<T> get_latest() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!item_.has_value()) {
            return std::nullopt;
        }
        has_new_ = false;
        return item_;
    }

    /**
     * Move and take the latest item out of the slot.
     * Slot becomes empty.
     */
    std::optional<T> take_latest() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!item_.has_value()) {
            return std::nullopt;
        }
        std::optional<T> res = std::move(item_);
        item_.reset();
        has_new_ = false;
        return res;
    }

    /**
     * Wait for a new item to arrive.
     * @param timeout wait duration (if < 0, waits indefinitely)
     * @return copy of the new item if arrived, or nullopt on timeout/shutdown.
     */
    std::optional<T> wait_new(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (timeout < std::chrono::milliseconds(0)) {
            cv_.wait(lock, [this]() {
                return has_new_ || shutdown_;
            });
        } else {
            cv_.wait_for(lock, timeout, [this]() {
                return has_new_ || shutdown_;
            });
        }

        if (shutdown_ || !has_new_ || !item_.has_value()) {
            return std::nullopt;
        }

        has_new_ = false;
        return item_;
    }

    [[nodiscard]] bool has_new() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return has_new_;
    }

    [[nodiscard]] bool is_empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return !item_.has_value();
    }

    [[nodiscard]] uint64_t version() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return version_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        item_.reset();
        has_new_ = false;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }

    void restart() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = false;
    }

private:
    std::optional<T> item_;
    bool has_new_{false};
    uint64_t version_{0};
    bool shutdown_{false};
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace sabit::core

#endif // SABIT_CORE_LATEST_SLOT_H
