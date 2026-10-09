module;
#include <benchmark/benchmark.h>

export module interop.benchmark;

export namespace benchmark {
using ::benchmark::Initialize;
using ::benchmark::MaybeReenterWithoutASLR;
using ::benchmark::RegisterBenchmark;
using ::benchmark::ReportUnrecognizedArguments;
using ::benchmark::RunSpecifiedBenchmarks;
using ::benchmark::Shutdown;
using ::benchmark::State;
}  // namespace benchmark
