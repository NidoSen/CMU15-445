//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/page/b_plus_tree_internal_page.cpp
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <sstream>

#include "common/exception.h"
#include "storage/page/b_plus_tree_internal_page.h"

namespace bustub {
/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/
/*
 * Init method after creating a new internal page
 * Including set page type, set current size, set page id, set parent id and set
 * max page size
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Init(page_id_t page_id, page_id_t parent_id, int max_size) {
  SetPageType(bustub::IndexPageType::INTERNAL_PAGE);
  SetSize(0);
  SetMaxSize(max_size);
  SetParentPageId(parent_id);
  SetPageId(page_id);
}
/*
 * Helper method to get/set the key associated with input "index"(a.k.a
 * array offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyAt(int index) const -> KeyType { return array_[index].first; }

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetKeyAt(int index, const KeyType &key) { array_[index].first = key; }

/*
 * Helper method to get the value associated with input "index"(a.k.a array
 * offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueAt(int index) const -> ValueType { return array_[index].second; }

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetValueAt(int index, const ValueType &value) { array_[index].second = value; }

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::FindKeyIndex(int *index, const KeyType &key, const KeyComparator &comparator) const
    -> bool {
  int left_index = 0;
  int right_index = GetSize() - 1;
  while (left_index < right_index) {
    int middle_index = (left_index + right_index + 1) >> 1;
    if (comparator(array_[middle_index].first, key) <= 0) {
      left_index = middle_index;
    } else {
      right_index = middle_index - 1;
    }
  }

  *index = left_index;
  return left_index > 0 && comparator(array_[left_index].first, key) == 0;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::InsertKeyValueAt(int index, const KeyType &key, const ValueType &value) {
  for (int i = GetSize(); i > index; i--) {
    array_[i] = array_[i - 1];
  }
  array_[index].first = key;
  array_[index].second = value;
  IncreaseSize(1);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::RemoveKeyValueAt(int index) {
  for (int i = index; i < GetSize() - 1; i++) {
    array_[i] = array_[i + 1];
  }
  IncreaseSize(-1);
}

// valuetype for internalNode should be page id_t
template class BPlusTreeInternalPage<GenericKey<4>, page_id_t, GenericComparator<4>>;
template class BPlusTreeInternalPage<GenericKey<8>, page_id_t, GenericComparator<8>>;
template class BPlusTreeInternalPage<GenericKey<16>, page_id_t, GenericComparator<16>>;
template class BPlusTreeInternalPage<GenericKey<32>, page_id_t, GenericComparator<32>>;
template class BPlusTreeInternalPage<GenericKey<64>, page_id_t, GenericComparator<64>>;
}  // namespace bustub
