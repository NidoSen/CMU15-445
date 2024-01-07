//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/include/index/index_iterator.h
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
/**
 * index_iterator.h
 * For range scan of b+ tree
 */
#pragma once
#include "storage/page/b_plus_tree_leaf_page.h"

namespace bustub {

#define INDEXITERATOR_TYPE IndexIterator<KeyType, ValueType, KeyComparator>

INDEX_TEMPLATE_ARGUMENTS
class IndexIterator {
 public:
  // you may define your own constructor based on your member variables
  IndexIterator(BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                page_id_t page_id = INVALID_PAGE_ID, int index = 0);
  // IndexIterator();
  ~IndexIterator();  // NOLINT

  auto IsEnd() -> bool;

  auto operator*() -> const MappingType &;

  auto operator++() -> IndexIterator &;

  auto operator==(const IndexIterator &itr) const -> bool {
    return page_id_ == itr.GetPageId() && index_ == itr.GetIndex();
  }

  auto operator!=(const IndexIterator &itr) const -> bool {
    return page_id_ != itr.GetPageId() || index_ != itr.GetIndex();
  }

  auto GetPageId() const -> page_id_t { return page_id_; }

  auto GetIndex() const -> int { return index_; }

 private:
  // add your own private member variables here
  BufferPoolManager *buffer_pool_manager_;
  KeyComparator comparator_;
  page_id_t page_id_;
  int index_;
  MappingType temp_;
};

}  // namespace bustub
