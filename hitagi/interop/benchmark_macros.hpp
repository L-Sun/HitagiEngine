// Copyright 2015 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Adapted from benchmark/benchmark.h: retain only caller-side registration/main.
#pragma once

// Registration and main must be declared in the importing translation unit.
#define HITAGI_BENCHMARK_JOIN_(a, b) a##b
#define HITAGI_BENCHMARK_JOIN(a, b) HITAGI_BENCHMARK_JOIN_(a, b)
#define BENCHMARK(function) \
    [[maybe_unused]] static auto* HITAGI_BENCHMARK_JOIN(hitagi_benchmark_, __LINE__) = ::benchmark::RegisterBenchmark(#function, function)
#define BENCHMARK_MAIN()                                                    \
    int main(int argc, char** argv) {                                       \
        ::benchmark::MaybeReenterWithoutASLR(argc, argv);                   \
        char  arg0[] = "benchmark";                                         \
        char* args   = arg0;                                                \
        if (!argv) {                                                        \
            argc = 1;                                                       \
            argv = &args;                                                   \
        }                                                                   \
        ::benchmark::Initialize(&argc, argv);                               \
        if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1; \
        ::benchmark::RunSpecifiedBenchmarks();                              \
        ::benchmark::Shutdown();                                            \
        return 0;                                                           \
    }
