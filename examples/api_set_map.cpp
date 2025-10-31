#include "shadowsyscall.hpp"

template <class... Types>
void debug_log(const std::wformat_string<Types...> fmt, Types... args) {
  std::wcout << std::format(fmt, std::forward<Types>(args)...) << "\n";
}

int main() {
  auto string_appender = [](std::wstring acc, auto host) {
    auto [value, alias] = host;
    auto separator = acc.empty() ? L"" : L", ";
    return acc.append(separator).append(value);
  };

  for (auto& entry : shadow::api_set_map_view()) {
    auto hosts = std::accumulate(entry.hosts().begin(), entry.hosts().end(), std::wstring{},
                                 string_appender);

    if (hosts.empty())
      hosts.assign(L"none");

    debug_log(L"{} -> {}", entry.contract().clean_name(), hosts);
  }
}
