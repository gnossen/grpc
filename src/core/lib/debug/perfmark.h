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

#include <cstdint>
#include <grpc/support/port_platform.h>

namespace perfmark {
namespace internal {

static constexpr uint64_t kDefaultCircularArraySize = 1 << 16;

template <typename T, uint64_t size = kDefaultCircularArraySize>
class CircularArray {
public:
    CircularArray(): data() {}

private:
    T data[kDefaultCircularArraySize];
};

}  // end namespace internal
} // end namespace perfmark

#endif
