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
  // 树空，查询失败
  if (IsEmpty()) {
    return false;
  }

  // 找到根结点对应的页，上锁，并加入到当前线程transaction的上锁页队列page_set_
  auto original_page = buffer_pool_manager_->FetchPage(root_page_id_);
  if (transaction != nullptr) {
    transaction->AddIntoPageSet(original_page);
    original_page->RLatch();
  }
  auto cur_page = reinterpret_cast<BPlusTreePage *>(original_page->GetData());
  int index;
  // 逐层下降，找到key（可能）所在的叶子结点
  while (!cur_page->IsLeafPage()) {
    // 找到下一层的页，并上锁
    auto cur_internal_page = reinterpret_cast<InternalPage *>(cur_page);
    cur_internal_page->FindKeyIndex(&index, key, comparator_);
    // LOG_INFO("$ %d $ %d # %d @ %d @***", cur_page->GetPageId(), cur_page->GetSize(), cur_page->GetParentPageId(),
    // index + 1);
    original_page = buffer_pool_manager_->FetchPage(cur_internal_page->ValueAt(index));
    if (transaction != nullptr) {
      original_page->RLatch();
      transaction->AddIntoPageSet(original_page);
    }
    cur_page = reinterpret_cast<BPlusTreePage *>(original_page->GetData());
    // 解锁当前层的页并UnpinPage，因为original_page已经走到了下一层，所以需要借助当前线程transaction的上锁页队列page_set_
    if (transaction != nullptr) {
      original_page = transaction->GetPageSet()->front();
      transaction->GetPageSet()->pop_front();
      original_page->RUnlatch();
    }
    buffer_pool_manager_->UnpinPage(cur_internal_page->GetPageId(), false);
  }

  auto cur_leaf_page = reinterpret_cast<LeafPage *>(cur_page);
  if (cur_leaf_page->FindKeyIndex(&index, key, comparator_)) {
    // LOG_INFO("$ %d $ %d # %d @ %d @+++", cur_page->GetPageId(), cur_page->GetSize(), cur_page->GetParentPageId(),
    // index + 1);
    result->emplace_back(cur_leaf_page->ValueAt(index));
    if (transaction != nullptr) {
      original_page = transaction->GetPageSet()->front();
      transaction->GetPageSet()->pop_front();
      original_page->RUnlatch();
    }
    buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), false);
    return true;
  }
  // LOG_INFO("$ %d $ %d # %d @ %d @---", cur_page->GetPageId(), cur_page->GetSize(), cur_page->GetParentPageId(), index
  // + 1);
  if (transaction != nullptr) {
    original_page = transaction->GetPageSet()->front();
    transaction->GetPageSet()->pop_front();
    original_page->RUnlatch();
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
  // 树空，新建结点
  latch_.WLock();
  bool get_root = true;
  // LOG_INFO("%d", root_page_id_);
  if (IsEmpty()) {
    auto original_page = buffer_pool_manager_->NewPage(&root_page_id_);
    original_page->WLatch();
    auto root_as_leaf = reinterpret_cast<LeafPage *>(original_page->GetData());
    root_as_leaf->Init(root_page_id_, INVALID_PAGE_ID, leaf_max_size_);
    root_as_leaf->InsertKeyValueAt(0, key, value);
    UpdateRootPageId(0);
    original_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(root_page_id_, true);
    latch_.WUnlock();
    return true;
  }

  // LOG_INFO("%ld", transaction->GetPageSet()->size());
  // 找到根结点对应的页，上W锁，并加入到当前线程transaction的上锁页双端队列page_set_
  auto original_page = buffer_pool_manager_->FetchPage(root_page_id_);
  original_page->WLatch();
  transaction->AddIntoPageSet(original_page);

  // 逐层下降，找到插入key的正确叶子结点
  auto cur_page = reinterpret_cast<BPlusTreePage *>(original_page->GetData());
  int index;
  while (!cur_page->IsLeafPage()) {
    auto cur_internal_page = reinterpret_cast<InternalPage *>(cur_page);
    // 如果当前结点发生插入键值对也不会导致分裂，说明当前结点是安全的，则可以将其所有祖先节点解W锁并UnpinPage
    if (cur_internal_page->GetSize() < internal_max_size_) {
      while (transaction->GetPageSet()->size() > 1) {
        original_page = transaction->GetPageSet()->front();
        transaction->GetPageSet()->pop_front();
        original_page->WUnlatch();
        buffer_pool_manager_->UnpinPage(original_page->GetPageId(), false);
      }
      if (!cur_page->IsRootPage() && get_root) {
        get_root = false;
        latch_.WUnlock();
      }
    }
    // 找到下一层的页，并上W锁
    cur_internal_page->FindKeyIndex(&index, key, comparator_);
    original_page = buffer_pool_manager_->FetchPage(cur_internal_page->ValueAt(index));
    original_page->WLatch();
    transaction->AddIntoPageSet(original_page);
    cur_page = reinterpret_cast<BPlusTreePage *>(original_page->GetData());
  }

  // 找到叶子结点后，如果叶子节点存在key，插入失败
  auto cur_leaf_page = reinterpret_cast<LeafPage *>(cur_page);
  if (cur_leaf_page->FindKeyIndex(&index, key, comparator_)) {
    // LOG_INFO("$ %d $ %d $ %d $", cur_leaf_page->GetPageId(), index, cur_leaf_page->GetSize());
    while (!transaction->GetPageSet()->empty()) {
      original_page = transaction->GetPageSet()->front();
      transaction->GetPageSet()->pop_front();
      original_page->WUnlatch();
      buffer_pool_manager_->UnpinPage(original_page->GetPageId(), false);
    }
    if (get_root) {
      get_root = false;
      latch_.WUnlock();
    }
    return false;
  }

  // 找到叶子结点后，如果叶子节点不存在key，可以插入
  original_page = transaction->GetPageSet()->back();
  // LOG_INFO("$ %d $ %d $", cur_leaf_page->GetPageId(), index);
  cur_leaf_page->InsertKeyValueAt(index, key, value);

  // 如果插入键值对后叶子结点的键值对个数没到leaf_max_size_，可以结束了
  if (cur_leaf_page->GetSize() < leaf_max_size_) {
    while (transaction->GetPageSet()->size() > 1) {
      original_page = transaction->GetPageSet()->front();
      transaction->GetPageSet()->pop_front();
      original_page->WUnlatch();
      buffer_pool_manager_->UnpinPage(original_page->GetPageId(), false);
    }
    original_page = transaction->GetPageSet()->front();
    transaction->GetPageSet()->pop_front();
    original_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), true);
    if (get_root) {
      get_root = false;
      latch_.WUnlock();
    }
    return true;
  }

  // 如果插入键值对后叶子结点的键值对个数到leaf_max_size_，需要分裂，先处理叶结点的分裂
  auto cur_original_page = transaction->GetPageSet()->back();
  transaction->GetPageSet()->pop_back();
  page_id_t new_leaf_page_id;  // 新建结点，上W锁
  auto new_original_page = buffer_pool_manager_->NewPage(&new_leaf_page_id);
  new_original_page->WLatch();
  Page *parent_original_page = nullptr;  // 找到父结点，如果父节点不存在，则新建并上W锁，更新树的根结点
  InternalPage *parent_internal_page = nullptr;
  if (cur_leaf_page->IsRootPage()) {
    parent_original_page = buffer_pool_manager_->NewPage(&root_page_id_);
    parent_original_page->WLatch();
    parent_internal_page = reinterpret_cast<InternalPage *>(parent_original_page->GetData());
    parent_internal_page->Init(root_page_id_, INVALID_PAGE_ID, internal_max_size_);
    parent_internal_page->SetValueAt(0, cur_leaf_page->GetPageId());
    parent_internal_page->SetSize(1);
    UpdateRootPageId(1);
  } else {
    parent_original_page = transaction->GetPageSet()->back();
    transaction->GetPageSet()->pop_back();
    parent_internal_page = reinterpret_cast<InternalPage *>(parent_original_page->GetData());
  }

  auto new_leaf_page = reinterpret_cast<LeafPage *>(new_original_page->GetData());
  new_leaf_page->Init(new_leaf_page_id, cur_leaf_page->GetParentPageId(), leaf_max_size_);
  new_leaf_page->SetNextPageId(cur_leaf_page->GetNextPageId());
  cur_leaf_page->SetNextPageId(new_leaf_page_id);
  cur_leaf_page->SetParentPageId(parent_internal_page->GetPageId());
  new_leaf_page->SetParentPageId(parent_internal_page->GetPageId());

  // LOG_INFO("@ %d @ %d @ %d @", cur_leaf_page->GetPageId(), new_leaf_page->GetPageId(),
  // parent_internal_page->GetPageId());

  for (int i = leaf_max_size_ / 2; i < leaf_max_size_; i++) {
    new_leaf_page->InsertKeyValueAt(i - leaf_max_size_ / 2, cur_leaf_page->KeyAt(i), cur_leaf_page->ValueAt(i));
  }
  cur_leaf_page->SetSize(leaf_max_size_ / 2);
  new_leaf_page->SetSize(leaf_max_size_ - leaf_max_size_ / 2);

  KeyType insert_key = new_leaf_page->KeyAt(0);
  page_id_t insert_value = new_leaf_page_id;

  cur_original_page->WUnlatch();
  new_original_page->WUnlatch();
  buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), true);
  buffer_pool_manager_->UnpinPage(new_leaf_page->GetPageId(), true);

  // 处理内部节点的（多重）分裂
  while (parent_internal_page->GetSize() == internal_max_size_) {
    cur_original_page = parent_original_page;
    auto cur_internal_page = parent_internal_page;
    page_id_t new_internal_page_id;
    new_original_page = buffer_pool_manager_->NewPage(&new_internal_page_id);
    new_original_page->WLatch();
    if (cur_internal_page->IsRootPage()) {
      parent_original_page = buffer_pool_manager_->NewPage(&root_page_id_);
      parent_original_page->WLatch();
      parent_internal_page = reinterpret_cast<InternalPage *>(parent_original_page->GetData());
      parent_internal_page->Init(root_page_id_, INVALID_PAGE_ID, internal_max_size_);
      // LOG_INFO("# %d %d", root_page_id_, cur_internal_page->GetPageId());
      parent_internal_page->SetValueAt(0, cur_internal_page->GetPageId());
      parent_internal_page->SetSize(1);
      UpdateRootPageId(1);
    } else {
      parent_original_page = transaction->GetPageSet()->back();
      transaction->GetPageSet()->pop_back();
      parent_internal_page = reinterpret_cast<InternalPage *>(parent_original_page->GetData());
    }
    auto new_internal_page = reinterpret_cast<InternalPage *>(new_original_page->GetData());
    new_internal_page->Init(new_internal_page_id, cur_internal_page->GetParentPageId(), internal_max_size_);
    cur_internal_page->SetParentPageId(parent_internal_page->GetPageId());
    new_internal_page->SetParentPageId(parent_internal_page->GetPageId());

    // LOG_INFO("@ %d @ %d @ %d @", cur_internal_page->GetPageId(), new_internal_page->GetPageId(),
    // parent_internal_page->GetPageId());

    if (comparator_(cur_internal_page->KeyAt(internal_max_size_ - 1), insert_key) < 0) {
      new_internal_page->InsertKeyValueAt(internal_max_size_ / 2, insert_key, insert_value);
    } else {
      new_internal_page->InsertKeyValueAt(internal_max_size_ / 2, cur_internal_page->KeyAt(internal_max_size_ - 1),
                                          cur_internal_page->ValueAt(internal_max_size_ - 1));
      int index;
      cur_internal_page->FindKeyIndex(&index, insert_key, comparator_);
      cur_internal_page->InsertKeyValueAt(index + 1, insert_key, insert_value);
    }
    new_internal_page->SetValueAt(0, cur_internal_page->ValueAt((internal_max_size_ + 1) / 2));
    new_internal_page->SetSize(1);
    for (int i = 1; i < internal_max_size_ / 2; i++) {
      new_internal_page->InsertKeyValueAt(i, cur_internal_page->KeyAt((internal_max_size_ + 1) / 2 + i),
                                          cur_internal_page->ValueAt((internal_max_size_ + 1) / 2 + i));
    }
    cur_internal_page->SetSize((internal_max_size_ + 1) / 2);
    new_internal_page->SetSize(internal_max_size_ / 2 + 1);

    for (int i = 0; i < new_internal_page->GetSize(); i++) {
      auto child_original_page = buffer_pool_manager_->FetchPage(new_internal_page->ValueAt(i));
      child_original_page->WLatch();
      auto child_page = reinterpret_cast<BPlusTreePage *>(child_original_page->GetData());
      child_page->SetParentPageId(new_internal_page_id);
      child_original_page->WUnlatch();
      buffer_pool_manager_->UnpinPage(child_page->GetPageId(), true);
    }

    insert_key = cur_internal_page->KeyAt((internal_max_size_ + 1) / 2);
    insert_value = new_internal_page_id;

    cur_original_page->WUnlatch();
    new_original_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(cur_internal_page->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(new_internal_page->GetPageId(), true);
  }

  int insert_index;
  parent_internal_page->FindKeyIndex(&insert_index, insert_key, comparator_);
  parent_internal_page->InsertKeyValueAt(insert_index + 1, insert_key, insert_value);
  // LOG_INFO("PageId: %d, Size: %d", parent_internal_page->GetPageId(), parent_internal_page->GetSize());
  parent_original_page->WUnlatch();
  buffer_pool_manager_->UnpinPage(parent_internal_page->GetPageId(), true);
  if (get_root) {
    get_root = false;
    latch_.WUnlock();
  }
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
  // 树空，无需删除，直接返回
  latch_.WLock();
  bool get_root = true;
  if (IsEmpty()) {
    latch_.WUnlock();
    return;
  }

  // 找到根结点对应的页，上W锁，并加入到当前线程transaction的上锁页双端队列page_set_
  auto original_page = buffer_pool_manager_->FetchPage(root_page_id_);
  original_page->WLatch();
  transaction->AddIntoPageSet(original_page);

  // 逐层下降，找到删除key的正确叶子结点
  auto cur_page = reinterpret_cast<BPlusTreePage *>(original_page->GetData());
  int index;
  while (!cur_page->IsLeafPage()) {
    auto cur_internal_page = reinterpret_cast<InternalPage *>(cur_page);
    // 如果当前结点发生删除键值对也不会导致借数据或合并，说明当前结点是安全的，则可以将其所有祖先节点解W锁并UnpinPage
    if (cur_internal_page->GetSize() > (internal_max_size_ + 1) / 2) {
      while (transaction->GetPageSet()->size() > 1) {
        original_page = transaction->GetPageSet()->front();
        transaction->GetPageSet()->pop_front();
        original_page->WUnlatch();
        buffer_pool_manager_->UnpinPage(original_page->GetPageId(), false);
      }
      if (!cur_page->IsRootPage() && get_root) {
        get_root = false;
        latch_.WUnlock();
      }
    }
    // 找到下一层的页，并上W锁
    cur_internal_page->FindKeyIndex(&index, key, comparator_);
    original_page = buffer_pool_manager_->FetchPage(cur_internal_page->ValueAt(index));
    original_page->WLatch();
    transaction->AddIntoPageSet(original_page);
    cur_page = reinterpret_cast<BPlusTreePage *>(original_page->GetData());
  }

  // 找到叶子结点后，如果叶子节点不存在key，无需删除，直接返回
  auto cur_leaf_page = reinterpret_cast<LeafPage *>(cur_page);
  if (!cur_leaf_page->FindKeyIndex(&index, key, comparator_)) {
    while (!transaction->GetPageSet()->empty()) {
      original_page = transaction->GetPageSet()->front();
      transaction->GetPageSet()->pop_front();
      original_page->WUnlatch();
      buffer_pool_manager_->UnpinPage(original_page->GetPageId(), false);
    }
    if (get_root) {
      get_root = false;
      latch_.WUnlock();
    }
    return;
  }

  // 找到叶子结点后，如果叶子节点存在key，可以删除
  original_page = transaction->GetPageSet()->back();
  // LOG_INFO("$ %d $ %d $ %d $", cur_leaf_page->GetPageId(), index, cur_leaf_page->GetSize());
  cur_leaf_page->RemoveKeyValueAt(index);

  // 如果删除键值对后叶子节点为根结点且为空，则删除叶子极点，并将树置空，可以结束了
  if (cur_leaf_page->IsRootPage() && cur_leaf_page->GetSize() == 0) {
    original_page = transaction->GetPageSet()->front();
    transaction->GetPageSet()->pop_front();
    original_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), true);
    buffer_pool_manager_->DeletePage(cur_leaf_page->GetPageId());
    root_page_id_ = INVALID_PAGE_ID;
    // UpdateRootPageId(1);
    if (get_root) {
      get_root = false;
      latch_.WUnlock();
    }
    return;
  }

  // 如果删除键值对后叶子结点为根结点且非空，或不是根结点但键值对个数大于等于leaf_max_size_/2，可以结束了
  if (cur_leaf_page->IsRootPage() || cur_leaf_page->GetSize() >= leaf_max_size_ / 2) {
    while (transaction->GetPageSet()->size() > 1) {
      original_page = transaction->GetPageSet()->front();
      transaction->GetPageSet()->pop_front();
      original_page->WUnlatch();
      buffer_pool_manager_->UnpinPage(original_page->GetPageId(), false);
    }
    original_page = transaction->GetPageSet()->front();
    transaction->GetPageSet()->pop_front();
    original_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(cur_leaf_page->GetPageId(), true);
    if (get_root) {
      get_root = false;
      latch_.WUnlock();
    }
    return;
  }

  // 如果删除键值键值对后叶子结点的键值对小于leaf_max_size_/2，需要向兄弟节点借数据，或者和兄弟节点合并，先处理叶结点的借数据或合并
  original_page = transaction->GetPageSet()->back();
  transaction->GetPageSet()->pop_back();
  auto parent_original_page = transaction->GetPageSet()->back();
  transaction->GetPageSet()->pop_back();
  auto parent_internal_page = reinterpret_cast<InternalPage *>(parent_original_page->GetData());
  parent_internal_page->FindKeyIndex(&index, key, comparator_);
  Page *left_original_page = nullptr;
  Page *right_original_page = nullptr;
  LeafPage *left_leaf_page = nullptr;
  LeafPage *right_leaf_page = nullptr;
  if (index < parent_internal_page->GetSize() - 1) {
    left_original_page = original_page;
    left_leaf_page = cur_leaf_page;
    right_original_page = buffer_pool_manager_->FetchPage(parent_internal_page->ValueAt(index + 1));
    right_original_page->WLatch();
    right_leaf_page = reinterpret_cast<LeafPage *>(right_original_page->GetData());
    index++;
  } else {
    left_original_page = buffer_pool_manager_->FetchPage(parent_internal_page->ValueAt(index - 1));
    left_original_page->WLatch();
    left_leaf_page = reinterpret_cast<LeafPage *>(left_original_page->GetData());
    right_original_page = original_page;
    right_leaf_page = cur_leaf_page;
  }

  // 如果删完后，和兄弟节点的键值对个数之和大于等于最大允许结点数，则借完数据，更新完父节点就可以结束了
  // LOG_INFO("$ %d $ %d $ %d $", left_leaf_page->GetPageId(), right_leaf_page->GetPageId(),
  // parent_internal_page->GetPageId()); LOG_INFO("# %d # %d # %d #", left_leaf_page->GetSize(),
  // right_leaf_page->GetSize(), parent_internal_page->GetSize());
  if (left_leaf_page->GetSize() + right_leaf_page->GetSize() >= leaf_max_size_) {
    if (left_leaf_page->GetSize() < leaf_max_size_ / 2) {
      int insert_index = left_leaf_page->GetSize();
      left_leaf_page->InsertKeyValueAt(insert_index, right_leaf_page->KeyAt(0), right_leaf_page->ValueAt(0));
      right_leaf_page->RemoveKeyValueAt(0);
    } else {
      int delete_index = left_leaf_page->GetSize() - 1;
      right_leaf_page->InsertKeyValueAt(0, left_leaf_page->KeyAt(delete_index), left_leaf_page->ValueAt(delete_index));
      left_leaf_page->RemoveKeyValueAt(delete_index);
    }

    parent_internal_page->SetKeyAt(index, right_leaf_page->KeyAt(0));

    // LOG_INFO("$ %d $ %d $ %d $", left_leaf_page->GetPageId(), right_leaf_page->GetPageId(),
    // parent_internal_page->GetPageId()); LOG_INFO("# %d # %d # %d #", left_leaf_page->GetSize(),
    // right_leaf_page->GetSize(), parent_internal_page->GetSize());

    left_original_page->WUnlatch();
    right_original_page->WUnlatch();
    parent_original_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(left_leaf_page->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(right_leaf_page->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(parent_internal_page->GetPageId(), true);
    while (!transaction->GetPageSet()->empty()) {
      original_page = transaction->GetPageSet()->front();
      transaction->GetPageSet()->pop_front();
      original_page->WUnlatch();
      buffer_pool_manager_->UnpinPage(original_page->GetPageId(), false);
    }

    if (get_root) {
      get_root = false;
      latch_.WUnlock();
    }
    return;
  }

  // 如果删完后，和兄弟节点的键值对个数之和小于最大允许结点数，需要合并，后续删除父节点对应的键值对，可能引发多重借数据和合并
  for (int i = 0; i < right_leaf_page->GetSize(); i++) {
    left_leaf_page->InsertKeyValueAt(left_leaf_page->GetSize(), right_leaf_page->KeyAt(i), right_leaf_page->ValueAt(i));
  }
  left_leaf_page->SetNextPageId(right_leaf_page->GetNextPageId());
  left_original_page->WUnlatch();
  right_original_page->WUnlatch();
  buffer_pool_manager_->UnpinPage(left_leaf_page->GetPageId(), true);
  buffer_pool_manager_->UnpinPage(right_leaf_page->GetPageId(), true);
  buffer_pool_manager_->DeletePage(right_leaf_page->GetPageId());

  // LOG_INFO("# %d # %d #", left_leaf_page->GetSize(), parent_internal_page->GetSize());
  KeyType delete_key = parent_internal_page->KeyAt(index);
  parent_internal_page->RemoveKeyValueAt(index);

  // 处理内部节点的（多重）借数据和合并
  // LOG_INFO("-----------");
  while (!parent_internal_page->IsRootPage() && parent_internal_page->GetSize() < (internal_max_size_ + 1) / 2) {
    // LOG_INFO("$ %d $ %d #", parent_internal_page->GetPageId(), parent_internal_page->GetSize());
    Page *before_original_page = parent_original_page;
    InternalPage *before_internal_page = parent_internal_page;
    parent_original_page = transaction->GetPageSet()->back();
    transaction->GetPageSet()->pop_back();
    parent_internal_page = reinterpret_cast<InternalPage *>(parent_original_page->GetData());
    parent_internal_page->FindKeyIndex(&index, delete_key, comparator_);
    InternalPage *left_internal_page = nullptr;
    InternalPage *right_internal_page = nullptr;
    if (index < parent_internal_page->GetSize() - 1) {
      left_original_page = before_original_page;
      left_internal_page = before_internal_page;
      right_original_page = buffer_pool_manager_->FetchPage(parent_internal_page->ValueAt(index + 1));
      right_original_page->WLatch();
      right_internal_page = reinterpret_cast<InternalPage *>(right_original_page->GetData());
      index++;
    } else {
      left_original_page = buffer_pool_manager_->FetchPage(parent_internal_page->ValueAt(index - 1));
      left_original_page->WLatch();
      left_internal_page = reinterpret_cast<InternalPage *>(left_original_page->GetData());
      right_original_page = before_original_page;
      right_internal_page = before_internal_page;
    }

    // 删完后，和兄弟节点的键值对个数之和大于最大允许结点数，则借完数据，更新完父节点就可以结束了
    // LOG_INFO("-$ %d $ %d $ %d $-", left_internal_page->GetPageId(), right_internal_page->GetPageId(),
    // parent_internal_page->GetPageId()); LOG_INFO("-# %d # %d # %d #-", left_internal_page->GetSize(),
    // right_internal_page->GetSize(), parent_internal_page->GetSize());
    if (left_internal_page->GetSize() + right_internal_page->GetSize() > internal_max_size_) {
      if (left_internal_page->GetSize() < (internal_max_size_ + 1) / 2) {
        left_internal_page->InsertKeyValueAt(left_internal_page->GetSize(), parent_internal_page->KeyAt(index),
                                             right_internal_page->ValueAt(0));
        parent_internal_page->SetKeyAt(index, right_internal_page->KeyAt(1));
        right_internal_page->RemoveKeyValueAt(0);
        auto child_page = reinterpret_cast<BPlusTreePage *>(
            buffer_pool_manager_->FetchPage(left_internal_page->ValueAt(left_internal_page->GetSize() - 1))->GetData());
        child_page->SetParentPageId(left_internal_page->GetPageId());
        buffer_pool_manager_->UnpinPage(child_page->GetPageId(), true);
      } else {
        right_internal_page->SetKeyAt(0, parent_internal_page->KeyAt(index));
        right_internal_page->InsertKeyValueAt(0, left_internal_page->KeyAt(left_internal_page->GetSize() - 1),
                                              left_internal_page->ValueAt(left_internal_page->GetSize() - 1));
        parent_internal_page->SetKeyAt(index, left_internal_page->KeyAt(left_internal_page->GetSize() - 1));
        left_internal_page->RemoveKeyValueAt(left_internal_page->GetSize() - 1);
        auto child_page = reinterpret_cast<BPlusTreePage *>(
            buffer_pool_manager_->FetchPage(right_internal_page->ValueAt(0))->GetData());
        child_page->SetParentPageId(right_internal_page->GetPageId());
        buffer_pool_manager_->UnpinPage(child_page->GetPageId(), true);
      }

      left_original_page->WUnlatch();
      right_original_page->WUnlatch();
      parent_original_page->WUnlatch();
      buffer_pool_manager_->UnpinPage(left_internal_page->GetPageId(), true);
      buffer_pool_manager_->UnpinPage(right_internal_page->GetPageId(), true);
      buffer_pool_manager_->UnpinPage(parent_internal_page->GetPageId(), true);
      while (!transaction->GetPageSet()->empty()) {
        original_page = transaction->GetPageSet()->front();
        transaction->GetPageSet()->pop_front();
        original_page->WUnlatch();
        buffer_pool_manager_->UnpinPage(original_page->GetPageId(), false);
      }

      if (get_root) {
        get_root = false;
        latch_.WUnlock();
      }
      return;
    }

    // 删完后，和兄弟节点的键值对个数之和小于等于最大允许结点数，需要合并，后续删除父节点对应的键值对，然后判断是否继续进入下一轮借数据与合并
    // LOG_INFO("$ %d $ %d $ %d $", left_internal_page->GetPageId(), right_internal_page->GetPageId(),
    // parent_internal_page->GetPageId()); LOG_INFO("# %d # %d # %d #", left_internal_page->GetSize(),
    // right_internal_page->GetSize(), parent_internal_page->GetSize());
    right_internal_page->SetKeyAt(0, parent_internal_page->KeyAt(index));
    for (int i = 0; i < right_internal_page->GetSize(); i++) {
      int insert_index = left_internal_page->GetSize();
      left_internal_page->InsertKeyValueAt(insert_index, right_internal_page->KeyAt(i),
                                           right_internal_page->ValueAt(i));
      auto child_page = reinterpret_cast<BPlusTreePage *>(
          buffer_pool_manager_->FetchPage(left_internal_page->ValueAt(insert_index))->GetData());
      child_page->SetParentPageId(left_internal_page->GetPageId());
      buffer_pool_manager_->UnpinPage(child_page->GetPageId(), true);
      // LOG_INFO("$ %d $ %d $ %d $", left_internal_page->GetPageId(), right_internal_page->GetPageId(),
      // parent_internal_page->GetPageId()); LOG_INFO("# %d # %d # %d #", left_internal_page->GetSize(),
      // right_internal_page->GetSize(), parent_internal_page->GetSize());
    }
    // LOG_INFO("$ %d $ %d $ %d $", left_internal_page->GetPageId(), right_internal_page->GetPageId(),
    // parent_internal_page->GetPageId()); LOG_INFO("# %d # %d # %d #", left_internal_page->GetSize(),
    // right_internal_page->GetSize(), parent_internal_page->GetSize());
    left_original_page->WUnlatch();
    right_original_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(left_internal_page->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(right_internal_page->GetPageId(), true);
    buffer_pool_manager_->DeletePage(right_internal_page->GetPageId());

    delete_key = parent_internal_page->KeyAt(index);
    parent_internal_page->RemoveKeyValueAt(index);
  }

  if (parent_internal_page->IsRootPage() && parent_internal_page->GetSize() == 1) {
    // LOG_INFO("%d %d", parent_internal_page->GetPageId(), parent_internal_page->GetSize());
    auto child_original_page = buffer_pool_manager_->FetchPage(parent_internal_page->ValueAt(0));
    child_original_page->WLatch();
    auto child_page = reinterpret_cast<BPlusTreePage *>(child_original_page->GetData());
    child_page->SetParentPageId(INVALID_PAGE_ID);
    parent_original_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(parent_internal_page->GetPageId(), true);
    buffer_pool_manager_->DeletePage(parent_internal_page->GetPageId());
    root_page_id_ = child_page->GetPageId();
    // LOG_INFO("%d %d", child_page->GetPageId(), child_page->GetSize());
    child_original_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(child_page->GetPageId(), true);
    UpdateRootPageId(1);
    if (get_root) {
      get_root = false;
      latch_.WUnlock();
    }
    return;
  }
  parent_original_page->WUnlatch();
  buffer_pool_manager_->UnpinPage(parent_internal_page->GetPageId(), true);
  if (get_root) {
    get_root = false;
    latch_.WUnlock();
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
    cur_internal_page->FindKeyIndex(&index, key, comparator_);
    cur_page = reinterpret_cast<BPlusTreePage *>(
        buffer_pool_manager_->FetchPage(cur_internal_page->ValueAt(index))->GetData());
    buffer_pool_manager_->UnpinPage(cur_internal_page->GetPageId(), false);
  }

  auto cur_leaf_page = reinterpret_cast<LeafPage *>(cur_page);
  if (cur_leaf_page->FindKeyIndex(&index, key, comparator_)) {
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
