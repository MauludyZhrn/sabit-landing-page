#ifndef SABIT_CORE_RING_BUFFER_H
#define SABIT_CORE_RING_BUFFER_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <cstdint>

namespace sabit::core {

template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : capacity_(capacity > 0 ? capacity : 1),
          buffer_(capacity > 0 ? capacity : 1) {}

    ~RingBuffer() {
        shutdown();
    }

    // Disable copy
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    /**
     * Push an item into the buffer.
     * If the buffer is full, the OLDEST item is dropped (policy: drop-oldest)
     * and the dropped counter is incremented.
     * @return true if an older item was dropped, false otherwise.
     */
    bool push(T item) {
        bool dropped = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (shutdown_) {
                return false;
            }

            if (count_ == capacity_) {
                // Buffer is full: advance head to drop the oldest item
                head_ = (head_ + 1) % capacity_;
                --count_;
                ++dropped_count_;
                dropped = true;
            }

            buffer_[tail_] = std::move(item);
            tail_ = (tail_ + 1) % capacity_;
            ++count_;
        }
        not_empty_cv_.notify_one();
        return dropped;
    }

    /**
     * Pop the next item from the buffer.
     * If timeout > 0, waits up to timeout for an item.
     * If timeout == 0, returns immediately (non-blocking).
     * If timeout < 0, waits indefinitely until an item arrives or shutdown.
     */
    std::optional<T> pop(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (timeout < std::chrono::milliseconds(0)) {
            not_empty_cv_.wait(lock, [this]() {
                return count_ > 0 || shutdown_;
            });
        } else if (timeout > std::chrono::milliseconds(0)) {
            not_empty_cv_.wait_for(lock, timeout, [this]() {
                return count_ > 0 || shutdown_;
            });
        }

        if (count_ == 0 || shutdown_) {
            if (count_ == 0) {
                return std::nullopt;
            }
        }

        T item = std::move(buffer_[head_]);
        head_ = (head_ + 1) % capacity_;
        --count_;
        return item;
    }

    std::optional<T> try_pop() {
        return pop(std::chrono::milliseconds(0));
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    [[nodiscard]] size_t capacity() const noexcept {
        return capacity_;
    }

    [[nodiscard]] bool is_empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ == 0;
    }

    [[nodiscard]] bool is_full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ == capacity_;
    }

    [[nodiscard]] uint64_t dropped_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return dropped_count_;
    }

    void reset_dropped_count() {
        std::lock_guard<std::mutex> lock(mutex_);
        dropped_count_ = 0;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = 0;
        tail_ = 0;
        count_ = 0;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        not_empty_cv_.notify_all();
    }

    void restart() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = false;
    }

private:
    const size_t capacity_;
    std::vector<T> buffer_;
    size_t head_{0};
    size_t tail_{0};
    size_t count_{0};
    uint64_t dropped_count_{0};
    bool shutdown_{false};
    mutable std::mutex mutex_;
    std::condition_variable not_empty_cv_;
};

} // namespace sabit::core

#endif // SABIT_CORE_RING_BUFFER_H
