//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// b_plus_tree_delete_test.cpp
//
// Identification: test/storage/b_plus_tree_delete_test.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <cstdio>

#include "buffer/buffer_pool_manager_instance.h"
#include "gtest/gtest.h"
#include "storage/index/b_plus_tree.h"
#include "test_util.h"  // NOLINT

// #include <numeric>
// #include <random>
// #include <string>

namespace bustub {

TEST(BPlusTreeTests, DISABLED_DeleteTest1) {
  // create KeyComparator and index schema
  auto key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema.get());

  auto *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManagerInstance(50, disk_manager);
  // create b+ tree
  // BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator);
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator, 3, 3);
  GenericKey<8> index_key;
  RID rid;
  // create transaction
  auto *transaction = new Transaction(0);

  // create and fetch header_page
  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;

  // std::vector<int64_t> keys = {1, 2, 3, 4, 5};
  std::vector<int64_t> keys = {1, 3, 5, 7, 9, 4, 10, 8, 6, 11};
  for (auto key : keys) {
    int64_t value = key & 0xFFFFFFFF;
    rid.Set(static_cast<int32_t>(key >> 32), value);
    index_key.SetFromInteger(key);
    tree.Insert(index_key, rid, transaction);
  }

  std::vector<RID> rids;
  for (auto key : keys) {
    rids.clear();
    index_key.SetFromInteger(key);
    tree.GetValue(index_key, &rids);
    EXPECT_EQ(rids.size(), 1);

    int64_t value = key & 0xFFFFFFFF;
    EXPECT_EQ(rids[0].GetSlotNum(), value);
  }

  // std::vector<int64_t> remove_keys = {1, 5};
  std::vector<int64_t> remove_keys = {5, 6};
  for (auto key : remove_keys) {
    index_key.SetFromInteger(key);
    tree.Remove(index_key, transaction);
  }

  int64_t size = 0;
  bool is_present;

  for (auto key : keys) {
    rids.clear();
    index_key.SetFromInteger(key);
    is_present = tree.GetValue(index_key, &rids);

    if (!is_present) {
      EXPECT_NE(std::find(remove_keys.begin(), remove_keys.end(), key), remove_keys.end());
    } else {
      EXPECT_EQ(rids.size(), 1);
      EXPECT_EQ(rids[0].GetPageId(), 0);
      EXPECT_EQ(rids[0].GetSlotNum(), key);
      size = size + 1;
    }
  }

  // EXPECT_EQ(size, 3);
  EXPECT_EQ(size, 8);

  bpm->UnpinPage(HEADER_PAGE_ID, true);
  delete transaction;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}

TEST(BPlusTreeTests, DISABLED_DeleteTest2) {
  // create KeyComparator and index schema
  auto key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema.get());

  auto *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManagerInstance(50, disk_manager);
  // create b+ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator);
  GenericKey<8> index_key;
  RID rid;
  // create transaction
  auto *transaction = new Transaction(0);

  // create and fetch header_page
  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;

  std::vector<int64_t> keys = {1, 2, 3, 4, 5};
  for (auto key : keys) {
    int64_t value = key & 0xFFFFFFFF;
    rid.Set(static_cast<int32_t>(key >> 32), value);
    index_key.SetFromInteger(key);
    tree.Insert(index_key, rid, transaction);
  }

  std::vector<RID> rids;
  for (auto key : keys) {
    rids.clear();
    index_key.SetFromInteger(key);
    tree.GetValue(index_key, &rids);
    EXPECT_EQ(rids.size(), 1);

    int64_t value = key & 0xFFFFFFFF;
    EXPECT_EQ(rids[0].GetSlotNum(), value);
  }

  std::vector<int64_t> remove_keys = {1, 5, 3, 4};
  for (auto key : remove_keys) {
    index_key.SetFromInteger(key);
    tree.Remove(index_key, transaction);
  }

  int64_t size = 0;
  bool is_present;

  for (auto key : keys) {
    rids.clear();
    index_key.SetFromInteger(key);
    is_present = tree.GetValue(index_key, &rids);

    if (!is_present) {
      EXPECT_NE(std::find(remove_keys.begin(), remove_keys.end(), key), remove_keys.end());
    } else {
      EXPECT_EQ(rids.size(), 1);
      EXPECT_EQ(rids[0].GetPageId(), 0);
      EXPECT_EQ(rids[0].GetSlotNum(), key);
      size = size + 1;
    }
  }

  EXPECT_EQ(size, 1);

  bpm->UnpinPage(HEADER_PAGE_ID, true);
  delete transaction;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}

// TEST(BPlusTreeTests, DISABLED_InsertTest2) {
//   // create KeyComparator and index schema
//   auto key_schema = ParseCreateStatement("a bigint");
//   GenericComparator<8> comparator(key_schema.get());

//   auto *disk_manager = new DiskManager("test.db");
//   BufferPoolManager *bpm = new BufferPoolManagerInstance(50, disk_manager);
//   // create b+ tree
//   BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator, 5, 4);
//   GenericKey<8> index_key;
//   RID rid;
//   // create transaction
//   auto *transaction = new Transaction(0);

//   // create and fetch header_page
//   page_id_t page_id;
//   auto header_page = bpm->NewPage(&page_id);
//   (void)header_page;

//   // std::vector<int64_t> keys = {14, 1, 12, 11, 6, 10, 9, 7, 13, 15, 16, 5, 3, 4, 17, 8, 2};
//   std::vector<int64_t> keys = {14, 1, 12, 11, 6, 10, 9, 7, 13, 15, 16, 5, 3, 4, 17, 8, 2};
//   for (auto key : keys) {
//     int64_t value = key & 0xFFFFFFFF;
//     rid.Set(static_cast<int32_t>(key >> 32), value);
//     index_key.SetFromInteger(key);
//     tree.Insert(index_key, rid, transaction);
//   }

//   std::vector<RID> rids;
//   // for (auto key : keys) {
//   //   rids.clear();
//   //   index_key.SetFromInteger(key);
//   //   tree.GetValue(index_key, &rids);
//   //   EXPECT_EQ(rids.size(), 1);

//   //   int64_t value = key & 0xFFFFFFFF;
//   //   EXPECT_EQ(rids[0].GetSlotNum(), value);
//   // }

//   int64_t size = 0;
//   bool is_present;

//   int64_t cnt = 0;

//   for (auto key : keys) {
//     rids.clear();
//     index_key.SetFromInteger(key);
//     is_present = tree.GetValue(index_key, &rids);

//     // EXPECT_EQ(is_present, true);
//     // EXPECT_EQ(rids.size(), 1);
//     // EXPECT_EQ(rids[0].GetPageId(), 0);
//     // EXPECT_EQ(rids[0].GetSlotNum(), key);
//     size = size + 1;

//     if (!is_present) {
//       cnt++;
//     }
//   }

//   EXPECT_EQ(size, keys.size());
//   EXPECT_EQ(cnt, 0);

//   bpm->UnpinPage(HEADER_PAGE_ID, true);
//   delete transaction;
//   delete disk_manager;
//   delete bpm;
//   remove("test.db");
//   remove("test.log");
// }

// TEST(BPlusTreeTests, DISABLED_ScaleTest) {
//   // create KeyComparator and index schema
//   auto key_schema = ParseCreateStatement("a bigint");
//   GenericComparator<8> comparator(key_schema.get());

//   auto *disk_manager = new DiskManager("test.db");
//   BufferPoolManager *bpm = new BufferPoolManagerInstance(30, disk_manager);
//   // create b+ tree
//   BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator, 2, 3);
//   GenericKey<8> index_key;
//   RID rid;
//   // create transaction
//   auto *transaction = new Transaction(0);

//   // create and fetch header_page
//   page_id_t page_id;
//   auto header_page = bpm->NewPage(&page_id);
//   (void)header_page;

//   int64_t scale = 100000;
//   std::vector<int64_t> keys;
//   for (int64_t key = 1; key < scale; key++) {
//     keys.push_back(key);
//   }

//   // randomized the insertion order
//   auto rng = std::default_random_engine{};
//   std::shuffle(keys.begin(), keys.end(), rng);
//   for (auto key : keys) {
//     int64_t value = key & 0xFFFFFFFF;
//     rid.Set(static_cast<int32_t>(key >> 32), value);
//     index_key.SetFromInteger(key);
//     tree.Insert(index_key, rid, transaction);
//   }

//   int64_t new_size = 0;
//   for (auto iterator = tree.Begin(); iterator != tree.End(); ++iterator) {
//     new_size = new_size + 1;
//   }
//   EXPECT_EQ(new_size, 99999);

//   std::vector<RID> rids;
//   int64_t cnt = 0;
//   for (auto key : keys) {
//     rids.clear();
//     index_key.SetFromInteger(key);
//     if (!tree.GetValue(index_key, &rids)) {
//       cnt++;
//     }
//     EXPECT_EQ(rids.size(), 1);

//     int64_t value = key & 0xFFFFFFFF;
//     EXPECT_EQ(rids[0].GetSlotNum(), value);
//   }
//   EXPECT_EQ(cnt, 0);

//   bpm->UnpinPage(HEADER_PAGE_ID, true);
//   delete transaction;
//   delete disk_manager;
//   delete bpm;
//   remove("test.db");
//   remove("test.log");
// }

// TEST(GradeScopeBPlusTreeTests, ScaleTest) {
//   auto key_schema = ParseCreateStatement("a bigint");
//   GenericComparator<8> comparator(key_schema.get());

//   auto *disk_manager = new DiskManager("test.db");
//   BufferPoolManager *bpm = new BufferPoolManagerInstance(30, disk_manager);
//   // create b+ tree
//   BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator, 2, 3);
//   GenericKey<8> index_key;
//   RID rid;
//   // create transaction
//   Transaction *transaction = new Transaction(0);
//   // create and fetch header_page
//   page_id_t page_id;
//   auto header_page = bpm->NewPage(&page_id);
//   (void)header_page;

//   int64_t scale = 100000;
//   std::vector<int64_t> keys;
//   for (int64_t key = 1; key < scale; key++) {
//     keys.push_back(key);
//   }

//   // shuffle keys
//   // std::random_shuffle(keys.begin(), keys.end());
//   // NOTE: 'std::random_shuffle' has been removed in C++17; use 'std::shuffle' instead
//   // std::shuffle(keys.begin(), keys.end(), std::mt19937(std::random_device()));
//   auto rng = std::default_random_engine{};
//   std::shuffle(keys.begin(), keys.end(), rng);
//   for (auto key : keys) {
//     int64_t value = key & 0xFFFFFFFF;
//     rid.Set(static_cast<int32_t>(key >> 32), value);
//     index_key.SetFromInteger(key);
//     tree.Insert(index_key, rid, transaction);
//   }
//   std::vector<RID> rids;
//   for (auto key : keys) {
//     rids.clear();
//     index_key.SetFromInteger(key);
//     tree.GetValue(index_key, &rids);
//     EXPECT_EQ(rids.size(), 1);

//     int64_t value = key & 0xFFFFFFFF;
//     EXPECT_EQ(rids[0].GetSlotNum(), value);
//   }

//   int64_t start_key = 1;
//   int64_t current_key = start_key;
//   // for (auto pair : tree) {
//   //   (void)pair;
//   //   current_key = current_key + 1;
//   // }
//   index_key.SetFromInteger(start_key);
//   for (auto iterator = tree.Begin(index_key); iterator != tree.End(); ++iterator) {
//     current_key = current_key + 1;
//   }
//   EXPECT_EQ(current_key, keys.size() + 1);

//   int64_t remove_scale = 99000;
//   std::vector<int64_t> remove_keys;
//   for (int64_t key = 1; key < remove_scale; key++) {
//     remove_keys.push_back(key);
//   }

//   // shuffle remove_keys
//   std::shuffle(remove_keys.begin(), remove_keys.end(), rng);
//   for (auto key : remove_keys) {
//     index_key.SetFromInteger(key);
//     tree.Remove(index_key, transaction);
//   }

//   start_key = remove_scale;
//   int64_t size = 0;
//   // for (auto pair : tree) {
//   //   (void)pair;
//   //   size = size + 1;
//   // }
//   index_key.SetFromInteger(start_key);
//   for (auto iterator = tree.Begin(index_key); iterator != tree.End(); ++iterator) {
//     size = size + 1;
//   }
//   EXPECT_EQ(size, 1000);

//   // int64_t new_size = 0;
//   // index_key.SetFromInteger(start_key);
//   // for (auto iterator = tree.Begin(); iterator != tree.End(); ++iterator) {
//   //   new_size = new_size + 1;
//   // }
//   // EXPECT_EQ(new_size, 100);

//   remove_keys.clear();
//   for (int64_t key = remove_scale; key < scale; key++) {
//     remove_keys.push_back(key);
//   }
//   for (auto key : remove_keys) {
//     index_key.SetFromInteger(key);
//     tree.Remove(index_key, transaction);
//   }
//   EXPECT_EQ(true, tree.IsEmpty());

//   bpm->UnpinPage(HEADER_PAGE_ID, true);
//   delete transaction;
//   delete disk_manager;
//   delete bpm;
//   remove("test.db");
//   remove("test.log");
// }

// TEST(GradeScopeBPlusTreeTests, DISABLED_SequentialMixTest) {
//   auto key_schema = ParseCreateStatement("a bigint");
//   GenericComparator<8> comparator(key_schema.get());

//   auto *disk_manager = new DiskManager("test.db");
//   BufferPoolManager *bpm = new BufferPoolManagerInstance(50, disk_manager);
//   // create b+ tree
//   BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator, 4, 4);
//   GenericKey<8> index_key;
//   RID rid;
//   // create transaction
//   Transaction *transaction = new Transaction(0);

//   // create and fetch header_page
//   page_id_t page_id;
//   auto header_page = bpm->NewPage(&page_id);
//   (void)header_page;
//   // first, populate index
//   std::vector<int64_t> for_insert;
//   std::vector<int64_t> for_delete;
//   size_t sieve = 2;  // divide evenly
//   size_t total_keys = 1000;
//   for (size_t i = 1; i <= total_keys; i++) {
//     if (i % sieve == 0) {
//       for_insert.push_back(i);
//     } else {
//       for_delete.push_back(i);
//     }
//   }

//   // Insert all the keys, including the ones that will remain at the end and
//   // the ones that are going to be removed next.
//   for (size_t i = 0; i < total_keys / 2; i++) {
//     int64_t insert_key = for_insert[i];
//     int64_t insert_value = insert_key & 0xFFFFFFFF;
//     rid.Set(static_cast<int32_t>(insert_key >> 32), insert_value);
//     index_key.SetFromInteger(insert_key);
//     tree.Insert(index_key, rid, transaction);

//     int64_t delete_key = for_delete[i];
//     int64_t delete_value = delete_key & 0xFFFFFFFF;
//     rid.Set(static_cast<int32_t>(delete_key >> 32), delete_value);
//     index_key.SetFromInteger(delete_key);
//     tree.Insert(index_key, rid, transaction);
//   }

//   // Remove the keys in for_delete
//   for (auto key : for_delete) {
//     index_key.SetFromInteger(key);
//     tree.Remove(index_key, transaction);
//   }

//   // Only half of the keys should remain
//   int64_t start_key = 2;
//   int64_t size = 0;
//   index_key.SetFromInteger(start_key);
//   //  for (auto pair : tree) {
//   for (auto iterator = tree.Begin(index_key); iterator != tree.End(); ++iterator) {
//     auto key = (*iterator).first;
//     EXPECT_EQ(key.ToString(), for_insert[size]);
//     size++;
//   }

//   EXPECT_EQ(size, for_insert.size());

//   bpm->UnpinPage(HEADER_PAGE_ID, true);
//   delete transaction;
//   delete disk_manager;
//   delete bpm;
//   remove("test.db");
//   remove("test.log");
// }

}  // namespace bustub
