#pragma once

#include <concepts>
#include <cstddef>
#include <ranges>

#include "omni/address.hpp"
#include "omni/win/directories.hpp"

namespace omni::concepts {

  template <typename Range, typename FindKey, typename Value = std::ranges::range_value_t<Range>>
  concept export_range = std::ranges::viewable_range<Range> && std::bidirectional_iterator<typename Range::iterator> &&
                         std::same_as<std::ranges::range_value_t<Range>, Value> &&
                         requires(const Range& range, FindKey key, std::size_t index, omni::address address) {
                           typename Range::iterator;

                           { range.address(index) } -> std::same_as<omni::address>;
                           { range.is_forwarded(address) } -> std::same_as<bool>;
                           { range.directory() } -> std::same_as<const win::export_directory*>;
                           { range.size() } -> std::same_as<std::size_t>;
                           { range.begin() } -> std::same_as<typename Range::iterator>;
                           { range.end() } -> std::same_as<typename Range::iterator>;
                           { range.find(key) } -> std::same_as<typename Range::iterator>;
                           {
                             range.find_if([](const Value&) { return true; })
                           } -> std::same_as<typename Range::iterator>;
                         };

} // namespace omni::concepts
