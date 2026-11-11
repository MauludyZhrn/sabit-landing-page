#include "src/core/frame.h"

namespace sabit::core {

Frame Frame::create_allocated(int width, int height, int channels, int bit_depth) {
    Frame f;
    f.width = width;
    f.height = height;
    f.channels = channels;
    f.bit_depth = bit_depth;
    size_t count = f.pixel_count();
    if (count > 0) {
        f.pixels = std::shared_ptr<uint16_t[]>(new uint16_t[count]());
    }
    f.timestamp_utc = Clock::now_utc();
    f.clock_sync = Clock::get_sync();
    return f;
}

Frame Frame::create_from_pool(FramePool& pool, int width, int height, int channels, int bit_depth) {
    Frame f;
    f.width = width;
    f.height = height;
    f.channels = channels;
    f.bit_depth = bit_depth;
    f.pixels = pool.acquire();
    f.timestamp_utc = Clock::now_utc();
    f.clock_sync = Clock::get_sync();
    return f;
}

FramePool::FramePool(size_t buffer_element_count, size_t initial_pool_size, size_t max_pool_size)
    : element_count_(buffer_element_count), max_size_(max_pool_size) {
    free_list_.reserve(max_pool_size);
    for (size_t i = 0; i < initial_pool_size; ++i) {
        free_list_.push_back(std::make_unique<uint16_t[]>(element_count_));
        ++total_allocated_;
    }
}

std::shared_ptr<uint16_t[]> FramePool::acquire() {
    uint16_t* raw_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!free_list_.empty()) {
            raw_ptr = free_list_.back().release();
            free_list_.pop_back();
        } else if (total_allocated_ < max_size_) {
            raw_ptr = new uint16_t[element_count_]();
            ++total_allocated_;
        } else {
            // Allocate fallback when pool exhausted
            raw_ptr = new uint16_t[element_count_]();
        }
    }

    std::weak_ptr<FramePool> weak_self = shared_from_this();
    return std::shared_ptr<uint16_t[]>(raw_ptr, [weak_self](uint16_t* p) {
        if (auto self = weak_self.lock()) {
            self->release(p);
        } else {
            delete[] p;
        }
    });
}

void FramePool::release(uint16_t* ptr) {
    if (!ptr) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (free_list_.size() < max_size_) {
        free_list_.push_back(std::unique_ptr<uint16_t[]>(ptr));
    } else {
        delete[] ptr;
        if (total_allocated_ > 0) {
            --total_allocated_;
        }
    }
}

size_t FramePool::available_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return free_list_.size();
}

size_t FramePool::total_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_allocated_;
}

} // namespace sabit::core
