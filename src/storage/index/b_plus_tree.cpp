#include <string>

#include "common/exception.h"
#include "common/logger.h"
#include "common/rid.h"
#include "storage/index/b_plus_tree.h"
#include "storage/page/header_page.h"

namespace bustub {
INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                          int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)),
      root_page_id_(INVALID_PAGE_ID),
      buffer_pool_manager_(buffer_pool_manager),
      comparator_(comparator),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size) {}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool { return root_page_id_ == INVALID_PAGE_ID; }
/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *transaction) -> bool {
  if (IsEmpty()) {
    return false;
  }

  auto cur_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(root_page_id_)->GetData());
  int index;
  while (!cur_page->IsLeafPage()) {
    auto cur_internal_page = reinterpret_cast<InternalPage *>(cur_page);
    cur_internal_page->FindKeyIndex(key, comparator_, &index);
    cur_page = reinterpret_cast<BPlusTreePage *>(
        buffer_pool_manager_->FetchPage(cur_internal_page->ValueAt(index))->GetData());
    buffer_pool_manager_->UnpinPage(cur_internal_page->GetPageId(), false);
  }

  auto cur_leaf_page = reinterpret_cast<LeafPage *>(cur_page);
  if (cur_leaf_page->FindKeyIndex(key, comparator_, &index)) {
    result->emplace_back(cur_leaf_page->ValueAt(index));
    buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), false);
    return true;
  }
  buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), false);

  return false;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value, Transaction *transaction) -> bool {
  if (IsEmpty()) {
    auto root_as_leaf = reinterpret_cast<LeafPage *>(buffer_pool_manager_->NewPage(&root_page_id_)->GetData());
    root_as_leaf->Init(root_page_id_, INVALID_PAGE_ID, leaf_max_size_);
    root_as_leaf->SetKeyValueAt(key, value, 0);
    UpdateRootPageId(0);
    buffer_pool_manager_->UnpinPage(root_page_id_, true);
    return true;
  }

  auto cur_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(root_page_id_)->GetData());
  int index;
  while (!cur_page->IsLeafPage()) {
    auto cur_internal_page = reinterpret_cast<InternalPage *>(cur_page);
    if (cur_internal_page->FindKeyIndex(key, comparator_, &index)) {  // 如果内部节点存在key，插入失败
      buffer_pool_manager_->UnpinPage(cur_internal_page->GetPageId(), false);
      return false;
    }
    // LOG_INFO("$ %d $ %d $ %d $", cur_internal_page->GetPageId(), index, cur_internal_page->ValueAt(index));
    cur_page = reinterpret_cast<BPlusTreePage *>(
        buffer_pool_manager_->FetchPage(cur_internal_page->ValueAt(index))->GetData());
    buffer_pool_manager_->UnpinPage(cur_internal_page->GetPageId(), false);
  }

  auto cur_leaf_page = reinterpret_cast<LeafPage *>(cur_page);
  if (cur_leaf_page->FindKeyIndex(key, comparator_, &index)) {  // 如果叶子节点存在key，插入失败
    buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), false);
    return false;
  }
  // LOG_INFO("$ %d $ %d $", cur_leaf_page->GetPageId(), index);
  cur_leaf_page->SetKeyValueAt(key, value, index);
  if (cur_leaf_page->GetSize() < leaf_max_size_) {  // 插入节点后叶子结点的键值对个数没到leaf_max_size_，可以结束了
    buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), true);
    return true;
  }

  // split
  page_id_t new_leaf_page_id;
  auto new_leaf_page = reinterpret_cast<LeafPage *>(buffer_pool_manager_->NewPage(&new_leaf_page_id)->GetData());
  new_leaf_page->Init(new_leaf_page_id, cur_leaf_page->GetParentPageId(), leaf_max_size_);
  new_leaf_page->SetNextPageId(cur_leaf_page->GetNextPageId());
  cur_leaf_page->SetNextPageId(new_leaf_page_id);
  InternalPage *parent_internal_page = nullptr;
  if (cur_leaf_page->IsRootPage()) {
    parent_internal_page = reinterpret_cast<InternalPage *>(buffer_pool_manager_->NewPage(&root_page_id_)->GetData());
    parent_internal_page->Init(root_page_id_, INVALID_PAGE_ID, internal_max_size_);
    parent_internal_page->SetMinValue(cur_leaf_page->GetPageId());
    parent_internal_page->SetSize(1);
    cur_leaf_page->SetParentPageId(root_page_id_);
    new_leaf_page->SetParentPageId(root_page_id_);
    UpdateRootPageId(1);
  } else {
    parent_internal_page =
        reinterpret_cast<InternalPage *>(buffer_pool_manager_->FetchPage(cur_leaf_page->GetParentPageId())->GetData());
  }
  // LOG_INFO("@ %d @ %d @ %d @", cur_leaf_page->GetPageId(), new_leaf_page->GetPageId(),
  // parent_internal_page->GetPageId());

  for (int i = leaf_max_size_ / 2; i < leaf_max_size_; i++) {
    new_leaf_page->SetKeyValueAt(cur_leaf_page->KeyAt(i), cur_leaf_page->ValueAt(i), i - leaf_max_size_ / 2);
  }
  cur_leaf_page->SetSize(leaf_max_size_ / 2);
  new_leaf_page->SetSize(leaf_max_size_ - leaf_max_size_ / 2);

  KeyType insert_key = new_leaf_page->KeyAt(0);
  page_id_t insert_value = new_leaf_page_id;

  buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), true);
  buffer_pool_manager_->UnpinPage(new_leaf_page->GetPageId(), true);

  // 多重 split
  while (parent_internal_page->GetSize() == internal_max_size_) {
    auto cur_internal_page = parent_internal_page;
    page_id_t new_internal_page_id;
    auto new_internal_page =
        reinterpret_cast<InternalPage *>(buffer_pool_manager_->NewPage(&new_internal_page_id)->GetData());
    new_internal_page->Init(new_internal_page_id, cur_internal_page->GetParentPageId(), internal_max_size_);
    if (cur_internal_page->IsRootPage()) {
      parent_internal_page = reinterpret_cast<InternalPage *>(buffer_pool_manager_->NewPage(&root_page_id_)->GetData());
      parent_internal_page->Init(root_page_id_, INVALID_PAGE_ID, internal_max_size_);
      // LOG_INFO("# %d %d", root_page_id_, cur_internal_page->GetPageId());
      parent_internal_page->SetMinValue(cur_internal_page->GetPageId());
      parent_internal_page->SetSize(1);
      cur_internal_page->SetParentPageId(root_page_id_);
      new_internal_page->SetParentPageId(root_page_id_);
      UpdateRootPageId(1);
    } else {
      parent_internal_page = reinterpret_cast<InternalPage *>(
          buffer_pool_manager_->FetchPage(cur_internal_page->GetParentPageId())->GetData());
    }

    // LOG_INFO("@ %d @ %d @ %d @", cur_internal_page->GetPageId(), new_internal_page->GetPageId(),
    // parent_internal_page->GetPageId());

    if (comparator_(cur_internal_page->KeyAt(internal_max_size_ - 1), insert_key) < 0) {
      new_internal_page->SetKeyValueAt(insert_key, insert_value, internal_max_size_ / 2);
    } else {
      new_internal_page->SetKeyValueAt(cur_internal_page->KeyAt(internal_max_size_ - 1),
                                       cur_internal_page->ValueAt(internal_max_size_ - 1), internal_max_size_ / 2);
      int index;
      cur_internal_page->FindKeyIndex(insert_key, comparator_, &index);
      cur_internal_page->SetKeyValueAt(insert_key, insert_value, index + 1);
    }
    new_internal_page->SetMinValue(cur_internal_page->ValueAt((internal_max_size_ + 1) / 2));
    new_internal_page->SetSize(1);
    for (int i = 1; i < internal_max_size_ / 2; i++) {
      new_internal_page->SetKeyValueAt(cur_internal_page->KeyAt((internal_max_size_ + 1) / 2 + i),
                                       cur_internal_page->ValueAt((internal_max_size_ + 1) / 2 + i), i);
    }
    cur_internal_page->SetSize((internal_max_size_ + 1) / 2);
    new_internal_page->SetSize(internal_max_size_ / 2 + 1);

    auto left_child_page =
        reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(new_internal_page->ValueAt(0))->GetData());
    auto right_child_page =
        reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(new_internal_page->ValueAt(1))->GetData());
    left_child_page->SetParentPageId(new_internal_page_id);
    right_child_page->SetParentPageId(new_internal_page_id);
    buffer_pool_manager_->UnpinPage(left_child_page->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(right_child_page->GetPageId(), true);

    insert_key = cur_internal_page->KeyAt((internal_max_size_ + 1) / 2);
    insert_value = new_internal_page_id;

    buffer_pool_manager_->UnpinPage(cur_internal_page->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(new_internal_page->GetPageId(), true);
  }

  int insert_index;
  parent_internal_page->FindKeyIndex(insert_key, comparator_, &insert_index);
  parent_internal_page->SetKeyValueAt(insert_key, insert_value, insert_index + 1);
  // LOG_INFO("PageId: %d, Size: %d", parent_internal_page->GetPageId(), parent_internal_page->GetSize());
  buffer_pool_manager_->UnpinPage(parent_internal_page->GetPageId(), true);

  return true;
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immdiately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *transaction) {
  if (IsEmpty()) {
    return;
  }

  auto cur_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(root_page_id_)->GetData());
  int index;
  while (!cur_page->IsLeafPage()) {
    auto cur_internal_page = reinterpret_cast<InternalPage *>(cur_page);
    cur_internal_page->FindKeyIndex(key, comparator_, &index);
    cur_page = reinterpret_cast<BPlusTreePage *>(
        buffer_pool_manager_->FetchPage(cur_internal_page->ValueAt(index))->GetData());
    buffer_pool_manager_->UnpinPage(cur_internal_page->GetPageId(), false);
  }

  auto cur_leaf_page = reinterpret_cast<LeafPage *>(cur_page);
  if (!cur_leaf_page->FindKeyIndex(key, comparator_, &index)) {  // 如果叶子节点不存在key，无需删除，直接返回
    buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), false);
    return;
  }

  // LOG_INFO("$ %d $ %d $ %d $", cur_leaf_page->GetPageId(), index, cur_leaf_page->GetSize());

  cur_leaf_page->RemoveKeyValueAt(key, index);
  // LOG_INFO("$ %d $ %d $ %d $", cur_leaf_page->GetPageId(), index, cur_leaf_page->GetSize());
  // 删除后叶子结点的键值对个数大于等于最小允许结点数，可以结束了
  if (cur_leaf_page->IsRootPage() || cur_leaf_page->GetSize() >= leaf_max_size_ / 2) {
    buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), true);
  }
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the leftmost leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE {
  if (IsEmpty()) {
    return INDEXITERATOR_TYPE(buffer_pool_manager_, comparator_);
  }

  auto cur_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(root_page_id_)->GetData());
  while (!cur_page->IsLeafPage()) {
    auto cur_internal_page = reinterpret_cast<InternalPage *>(cur_page);
    cur_page =
        reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(cur_internal_page->ValueAt(0))->GetData());
    buffer_pool_manager_->UnpinPage(cur_internal_page->GetPageId(), false);
  }

  page_id_t page_id = cur_page->GetPageId();
  int index = 0;
  buffer_pool_manager_->UnpinPage(cur_page->GetPageId(), false);

  return INDEXITERATOR_TYPE(buffer_pool_manager_, comparator_, page_id, index);
}

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE {
  if (IsEmpty()) {
    return INDEXITERATOR_TYPE(buffer_pool_manager_, comparator_);
  }

  auto cur_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(root_page_id_)->GetData());
  int index;
  while (!cur_page->IsLeafPage()) {
    auto cur_internal_page = reinterpret_cast<InternalPage *>(cur_page);
    cur_internal_page->FindKeyIndex(key, comparator_, &index);
    cur_page = reinterpret_cast<BPlusTreePage *>(
        buffer_pool_manager_->FetchPage(cur_internal_page->ValueAt(index))->GetData());
    buffer_pool_manager_->UnpinPage(cur_internal_page->GetPageId(), false);
  }

  auto cur_leaf_page = reinterpret_cast<LeafPage *>(cur_page);
  if (cur_leaf_page->FindKeyIndex(key, comparator_, &index)) {
    page_id_t page_id = cur_page->GetPageId();
    buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), false);
    return INDEXITERATOR_TYPE(buffer_pool_manager_, comparator_, page_id, index);
  }
  buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), false);

  return INDEXITERATOR_TYPE(buffer_pool_manager_, comparator_);
}

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(buffer_pool_manager_, comparator_); }

/**
 * @return Page id of the root of this tree
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t { return root_page_id_; }

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Update/Insert root page id in header page(where page_id = 0, header_page is
 * defined under include/page/header_page.h)
 * Call this method everytime root page id is changed.
 * @parameter: insert_record      defualt value is false. When set to true,
 * insert a record <index_name, root_page_id> into header page instead of
 * updating it.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::UpdateRootPageId(int insert_record) {
  auto *header_page = static_cast<HeaderPage *>(buffer_pool_manager_->FetchPage(HEADER_PAGE_ID));
  if (insert_record != 0) {
    // create a new record<index_name + root_page_id> in header_page
    header_page->InsertRecord(index_name_, root_page_id_);
  } else {
    // update root_page_id in header_page
    header_page->UpdateRecord(index_name_, root_page_id_);
  }
  buffer_pool_manager_->UnpinPage(HEADER_PAGE_ID, true);
}

/*
 * This method is used for test only
 * Read data from file and insert one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;

    KeyType index_key;
    index_key.SetFromInteger(key);
    RID rid(key);
    Insert(index_key, rid, transaction);
  }
}
/*
 * This method is used for test only
 * Read data from file and remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RemoveFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;
    KeyType index_key;
    index_key.SetFromInteger(key);
    Remove(index_key, transaction);
  }
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Draw(BufferPoolManager *bpm, const std::string &outf) {
  if (IsEmpty()) {
    LOG_WARN("Draw an empty tree");
    return;
  }
  std::ofstream out(outf);
  out << "digraph G {" << std::endl;
  ToGraph(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(root_page_id_)->GetData()), bpm, out);
  out << "}" << std::endl;
  out.flush();
  out.close();
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Print(BufferPoolManager *bpm) {
  if (IsEmpty()) {
    LOG_WARN("Print an empty tree");
    return;
  }
  ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(root_page_id_)->GetData()), bpm);
}

/**
 * This method is used for debug only, You don't need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 * @param out
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out) const {
  std::string leaf_prefix("LEAF_");
  std::string internal_prefix("INT_");
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    // Print node name
    out << leaf_prefix << leaf->GetPageId();
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << leaf->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << ",size=" << leaf->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      out << "<TD>" << leaf->KeyAt(i) << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << leaf->GetPageId() << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << leaf->GetPageId() << " " << leaf_prefix << leaf->GetNextPageId() << "};\n";
    }

    // Print parent links if there is a parent
    if (leaf->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << leaf->GetParentPageId() << ":p" << leaf->GetPageId() << " -> " << leaf_prefix
          << leaf->GetPageId() << ";\n";
    }
  } else {
    auto *inner = reinterpret_cast<InternalPage *>(page);
    // Print node name
    out << internal_prefix << inner->GetPageId();
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << inner->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
        << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << ",size=" << inner->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < inner->GetSize(); i++) {
      out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
      if (i > 0) {
        out << inner->KeyAt(i);
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Parent link
    if (inner->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << inner->GetParentPageId() << ":p" << inner->GetPageId() << " -> " << internal_prefix
          << inner->GetPageId() << ";\n";
    }
    // Print leaves
    for (int i = 0; i < inner->GetSize(); i++) {
      auto child_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i))->GetData());
      ToGraph(child_page, bpm, out);
      if (i > 0) {
        auto sibling_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i - 1))->GetData());
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_page->GetPageId() << " " << internal_prefix
              << child_page->GetPageId() << "};\n";
        }
        bpm->UnpinPage(sibling_page->GetPageId(), false);
      }
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

/**
 * This function is for debug only, you don't need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToString(BPlusTreePage *page, BufferPoolManager *bpm) const {
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    std::cout << "Leaf Page: " << leaf->GetPageId() << " parent: " << leaf->GetParentPageId()
              << " next: " << leaf->GetNextPageId() << std::endl;
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
  } else {
    auto *internal = reinterpret_cast<InternalPage *>(page);
    std::cout << "Internal Page: " << internal->GetPageId() << " parent: " << internal->GetParentPageId() << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i))->GetData()), bpm);
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
