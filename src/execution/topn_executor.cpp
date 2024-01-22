#include "execution/executors/topn_executor.h"

namespace bustub {

TopNExecutor::TopNExecutor(ExecutorContext *exec_ctx, const TopNPlanNode *plan,
                           std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void TopNExecutor::Init() {
  child_executor_->Init();

  if (!sorted_tuples_.empty()) {
    sorted_tuples_index_ = 0;
    return;
  }

  Tuple tuple;
  RID rid;
  while (child_executor_->Next(&tuple, &rid)) {
    sorted_tuples_.emplace_back(tuple);
  }

  std::sort(sorted_tuples_.begin(), sorted_tuples_.end(),
            [order_bys = plan_->GetOrderBy(), schema = GetOutputSchema()](const Tuple &tuple_a,
                                                                          const Tuple &tuple_b) -> bool {
              for (const auto &order_by : order_bys) {
                auto value_a = order_by.second->Evaluate(&tuple_a, schema);
                auto value_b = order_by.second->Evaluate(&tuple_b, schema);
                bustub::CmpBool result1 = value_a.CompareLessThan(value_b);
                bustub::CmpBool result2 = value_a.CompareGreaterThan(value_b);
                switch (order_by.first) {
                  case bustub::OrderByType::INVALID:
                    return true;
                  case bustub::OrderByType::DEFAULT:
                  case bustub::OrderByType::ASC:
                    if (result1 == bustub::CmpBool::CmpTrue) {
                      return true;
                    }
                    if (result2 == bustub::CmpBool::CmpTrue) {
                      return false;
                    }
                    break;
                  case bustub::OrderByType::DESC:
                    if (result2 == bustub::CmpBool::CmpTrue) {
                      return true;
                    }
                    if (result1 == bustub::CmpBool::CmpTrue) {
                      return false;
                    }
                    break;
                }
              }
              return true;
            });

  sorted_tuples_index_ = 0;
}

auto TopNExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (sorted_tuples_index_ == sorted_tuples_.size() || sorted_tuples_index_ == plan_->GetN()) {
    return false;
  }
  *tuple = sorted_tuples_[sorted_tuples_index_];
  *rid = tuple->GetRid();
  sorted_tuples_index_++;
  return true;
}

}  // namespace bustub
