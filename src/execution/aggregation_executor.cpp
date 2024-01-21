//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// aggregation_executor.cpp
//
// Identification: src/execution/aggregation_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include <memory>
#include <vector>

#include "execution/executors/aggregation_executor.h"

namespace bustub {

AggregationExecutor::AggregationExecutor(ExecutorContext *exec_ctx, const AggregationPlanNode *plan,
                                         std::unique_ptr<AbstractExecutor> &&child)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      child_(std::move(child)),
      aht_(plan_->GetAggregates(), plan_->GetAggregateTypes()),
      aht_iterator_(aht_.Begin()) {}

void AggregationExecutor::Init() {
  child_->Init();
  aht_.Clear();
  Tuple tuple;
  RID rid;
  while (child_->Next(&tuple, &rid)) {
    // LOG_DEBUG("%s", tuple.ToString(&GetOutputSchema()).c_str());
    // LOG_DEBUG("%s", tuple.ToString(&(child_->GetOutputSchema())).c_str());
    // LOG_DEBUG("%ld", MakeAggregateKey(&tuple).group_bys_.size());
    // LOG_DEBUG("%ld", MakeAggregateValue(&tuple).aggregates_.size());
    // LOG_DEBUG("%s", MakeAggregateKey(&tuple).group_bys_[0].ToString().c_str());
    // LOG_DEBUG("%s", MakeAggregateValue(&tuple).aggregates_[0].ToString().c_str());
    aht_.InsertCombine(MakeAggregateKey(&tuple), MakeAggregateValue(&tuple));
  }
  aht_iterator_ = aht_.Begin();
  if (aht_iterator_ == aht_.End() && GetOutputSchema().GetColumnCount() != 1) {
    is_end_ = true;
    return;
  }
  is_end_ = false;
}

auto AggregationExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (is_end_) {
    return false;
  }
  if (aht_iterator_ == aht_.End()) {
    std::vector<Value> values{};
    values.reserve(GetOutputSchema().GetColumnCount());
    auto agg_keys = MakeAggregateKey(tuple).group_bys_;
    auto agg_vals = aht_.GenerateInitialAggregateValue().aggregates_;
    for (const auto &agg_key : agg_keys) {
      values.push_back(agg_key);
    }
    for (const auto &agg_val : agg_vals) {
      values.push_back(agg_val);
    }
    *tuple = Tuple(values, &GetOutputSchema());
    is_end_ = true;
  } else {
    // LOG_DEBUG("%ld %d", aht_iterator_.Val().aggregates_.size(), child_->GetOutputSchema().GetColumnCount());
    std::vector<Value> values{};
    values.reserve(GetOutputSchema().GetColumnCount());
    for (const auto &agg_key : aht_iterator_.Key().group_bys_) {
      values.push_back(agg_key);
    }
    for (const auto &agg_val : aht_iterator_.Val().aggregates_) {
      values.push_back(agg_val);
    }

    *tuple = Tuple(values, &GetOutputSchema());
    ++aht_iterator_;
    if (aht_iterator_ == aht_.End()) {
      is_end_ = true;
    }
  }
  return true;
}

auto AggregationExecutor::GetChildExecutor() const -> const AbstractExecutor * { return child_.get(); }

}  // namespace bustub
