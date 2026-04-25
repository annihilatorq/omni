#pragma once

#include <cstdint>

#include "omni/win/file_header.hpp"
#include "omni/win/nt_headers.hpp"

namespace omni::win {

  struct dos_header {
    std::uint16_t e_magic;
    std::uint16_t e_cblp;
    std::uint16_t e_cp;
    std::uint16_t e_crlc;
    std::uint16_t e_cparhdr;
    std::uint16_t e_minalloc;
    std::uint16_t e_maxalloc;
    std::uint16_t e_ss;
    std::uint16_t e_sp;
    std::uint16_t e_csum;
    std::uint16_t e_ip;
    std::uint16_t e_cs;
    std::uint16_t e_lfarlc;
    std::uint16_t e_ovno;
    std::uint16_t e_res[4];
    std::uint16_t e_oemid;
    std::uint16_t e_oeminfo;
    std::uint16_t e_res2[10];
    std::uint32_t e_lfanew;

    [[nodiscard]] file_header* get_file_header() {
      return &get_nt_headers()->file_header;
    }

    [[nodiscard]] const file_header* get_file_header() const {
      return &get_nt_headers()->file_header;
    }

    [[nodiscard]] nt_headers* get_nt_headers() {
      return reinterpret_cast<nt_headers*>(reinterpret_cast<std::byte*>(this) + e_lfanew);
    }

    [[nodiscard]] const nt_headers* get_nt_headers() const {
      return reinterpret_cast<const nt_headers*>(reinterpret_cast<const std::byte*>(this) + e_lfanew);
    }
  };

} // namespace omni::win
