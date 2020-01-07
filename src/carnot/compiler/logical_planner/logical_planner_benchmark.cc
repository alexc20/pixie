#include <benchmark/benchmark.h>

#include "src/carnot/compiler/logical_planner/logical_planner.h"
#include "src/carnot/compiler/logical_planner/test_utils.h"
#include "src/common/base/test_utils.h"
#include "src/common/perf/perf.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace logical_planner {

// NOLINTNEXTLINE : runtime/references.
void BM_Query(benchmark::State& state, bool distributed) {
  auto planner = LogicalPlanner::Create(distributed).ConsumeValueOrDie();
  auto planner_state =
      testutils::CreateTwoAgentsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  for (auto _ : state) {
    auto plan_or_s = planner->Plan(planner_state, testutils::kHttpRequestStats);
    EXPECT_OK(plan_or_s);
  }
}

BENCHMARK_CAPTURE(BM_Query, distributed, true);
BENCHMARK_CAPTURE(BM_Query, non_distributed, false);

}  // namespace logical_planner
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
