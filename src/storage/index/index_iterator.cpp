/**
 * index_iterator.cpp
 */
#include <cassert>

#include "storage/index/index_iterator.h"

namespace bustub {

/*
 * NOTE: you can change the destructor/constructor method here
 * set your own input parameters
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator(BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                                  page_id_t page_id, int index)
    : buffer_pool_manager_(buffer_pool_manager), comparator_(comparator) {
  page_id_ = page_id;
  index_ = index;
}

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator() = default;  // NOLINT

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::IsEnd() -> bool { return page_id_ == INVALID_PAGE_ID; }

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator*() -> const MappingType & {
  auto leaf_page = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(
      buffer_pool_manager_->FetchPage(page_id_)->GetData());
  temp_.first = leaf_page->KeyAt(index_);
  temp_.second = leaf_page->ValueAt(index_);
  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
  return temp_;
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator++() -> INDEXITERATOR_TYPE & {
  auto leaf_page = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(
      buffer_pool_manager_->FetchPage(page_id_)->GetData());
  index_++;
  if (index_ == leaf_page->GetSize()) {
    page_id_ = leaf_page->GetNextPageId();
    index_ = 0;
  }
  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
  return *this;
}

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;

template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;

template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;

template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;

template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
