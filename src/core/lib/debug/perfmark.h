/*
 *
 * Copyright 2019, the gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef GRPC_CORE_LIB_DEBUG_PERFMARK_H
#define GRPC_CORE_LIB_DEBUG_PERFMARK_H

#include <atomic>
#include <cstdint>
#include <grpc/support/port_platform.h>

namespace perfmark {

namespace internal {

static constexpr uint64_t kDefaultCircularArraySize = 1 << 16;

// Single writer, single reader circular array.
// Often written and rarely read.
template <typename T, uint64_t size = kDefaultCircularArraySize>
class CircularArray {
public:
    CircularArray(): data_(), write_marker_(0) {}

    void insert(const T& datum) {
        data_[write_marker_.load(std::memory_order_relaxed) % size] = datum;
        write_marker_.fetch_add(1, std::memory_order_acq_rel);
    }

    // data_out is expected to be at least of size `size`.
    // returns the last index read.
    uint64_t read(uint64_t last_read, T* data_out, uint64_t* corrupted_size, uint64_t* read_count) {
        uint64_t first_observed_write = write_marker_.load(std::memory_order_acquire);
        T* it = data_out;
        // In case we've missed whole laps around the array, trim.
        last_read = ((first_observed_write - 1) / size) * size + (last_read % size);
        for (size_t i = last_read; i < first_observed_write; ++i) {
           *(it++) = data_[i % size];
        }
        uint64_t last_observed_write = write_marker_.load(std::memory_order_acquire);
        // Figure out what might be corrupted.
        if ((last_observed_write - first_observed_write) > size - (last_read % size)) {
            *corrupted_size = (last_observed_write % size) - (last_read % size);
            *read_count = (first_observed_write - last_read) - *corrupted_size;
        } else {
            *corrupted_size = 0;
            *read_count = first_observed_write - last_read;
        }
        return first_observed_write;
    }

private:
    T data_[kDefaultCircularArraySize];
    std::atomic<uint64_t> write_marker_;
};

}  // end namespace internal

typedef struct {
    uint64_t id;
    uint64_t timestamp;
    uint64_t link_id;
} Tag ;

Tag create_tag(uint64_t id, uint64_t link_id = 0) {
    Tag tag;
    tag.id = id;
    // Using clang intrinsic for the moment.
    tag.timestamp = __rdtsc();
    tag.link_id = link_id;
    return tag;
}

} // end namespace perfmark

#endif
