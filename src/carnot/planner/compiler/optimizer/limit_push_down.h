#pragma once

#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/rules/rules.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief This rule pushes limits as early in the IR as possible.
 *
 */
class LimitPushdownRule : public Rule {
 public:
  LimitPushdownRule() : Rule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode*) override;

 private:
  absl::flat_hash_set<OperatorIR*> NewLimitParents(OperatorIR* current_node, int64_t limit_value);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
