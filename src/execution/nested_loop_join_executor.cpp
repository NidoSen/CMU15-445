//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// nested_loop_join_executor.cpp
//
// Identification: src/execution/nested_loop_join_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/nested_loop_join_executor.h"
#include "binder/table_ref/bound_join_ref.h"
#include "common/exception.h"

namespace bustub {

NestedLoopJoinExecutor::NestedLoopJoinExecutor(ExecutorContext *exec_ctx, const NestedLoopJoinPlanNode *plan,
                                               std::unique_ptr<AbstractExecutor> &&left_executor,
                                               std::unique_ptr<AbstractExecutor> &&right_executor)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      left_executor_(std::move(left_executor)),
      right_executor_(std::move(right_executor)) {
  if (!(plan->GetJoinType() == JoinType::LEFT || plan->GetJoinType() == JoinType::INNER)) {
    // Note for 2022 Fall: You ONLY need to implement left join and inner join.
    throw bustub::NotImplementedException(fmt::format("join type {} not supported", plan->GetJoinType()));
  }
}

void NestedLoopJoinExecutor::Init() {
  left_executor_->Init();
  right_executor_->Init();

  // LOG_DEBUG("%s", left_executor_->GetOutputSchema().ToString().c_str());
  // LOG_DEBUG("%s", right_executor_->GetOutputSchema().ToString().c_str());
  // LOG_DEBUG("%s", GetOutputSchema().ToString().c_str());

  left_status_ = left_executor_->Next(&left_tuple_, &left_rid_);
  left_find_ = false;
}

auto NestedLoopJoinExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  auto filter_expr = &(plan_->Predicate());

  while (left_status_) {
    right_status_ = right_executor_->Next(&right_tuple_, &right_rid_);

    // LOG_DEBUG("%s", left_tuple_.ToString(&(left_executor_->GetOutputSchema())).c_str());
    // LOG_DEBUG("%s", right_tuple_.ToString(&(right_executor_->GetOutputSchema())).c_str());

    if (right_status_) {
      auto value = filter_expr->EvaluateJoin(&left_tuple_, left_executor_->GetOutputSchema(), &right_tuple_,
                                             right_executor_->GetOutputSchema());
      if (!value.IsNull() && value.GetAs<bool>()) {
        left_find_ = true;
        std::vector<Value> values{};
        values.reserve(GetOutputSchema().GetColumnCount());
        for (size_t i = 0; i < left_executor_->GetOutputSchema().GetColumnCount(); i++) {
          values.push_back(left_tuple_.GetValue(&(left_executor_->GetOutputSchema()), i));
        }
        for (size_t i = 0; i < right_executor_->GetOutputSchema().GetColumnCount(); i++) {
          values.push_back(right_tuple_.GetValue(&(right_executor_->GetOutputSchema()), i));
        }

        *tuple = Tuple(values, &GetOutputSchema());
        return true;
      }
      continue;
    }

    Tuple temp_tuple = left_tuple_;

    left_status_ = left_executor_->Next(&left_tuple_, &left_rid_);
    right_executor_->Init();

    if (!left_find_ && plan_->GetJoinType() == JoinType::LEFT) {
      std::vector<Value> values{};
      values.reserve(GetOutputSchema().GetColumnCount());
      for (size_t i = 0; i < left_executor_->GetOutputSchema().GetColumnCount(); i++) {
        values.push_back(temp_tuple.GetValue(&(left_executor_->GetOutputSchema()), i));
      }
      for (size_t i = 0; i < right_executor_->GetOutputSchema().GetColumnCount(); i++) {
        values.push_back(ValueFactory::GetNullValueByType(right_executor_->GetOutputSchema().GetColumn(i).GetType()));
      }

      *tuple = Tuple(values, &GetOutputSchema());
      left_find_ = false;
      return true;
    }

    left_find_ = false;
  }

  return false;
}

}  // namespace bustub
