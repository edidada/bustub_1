#include "storage/page/page_guard.h"
#include "buffer/buffer_pool_manager.h"

namespace bustub {

BasicPageGuard::BasicPageGuard(BasicPageGuard &&that) noexcept {
  is_dirty_ = that.is_dirty_;
  page_ = that.page_;
  bpm_ = that.bpm_;
  that.page_ = nullptr;
  that.bpm_ = nullptr;
  that.is_dirty_ = false;
}

void BasicPageGuard::Drop() {
  bpm_->UnpinPage(PageId(), is_dirty_);
}

auto BasicPageGuard::operator=(BasicPageGuard &&that) noexcept -> BasicPageGuard & {
  Drop();  
  is_dirty_ = that.is_dirty_;
  bpm_ = that.bpm_;
  return *this;
}

BasicPageGuard::~BasicPageGuard(){
  page_ = nullptr;
};  // NOLINT

ReadPageGuard::ReadPageGuard(ReadPageGuard &&that) noexcept = default;

auto ReadPageGuard::operator=(ReadPageGuard &&that) noexcept -> ReadPageGuard & {
  guard_ = std::move(that.guard_);
  return *this;
}

void ReadPageGuard::Drop() {
  // may hold read latch
  guard_.page_->RUnlatch();
  guard_.Drop();
}

ReadPageGuard::~ReadPageGuard() {
  Drop();
}  // NOLINT

WritePageGuard::WritePageGuard(WritePageGuard &&that) noexcept = default;

auto WritePageGuard::operator=(WritePageGuard &&that) noexcept -> WritePageGuard & {
  guard_ = std::move(that.guard_);
  return *this;
}

void WritePageGuard::Drop() {
  guard_.page_->WUnlatch();
  guard_.Drop();
}

WritePageGuard::~WritePageGuard() {
  guard_.Drop();
}  // NOLINT

}  // namespace bustub
