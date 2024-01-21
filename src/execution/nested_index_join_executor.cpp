//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// nested_index_join_executor.cpp
//
// Identification: src/execution/nested_index_join_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/nested_index_join_executor.h"

namespace bustub {

NestIndexJoinExecutor::NestIndexJoinExecutor(ExecutorContext *exec_ctx, const NestedIndexJoinPlanNode *plan,
                                             std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      child_executor_(std::move(child_executor)),
      inner_table_info_(exec_ctx_->GetCatalog()->GetTable(plan_->GetInnerTableOid())),
      index_info_(exec_ctx_->GetCatalog()->GetIndex(plan_->GetIndexOid())),
      tree_(dynamic_cast<BPlusTreeIndexForOneIntegerColumn *>(index_info_->index_.get())),
      index_iter_(tree_->GetBeginIterator()) {
  if (!(plan->GetJoinType() == JoinType::LEFT || plan->GetJoinType() == JoinType::INNER)) {
    // Note for 2022 Fall: You ONLY need to implement left join and inner join.
    throw bustub::NotImplementedException(fmt::format("join type {} not supported", plan->GetJoinType()));
  }
}

void NestIndexJoinExecutor::Init() {
  child_executor_->Init();
  // LOG_DEBUG("%s", GetOutputSchema().ToString().c_str());
  // LOG_DEBUG("%s", child_executor_->GetOutputSchema().ToString().c_str());
  // LOG_DEBUG("%s", plan_->InnerTableSchema().ToString().c_str());
  // LOG_DEBUG("%s", plan_->GetIndexName().c_str());
  // LOG_DEBUG("%s", index_info_->index_->ToString().c_str());

  index_iter_ = tree_->GetBeginIterator();
  left_status_ = child_executor_->Next(&left_tuple_, &left_rid_);
  left_find_ = false;
}

auto NestIndexJoinExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  auto filter_expr = plan_->KeyPredicate();

  // LOG_DEBUG("%s", filter_expr->ToString().c_str());
  while (left_status_) {
    if (index_iter_ != tree_->GetEndIterator()) {
      right_rid_ = (*index_iter_).second;
      inner_table_info_->table_->GetTuple(right_rid_, &right_tuple_, exec_ctx_->GetTransaction());
      ++index_iter_;
      auto left_value = filter_expr->Evaluate(&left_tuple_, child_executor_->GetOutputSchema());
      auto key_right_tuple = right_tuple_.KeyFromTuple(
          plan_->InnerTableSchema(), *(index_info_->index_->GetKeySchema()), index_info_->index_->GetKeyAttrs());
      auto right_value = key_right_tuple.GetValue(index_info_->index_->GetKeySchema(), 0);

      if (left_value.CompareEquals(right_value) == bustub::CmpBool::CmpTrue) {
        left_find_ = true;
        std::vector<Value> values{};
        values.reserve(GetOutputSchema().GetColumnCount());
        for (size_t i = 0; i < child_executor_->GetOutputSchema().GetColumnCount(); i++) {
          values.push_back(left_tuple_.GetValue(&(child_executor_->GetOutputSchema()), i));
        }
        for (size_t i = 0; i < plan_->InnerTableSchema().GetColumnCount(); i++) {
          values.push_back(right_tuple_.GetValue(&(plan_->InnerTableSchema()), i));
        }

        *tuple = Tuple(values, &GetOutputSchema());
        return true;
      }
      continue;
    }

    Tuple temp_tuple = left_tuple_;

    left_status_ = child_executor_->Next(&left_tuple_, &left_rid_);
    index_iter_ = tree_->GetBeginIterator();

    if (!left_find_ && plan_->GetJoinType() == JoinType::LEFT) {
      std::vector<Value> values{};
      values.reserve(GetOutputSchema().GetColumnCount());
      for (size_t i = 0; i < child_executor_->GetOutputSchema().GetColumnCount(); i++) {
        values.push_back(temp_tuple.GetValue(&(child_executor_->GetOutputSchema()), i));
      }
      for (size_t i = 0; i < plan_->InnerTableSchema().GetColumnCount(); i++) {
        values.push_back(ValueFactory::GetNullValueByType(plan_->InnerTableSchema().GetColumn(i).GetType()));
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
