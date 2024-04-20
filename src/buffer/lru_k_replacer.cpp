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
#include <string>
#include <time.h>
#include "common/exception.h"

namespace bustub {

void LRUKNode::AccessFrame() {
  if (history_.size() == k_+1) {
    history_.pop_front();
  }
  history_.emplace_back(static_cast<size_t>(time(nullptr)));
}

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k) {}

auto LRUKReplacer::Evict(frame_id_t *frame_id) -> bool {
  std::lock_guard<std::mutex> lock(latch_);
  bool succ = false;
  auto erase_from_list = [this] (std::list<frame_id_t>& list, frame_id_t* frame_id) -> bool {
    for (auto iter = list.begin(); iter != list.end(); ++iter) {
      if (node_store_.at(*iter).IsEvictable()) {
        *frame_id = *iter;
        node_store_.erase(*iter);
        list.erase(iter);
        return true;
      }
    }
    return false;
  };
  succ = erase_from_list(this->history_list_, frame_id);
  if (!succ) {
    succ = erase_from_list(this->frame_pool_, frame_id);
  }
  curr_size_ = (succ) ? curr_size_-1 : curr_size_;
  return succ;
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id, [[maybe_unused]] AccessType access_type) {
  BUSTUB_ASSERT((size_t)frame_id < replacer_size_, "Try to access an invalid frame");
  std::lock_guard<std::mutex> lock(latch_);
  auto lru_node_ptr = node_store_.find(frame_id);
  if (lru_node_ptr == node_store_.end()) {
    LRUKNode new_node(k_, frame_id, true);
    new_node.AccessFrame();
    // has to evict
    history_list_.emplace_back(frame_id);
    node_store_.emplace(frame_id, std::move(new_node));
    ++curr_size_;
  } else {
    lru_node_ptr->second.AccessFrame();
    if (lru_node_ptr->second.GetAccessCount() == k_) {
      // move from fifo_list to lru_list
      history_list_.remove(frame_id);
      frame_pool_.emplace_back(frame_id);
    } else if (lru_node_ptr->second.GetAccessCount() > k_) {
      frame_pool_.remove(frame_id);
      frame_pool_.emplace_back(frame_id);
    } else {
      history_list_.remove(frame_id);
      history_list_.emplace_back(frame_id);
    }
  }
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  BUSTUB_ASSERT((size_t)frame_id < replacer_size_, "Try to access an invalid frame");
  std::lock_guard<std::mutex> lock(latch_);
  auto lru_node_ptr = node_store_.find(frame_id);
  bool prev_evict = false;
  if (lru_node_ptr != node_store_.end()) {
    prev_evict = lru_node_ptr->second.IsEvictable();
    lru_node_ptr->second.SetIsEvictable(set_evictable);
  }
  if (prev_evict && !set_evictable) {
    --curr_size_;
  }
  if (!prev_evict && set_evictable) {
    ++curr_size_;
  }
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
  BUSTUB_ASSERT((size_t)frame_id < replacer_size_, "Try to access an invalid frame");
  std::lock_guard<std::mutex> lock(latch_);
  auto lru_node_ptr = node_store_.find(frame_id);
  if (lru_node_ptr != node_store_.end()) {
    if (!lru_node_ptr->second.IsEvictable()) {
      throw bustub::Exception("Try to remove a pinned frame " + std::to_string(frame_id));
    }
    node_store_.erase(frame_id);
    history_list_.remove(frame_id);
    frame_pool_.remove(frame_id);
    --curr_size_;
  }
}

auto LRUKReplacer::Size() -> size_t { return curr_size_; }

}  // namespace bustub
