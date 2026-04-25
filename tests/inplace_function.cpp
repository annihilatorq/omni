#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include "omni/detail/config.hpp"
#include "omni/detail/inplace_function.hpp"
#include "test_utils.hpp"

namespace {

  using omni::detail::inplace_function;

  static_assert(std::is_default_constructible_v<inplace_function<int(int)>>);
  static_assert(std::copy_constructible<inplace_function<int(int)>>);
  static_assert(std::move_constructible<inplace_function<int(int)>>);

  struct move_only_functor {
    move_only_functor() = default;
    move_only_functor(const move_only_functor&) = delete;
    move_only_functor(move_only_functor&&) = default;
    move_only_functor& operator=(const move_only_functor&) = delete;
    move_only_functor& operator=(move_only_functor&&) = default;

    [[nodiscard]] int operator()(int value) const noexcept {
      return value;
    }
  };

  static_assert(!std::is_constructible_v<inplace_function<int(int)>, move_only_functor>);

  int add(int left, int right) {
    return left + right;
  }

  struct lifecycle_functor {
    int* destroy_count{};
    int multiplier{};

    [[nodiscard]] int operator()(int value) const noexcept {
      return value * multiplier;
    }

    ~lifecycle_functor() {
      if (destroy_count != nullptr) {
        ++*destroy_count;
      }
    }
  };

  struct counter_functor {
    int value{};

    [[nodiscard]] int operator()() noexcept {
      return ++value;
    }
  };

} // namespace

ut::suite<"omni::detail::inplace_function"> inplace_function_suite = [] {
  "invokes free functions and forwards arguments"_test = [] {
    inplace_function<int(int, int)> function{add};

    expect(function(40, 2) == 42);
    expect(function(-10, 3) == -7);
  };

  "mutable lambdas keep their internal state between calls"_test = [] {
    inplace_function<int()> function{[count = 0]() mutable { return ++count; }};

    expect(function() == 1);
    expect(function() == 2);
    expect(function() == 3);
  };

  "void callables can mutate arguments"_test = [] {
    inplace_function<void(int&)> function{[](int& value) { value += 7; }};
    int value = 35;

    function(value);

    expect(value == 42);
  };

  "reference return types propagate the original object"_test = [] {
    int value = 41;
    inplace_function<int&()> function{[&value]() -> int& { return value; }};

    function() = 42;

    expect(value == 42);
    expect(&function() == &value);
  };

  "destroying the wrapper destroys the stored callable"_test = [] {
    int destroy_count = 0;

    {
      lifecycle_functor callable{.destroy_count = &destroy_count, .multiplier = 6};

      {
        inplace_function<int(int)> function{callable};

        expect(function(7) == 42);
        expect(destroy_count == 0);
      }

      expect(destroy_count == 1);
    }

    expect(destroy_count == 2);
  };

  "copy construction copies callable state"_test = [] {
    inplace_function<int()> first{counter_functor{.value = 0}};
    auto second = first;

    expect(first() == 1);
    expect(first() == 2);
    expect(second() == 1);
    expect(second() == 2);
  };

  "copy assignment replaces the stored callable"_test = [] {
    inplace_function<int()> destination{counter_functor{.value = 0}};
    inplace_function<int()> source{counter_functor{.value = 40}};

    expect(destination() == 1);

    destination = source;

    expect(destination() == 41);
    expect(source() == 41);
  };

  "move construction preserves callable behavior"_test = [] {
    inplace_function<int()> source{counter_functor{.value = 41}};
    auto moved = std::move(source);

    expect(moved() == 42);
    expect(moved() == 43);
  };

  "move assignment keeps the new callable invocable"_test = [] {
    inplace_function<int()> destination{counter_functor{.value = 0}};
    inplace_function<int()> source{counter_functor{.value = 41}};

    destination = std::move(source);

    expect(destination() == 42);
    expect(destination() == 43);
  };

  "swap exchanges stored callables"_test = [] {
    inplace_function<int()> left{counter_functor{.value = 0}};
    inplace_function<int()> right{counter_functor{.value = 40}};

    left.swap(right);

    expect(left() == 41);
    expect(right() == 1);
  };

#ifdef OMNI_HAS_EXCEPTIONS

  "empty wrappers throw std::bad_function_call"_test = [] {
    inplace_function<int()> function{};

    expect(throws<std::bad_function_call>([&function] { static_cast<void>(function()); }));
  };

#endif
};
