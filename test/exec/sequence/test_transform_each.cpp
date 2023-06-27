/*
 * Copyright (c) 2023 Maikel Nadolski
 * Copyright (c) 2023 NVIDIA Corporation
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "exec/sequence/transform_each.hpp"

#include <catch2/catch.hpp>

struct next_rcvr {
  using __id = next_rcvr;
  using __t = next_rcvr;
  using is_receiver = void;

  friend auto tag_invoke(exec::set_next_t, next_rcvr, auto item) {
    return item;
  }

  friend void tag_invoke(stdexec::set_value_t, next_rcvr) noexcept {
  }

  friend stdexec::empty_env tag_invoke(stdexec::get_env_t, next_rcvr) noexcept {
    return {};
  }
};

TEST_CASE(
  "transform_each - transform sender applies adaptor",
  "[sequence_senders][transform_each][empty_sequence]") {
  int value = 0;
  auto transformed = exec::transform_each(
    stdexec::just(42), stdexec::then([&value](int x) noexcept { value = x; }));
  auto op = exec::subscribe(transformed, next_rcvr{});
  stdexec::start(op);
  CHECK(value == 42);
}