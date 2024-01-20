//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// insert_executor.cpp
//
// Identification: src/execution/insert_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>

#include "execution/executors/insert_executor.h"

namespace bustub {

InsertExecutor::InsertExecutor(ExecutorContext *exec_ctx, const InsertPlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      child_executor_(std::move(child_executor)),
      table_info_(exec_ctx_->GetCatalog()->GetTable(plan_->TableOid())) {}

void InsertExecutor::Init() {
  child_executor_->Init();
  table_indexes_ = exec_ctx_->GetCatalog()->GetTableIndexes(table_info_->name_);
  is_end_ = false;
}

auto InsertExecutor::Next([[maybe_unused]] Tuple *tuple, RID *rid) -> bool {
  if (is_end_) {
    return false;
  }

  int inserted_count = 0;
  while (child_executor_->Next(tuple, rid)) {
    if (table_info_->table_->InsertTuple(*tuple, rid, exec_ctx_->GetTransaction())) {
      inserted_count++;
      for (auto table_index : table_indexes_) {
        const Schema schema = plan_->GetChildPlan()->OutputSchema();
        const Schema key_schema = *(table_index->index_->GetKeySchema());
        const std::vector<uint32_t> key_attrs = table_index->index_->GetKeyAttrs();
        table_index->index_->InsertEntry(tuple->KeyFromTuple(schema, key_schema, key_attrs), *rid,
                                         exec_ctx_->GetTransaction());
      }
    }
  }

  std::vector<Value> values{};
  values.reserve(GetOutputSchema().GetColumnCount());
  values.emplace_back(TypeId::INTEGER, inserted_count);
  *tuple = Tuple(values, &GetOutputSchema());
  is_end_ = true;
  return true;
}

}  // namespace bustub
