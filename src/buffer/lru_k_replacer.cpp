//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.cpp
//
// Identification: src/buffer/lru_k_replacer.cpp
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_k_replacer.h"

namespace bustub {

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k) {}

auto LRUKReplacer::Evict(frame_id_t *frame_id) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);

  size_t min_time = current_timestamp_;
  bool is_find = false;
  bool is_inf = false;

  for (auto item : frames_is_evictable_) {
    is_find = true;
    if (!hist_[item].empty() && hist_[item].size() < k_) {
      if (!is_inf) {
        min_time = hist_[item][hist_[item].size() - 1];
        *frame_id = item;
        is_inf = true;
      } else {
        if (hist_[item][hist_[item].size() - 1] < min_time) {
          *frame_id = item;
          min_time = hist_[item][hist_[item].size() - 1];
        }
      }
    } else if (!is_inf && hist_[item].size() == k_) {
      if (hist_[item][k_ - 1] < min_time) {
        *frame_id = item;
        min_time = hist_[item][k_ - 1];
      }
    }
  }

  if (is_find) {
    frames_is_evictable_.erase(*frame_id);
    frames_in_buffer_.erase(*frame_id);
    curr_size_--;
    hist_[*frame_id].resize(0);
  }

  return is_find;
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id) {
  std::scoped_lock<std::mutex> lock(latch_);

  if (frame_id > static_cast<frame_id_t>(replacer_size_)) {
    BUSTUB_ASSERT(frame_id > static_cast<frame_id_t>(replacer_size_), "frame id is invalid.");
  }

  current_timestamp_++;
  frames_in_buffer_.insert(frame_id);
  if (hist_[frame_id].size() < k_) {
    hist_[frame_id].resize(hist_[frame_id].size() + 1);
  }

  for (int i = hist_[frame_id].size() - 1; i > 0; i--) {
    hist_[frame_id][i] = hist_[frame_id][i - 1];
  }
  hist_[frame_id][0] = current_timestamp_;
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  std::scoped_lock<std::mutex> lock(latch_);

  if (frame_id > static_cast<frame_id_t>(replacer_size_)) {
    BUSTUB_ASSERT(frame_id > static_cast<frame_id_t>(replacer_size_), "frame id is invalid.");
  }

  if (frames_in_buffer_.find(frame_id) == frames_in_buffer_.end()) {
    return;
  }

  if (set_evictable && frames_is_evictable_.find(frame_id) == frames_is_evictable_.end()) {
    frames_is_evictable_.insert(frame_id);
    curr_size_++;
  }
  if (!set_evictable && frames_is_evictable_.find(frame_id) != frames_is_evictable_.end()) {
    frames_is_evictable_.erase(frame_id);
    curr_size_--;
  }
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
  std::scoped_lock<std::mutex> lock(latch_);

  if (frames_in_buffer_.find(frame_id) == frames_in_buffer_.end()) {
    return;
  }

  if (frames_is_evictable_.find(frame_id) == frames_is_evictable_.end()) {
    BUSTUB_ASSERT(frames_is_evictable_.find(frame_id) == frames_is_evictable_.end(),
                  "Remove is called on a non-evictable frame");
  }

  frames_is_evictable_.erase(frame_id);
  frames_in_buffer_.erase(frame_id);
  curr_size_--;
  hist_[frame_id].resize(0);
}

auto LRUKReplacer::Size() -> size_t { return curr_size_; }

}  // namespace bustub
