//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/buffer_pool_manager.h"

#include "common/exception.h"
#include "common/macros.h"
#include "storage/page/page_guard.h"

namespace bustub {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager, size_t replacer_k,
                                     LogManager *log_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager), log_manager_(log_manager) {
  // TODO(students): remove this line after you have implemented the buffer pool manager
  // we allocate a consecutive memory space for the buffer pool
  pages_ = new Page[pool_size_];
  replacer_ = std::make_unique<LRUKReplacer>(pool_size, replacer_k);

  // Initially, every page is in the free list.
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<int>(i));
  }
}

BufferPoolManager::~BufferPoolManager() { delete[] pages_; }

auto BufferPoolManager::NewPage(page_id_t *page_id) -> Page * {
  std::lock_guard<std::mutex> lock(latch_);
  Page * result = nullptr;
  frame_id_t frame = -1;
  if (!free_list_.empty()) {
    frame = free_list_.front();
    free_list_.pop_front();
  } 
  if (frame == -1) {
    replacer_->Evict(&frame);
  }
  if (frame != -1) {
    *page_id = AllocatePage();
    result = &pages_[frame];
    page_table_.erase(result->GetPageId());
    {
      result->WLatch();
      if (result->IsDirty()) {
        disk_manager_->WritePage(result->GetPageId(), result->GetData());
        result->is_dirty_ = false;
      } 
      result->page_id_ = *page_id;
      ++result->pin_count_;
      result->ResetMemory();
      result->WUnlatch();
    }
    page_table_[*page_id] = frame;
    replacer_->RecordAccess(frame);
    replacer_->SetEvictable(frame, false);
  }
  return result;
}

auto BufferPoolManager::FetchPage(page_id_t page_id, [[maybe_unused]] AccessType access_type) -> Page * {
  std::lock_guard<std::mutex> lock(latch_);
  auto iter = page_table_.find(page_id);
  Page * fetch_page = nullptr;
  if (iter != page_table_.end()) {
    fetch_page = &pages_[iter->second];
    ++fetch_page->pin_count_;
    replacer_->RecordAccess(iter->second);
    replacer_->SetEvictable(iter->second, false);
    return fetch_page;
  }
  // find a free page first 
  frame_id_t new_frame = -1;
  if (!free_list_.empty()) {
    new_frame = free_list_.front();
    free_list_.pop_front();
  }
  if (new_frame == -1) {
    replacer_->Evict(&new_frame);
  }
  if (new_frame != -1) {
    fetch_page = &pages_[new_frame];
    {
      if (fetch_page->IsDirty()) {
        disk_manager_->WritePage(fetch_page->GetPageId(), fetch_page->GetData());
        fetch_page->is_dirty_ = false;
      }
      fetch_page->page_id_ = page_id;
      ++fetch_page->pin_count_;
      disk_manager_->ReadPage(page_id, fetch_page->GetData());
    }
    replacer_->RecordAccess(new_frame);
    replacer_->SetEvictable(new_frame, false);
    page_table_[page_id] = new_frame;
  }
  return fetch_page;
}

auto BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty, [[maybe_unused]] AccessType access_type) -> bool {
  BUSTUB_ASSERT(page_id != INVALID_PAGE_ID, "Try to operate an invalid page");
  std::lock_guard<std::mutex> lock(latch_);
  auto iter = page_table_.find(page_id);
  if (iter == page_table_.end() || pages_[iter->second].GetPinCount() == 0) {
    return false;
  }
  Page * target_page = &pages_[iter->second];
  target_page->is_dirty_ = is_dirty;
  --target_page->pin_count_;
  if (target_page->pin_count_ == 0) {
    replacer_->SetEvictable(iter->second, true);
  }
  return true;
}

auto BufferPoolManager::FlushPage(page_id_t page_id) -> bool {
  BUSTUB_ASSERT(page_id != INVALID_PAGE_ID, "Try to flush an invalid page to disk");
  std::lock_guard<std::mutex> lock(latch_);
  auto iter = page_table_.find(page_id);
  if (iter == page_table_.end()) {
    return false;
  }
  Page * target_page = &pages_[iter->second];
  disk_manager_->WritePage(page_id, target_page->GetData());
  target_page->is_dirty_ = false;
  return true;
}

void BufferPoolManager::FlushAllPages() {
  for (auto & iter : page_table_) {  
    FlushPage(iter.first);
  }
}

auto BufferPoolManager::DeletePage(page_id_t page_id) -> bool {
  BUSTUB_ASSERT(page_id != INVALID_PAGE_ID, "Try to delete an invalid page from buffer");
  std::lock_guard<std::mutex> lock(latch_);
  auto iter = page_table_.find(page_id);
  if (iter == page_table_.end()) {
    return true;
  }
  Page * del_page = &pages_[iter->second];
  // may need page latch here
  if (del_page->GetPinCount() != 0) {
    return false;
  }
  del_page->is_dirty_ = false;
  del_page->page_id_ = INVALID_PAGE_ID;
  del_page->ResetMemory();
  DeallocatePage(page_id);
  replacer_->Remove(iter->second);
  free_list_.emplace_back(iter->second);
  return true;
}

auto BufferPoolManager::AllocatePage() -> page_id_t { return next_page_id_++; }

auto BufferPoolManager::FetchPageBasic(page_id_t page_id) -> BasicPageGuard { return {this, FetchPage(page_id)}; }

auto BufferPoolManager::FetchPageRead(page_id_t page_id) -> ReadPageGuard {
  Page * read_page = FetchPage(page_id);
  read_page->RLatch();
  return {this, read_page};
}

auto BufferPoolManager::FetchPageWrite(page_id_t page_id) -> WritePageGuard {
  Page * write_page = FetchPage(page_id);
  write_page->WLatch();
  return {this, write_page};
}

auto BufferPoolManager::NewPageGuarded(page_id_t *page_id) -> BasicPageGuard {
  NewPage(page_id);
  return FetchPageBasic(*page_id);
}

}  // namespace bustub
