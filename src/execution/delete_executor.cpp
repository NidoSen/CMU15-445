//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// delete_executor.cpp
//
// Identification: src/execution/delete_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>

#include "execution/executors/delete_executor.h"

namespace bustub {

DeleteExecutor::DeleteExecutor(ExecutorContext *exec_ctx, const DeletePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      child_executor_(std::move(child_executor)),
      table_info_(exec_ctx_->GetCatalog()->GetTable(plan_->TableOid())) {}

void DeleteExecutor::Init() {
  child_executor_->Init();
  table_indexes_ = exec_ctx_->GetCatalog()->GetTableIndexes(table_info_->name_);
  is_end_ = false;
}

auto DeleteExecutor::Next([[maybe_unused]] Tuple *tuple, RID *rid) -> bool {
  if (is_end_) {
    return false;
  }

  int deleted_count = 0;
  while (child_executor_->Next(tuple, rid)) {
    if (table_info_->table_->MarkDelete(*rid, exec_ctx_->GetTransaction())) {
      deleted_count++;
      for (auto table_index : table_indexes_) {
        const Schema schema = plan_->GetChildPlan()->OutputSchema();
        const Schema key_schema = *(table_index->index_->GetKeySchema());
        const std::vector<uint32_t> key_attrs = table_index->index_->GetKeyAttrs();
        table_index->index_->DeleteEntry(tuple->KeyFromTuple(schema, key_schema, key_attrs), *rid,
                                         exec_ctx_->GetTransaction());
      }
    }
  }

  std::vector<Value> values{};
  values.reserve(GetOutputSchema().GetColumnCount());
  values.emplace_back(TypeId::INTEGER, deleted_count);
  *tuple = Tuple(values, &GetOutputSchema());
  is_end_ = true;
  return true;
}

}  // namespace bustub
