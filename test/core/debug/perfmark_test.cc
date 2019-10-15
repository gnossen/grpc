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

#include "src/core/lib/debug/perfmark.h"
#include <gtest/gtest.h>

TEST(PerfmarkTest, InstantiateCircularArray) {
    static constexpr uint64_t test_range = 20;
    uint64_t* out_buffer = new uint64_t[::perfmark::internal::kDefaultCircularArraySize];
    uint64_t corrupted_size = 0;
    uint64_t read_count = 0;
    auto* circular_array = new ::perfmark::internal::CircularArray<uint64_t>;
    for (uint64_t i = 0; i < test_range; ++i) {
        circular_array->insert(i);
    }
    circular_array->read(0, out_buffer, &corrupted_size, &read_count);
    for (uint64_t i = 0; i < test_range; ++i) {
        EXPECT_EQ(i, out_buffer[i]);
    }
    EXPECT_EQ(0, corrupted_size);
}

TEST(Perfmark, InstantiateTag) {
   auto tag = ::perfmark::create_tag(0, 1);
   auto tag2 = ::perfmark::create_tag(0, 2);
   EXPECT_EQ(0, tag.id);
   EXPECT_EQ(0, tag2.id);
   EXPECT_GT(tag2.timestamp, tag.timestamp);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
