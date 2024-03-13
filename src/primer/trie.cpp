#include "primer/trie.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stack>
#include <string_view>
#include "common/exception.h"
#include "common/macros.h"

namespace bustub {

template <class T>
auto Trie::Get(std::string_view key) const -> const T * {
  // throw NotImplementedException("Trie::Get is not implemented.");
  std::shared_ptr<const TrieNode> node = root_;
  size_t loc = 0;
  while (node != nullptr && loc < key.length()) {
    auto iter = node->children_.find(key[loc++]);
    if (iter == node->children_.end()) {
      node.reset();
      break;
    }
    node = iter->second;
  }
  if (node != nullptr && node->is_value_node_) {
    auto res = dynamic_cast<const TrieNodeWithValue<T> *>(node.get());
    return (res == nullptr) ? nullptr : res->value_.get();
  }
  return nullptr;
  // You should walk through the trie to find the node corresponding to the key. If the node doesn't exist, return
  // nullptr. After you find the node, you should use `dynamic_cast` to cast it to `const TrieNodeWithValue<T> *`. If
  // dynamic_cast returns `nullptr`, it means the type of the value is mismatched, and you should return nullptr.
  // Otherwise, return the value.
}

template <class T>
auto Trie::Put(std::string_view key, T value) const -> Trie {
  // Note that `T` might be a non-copyable type. Always use `std::move` when creating `shared_ptr` on that value.
  // throw NotImplementedException("Trie::Put is not implemented.");
  // @note For copy on write tree, every single write create a new node
  std::shared_ptr<const TrieNode> node = root_;
  std::stack<std::shared_ptr<const TrieNode>> path;
  size_t loc = 0;
  // find pos to create new node
  while (node != nullptr && loc < key.length()) {
    path.push(node);
    auto next_node = node->children_.find(key[loc++]);
    if (next_node == node->children_.end()) {
      node = nullptr;
    } else {
      node = next_node->second;
    }
  }
  // create new node if needed
  size_t pos = key.length() - 1;
  std::shared_ptr<TrieNode> prev;
  if (node != nullptr) {
    auto temp = node->Clone();
    prev = std::make_shared<TrieNodeWithValue<T>>(temp->children_, std::make_shared<T>(std::forward<T>(value)));
  } else {
    prev = std::make_shared<TrieNodeWithValue<T>>(std::make_shared<T>(std::forward<T>(value)));
  }
  std::shared_ptr<TrieNode> iter = prev;
  while (pos < SIZE_MAX && pos >= loc) {
    iter = std::make_shared<TrieNode>();
    iter->children_.emplace(key[pos--], prev);
    prev = iter;
  }
  // copy old nodes and update children_
  while (!path.empty()) {
    node = path.top();
    auto new_node = node->Clone();
    new_node->children_[key[pos--]] = iter;
    iter = std::shared_ptr<TrieNode>(std::move(new_node));
    path.pop();
  }
  Trie ret(iter);
  return ret;
  // You should walk through the trie and create new nodes if necessary. If the node corresponding to the key already
  // exists, you should create a new `TrieNodeWithValue`.
}

auto Trie::Remove(std::string_view key) const -> Trie {
  /// throw NotImplementedException("Trie::Remove is not implemented.");
  /// is_value_node is expected to use in multi-methods
  if (key.empty()) {
    return *this;
  }
  std::shared_ptr<const TrieNode> node = root_;
  std::stack<std::shared_ptr<const TrieNode>> path;
  size_t loc = 0;
  while (node != nullptr && loc < key.length()) {
    path.push(node);
    auto iter = node->children_.find(key[loc++]);
    if (iter == node->children_.end()) {
      node = nullptr;
    } else {
      node = iter->second;
    }
  }
  if (node == nullptr) {
    return *this;
  }
  BUSTUB_ASSERT(node->is_value_node_, ("Find a non-value node to remove"));
  std::shared_ptr<TrieNode> new_node = std::make_shared<TrieNode>(node->children_);
  loc = key.length()-1;
  while (!path.empty()) {
    auto temp = path.top();
    auto prev = temp->Clone();
    if (new_node->children_.empty() && !new_node->is_value_node_) {
      prev->children_.erase(key[loc]);
    } else {
      prev->children_[key[loc]] = new_node;
    }
    new_node = std::shared_ptr<TrieNode>(std::move(prev));
    --loc;
    path.pop();
  }
  return Trie(new_node);
  // You should walk through the trie and remove nodes if necessary. If the node doesn't contain a value any more,
  // you should convert it to `TrieNode`. If a node doesn't have children any more, you should remove it.
}

// Below are explicit instantiation of template functions.
//
// Generally people would write the implementation of template classes and functions in the header file. However, we
// separate the implementation into a .cpp file to make things clearer. In order to make the compiler know the
// implementation of the template functions, we need to explicitly instantiate them here, so that they can be picked up
// by the linker.

template auto Trie::Put(std::string_view key, uint32_t value) const -> Trie;
template auto Trie::Get(std::string_view key) const -> const uint32_t *;

template auto Trie::Put(std::string_view key, uint64_t value) const -> Trie;
template auto Trie::Get(std::string_view key) const -> const uint64_t *;

template auto Trie::Put(std::string_view key, std::string value) const -> Trie;
template auto Trie::Get(std::string_view key) const -> const std::string *;

// If your solution cannot compile for non-copy tests, you can remove the below lines to get partial score.

using Integer = std::unique_ptr<uint32_t>;

template auto Trie::Put(std::string_view key, Integer value) const -> Trie;
template auto Trie::Get(std::string_view key) const -> const Integer *;

template auto Trie::Put(std::string_view key, MoveBlocked value) const -> Trie;
template auto Trie::Get(std::string_view key) const -> const MoveBlocked *;

}  // namespace bustub
