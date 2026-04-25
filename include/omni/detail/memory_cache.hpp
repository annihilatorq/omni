#pragma once

#include <mutex>
#include <optional>
#include <unordered_map>

namespace omni::detail {

  template <typename KeyT, typename ValueT, typename Hasher = std::hash<KeyT>>
  class memory_cache {
   public:
    using hasher = Hasher;
    using key_type = KeyT;
    using value_type = ValueT;
    using storage_type = std::unordered_map<key_type, value_type, hasher>;

    std::optional<value_type> try_get(const key_type& export_hash) {
      std::scoped_lock lock(mutex_);
      auto it = storage_.find(export_hash);
      return it == storage_.end() ? std::nullopt : std::make_optional(it->second);
    }

    void set(const key_type& key, value_type value) {
      std::scoped_lock lock(mutex_);
      storage_[key] = std::move(value);
    }

    void remove(const key_type& key) {
      std::scoped_lock lock(mutex_);
      storage_.erase(key);
    }

    void clear() {
      std::scoped_lock lock(mutex_);
      storage_.clear();
    }

    [[nodiscard]] bool contains(const key_type& key) const {
      std::scoped_lock lock(mutex_);
      return storage_.contains(key);
    }

    [[nodiscard]] std::size_t size() const {
      std::scoped_lock lock(mutex_);
      return storage_.size();
    }

   private:
    mutable std::mutex mutex_;
    storage_type storage_{};
  };

} // namespace omni::detail
