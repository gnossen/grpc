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
#include <stdexcept>
#include <thread>
#include <grpc/support/port_platform.h>

#include <iostream>

namespace perfmark {

static constexpr uint64_t kDefaultCircularArraySize = 1 << 16;

namespace internal {

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
        for (uint64_t i = last_read; i < first_observed_write; ++i) {
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
    T data_[size];
    std::atomic<uint64_t> write_marker_;
};

}  // end namespace internal

typedef enum {
  START_OP = 1,
  END_OP = 2
} TagType;

typedef struct {
    uint64_t id;
    uint64_t thread_id;
    uint64_t timestamp;
    uint64_t link_id;
    TagType tag_type;
} Tag;

// TODO: These should be in the internal namespace.
thread_local internal::CircularArray<Tag>* g_tag_store = nullptr;
thread_local uint64_t g_thread_id = 0;
static internal::CircularArray<Tag> ** g_threadlocal_tag_stores = nullptr;
static std::atomic<uint64_t> g_next_thread_id {0};

Tag create_tag(uint64_t id, uint64_t link_id, TagType tag_type) {
    Tag tag;
    tag.id = id;
    tag.thread_id = g_thread_id;
    // Using clang intrinsic for the moment.
    tag.timestamp = __rdtsc();
    tag.link_id = link_id;
    tag.tag_type = tag_type;
    return tag;
}

// Only the collection thread ever touches these, so no locking is needed.
static Tag * g_global_tag_store;
static Tag ** g_per_thread_buffers;


// TODO: Move to internal namespace.
void CollectionThread() {
  // TODO: Clean this up.
  uint64_t* last_reads = new uint64_t[std::thread::hardware_concurrency()];
  for (uint64_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
    last_reads[i] = 0;
  }
  uint64_t* corrupted_sizes = new uint64_t[std::thread::hardware_concurrency()];
  for (uint64_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
    corrupted_sizes[i] = 0;
  }
  uint64_t* read_counts = new uint64_t[std::thread::hardware_concurrency()];
  for (uint64_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
    read_counts[i] = 0;
  }
  for (;;) {
    const uint64_t thread_count = g_next_thread_id.load(std::memory_order_acquire)
    for (uint64_t i = 0; i < thread_count; ++i) {
      // Copy into a non-racy buffer.
      g_threadlocal_tag_stores[i]->read(&last_reads[i], g_per_thread_buffers[i], &corrupted_sizes[i], &read_counts[i]);
    }
    // Merge all buffers into a single, totally chronologically-ordered
    // buffer.

  }
}


void StartCollectionThread() {
  // TODO: Consider the case where gRPC is brought completely down then back up
  // in the same process.
  if (g_global_tag_store != nullptr || g_per_thread_buffers != nullptr || g_threadlocal_tag_stores != nullptr) {
    throw std::runtime_error("");
  }
  g_global_tag_store = new Tag[std::thread::hardware_concurrency() * kDefaultCircularArraySize];
  g_per_thread_buffers = new Tag*[std::thread::hardware_concurrency()];
  g_threadlocal_tag_stores = new internal::CircularArray<Tag>*[std::thread::hardware_concurrency()];
  for (uint64_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
    g_per_thread_buffers[i] = new Tag[kDefaultCircularArraySize];
  }
  // TODO: Figure out deactivating the thread.
  std::thread collection_thread(&StartCollectionThread);
  collection_thread.detach();
}

void InitThread() {
     if (g_tag_store != nullptr) {
        // TODO: Do whatever OSS Googlers do when they can't throw. Which is... what exactly?
        throw std::runtime_error("Ahhhhhh!");
     }
     g_tag_store = new internal::CircularArray<Tag>();
     g_thread_id = g_next_thread_id.fetch_add(1, std::memory_order_acq_rel);
     // Add a pointer to this thread's circular array somewhere the collection
     // thread can get to it.
     g_threadlocal_tag_stores[g_thread_id] = g_tag_store;
}

void ShutdownThread() {
     if (g_tag_store == nullptr) {
        // TODO: Do whatever OSS Googlers do when they can't throw. Which is... what exactly?
        throw std::runtime_error("Ahhhhhh!");
     }
     delete g_tag_store;
     // Do not decrement the next_thread ID so we can keep uniqueness of IDs.
}


class Task {
public:
    Task(uint64_t task_id) : id_(task_id) {
        // We assume that InitThread() has already been called.
        // TODO: Actually fill out the tag.
        g_tag_store->insert(create_tag(id_, 0, START_OP));
    }

    ~Task() {
        g_tag_store->insert(create_tag(id_, 0, END_OP));
    }

private:
    uint64_t id_;
};

} // end namespace perfmark

#endif
