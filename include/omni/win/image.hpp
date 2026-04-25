#pragma once

#include <cstdint>
#include <limits>
#include <memory>

#include "omni/win/directories.hpp"
#include "omni/win/dos_header.hpp"
#include "omni/win/nt_headers.hpp"

namespace omni::win {

  struct image {
    constexpr static auto npos{(std::numeric_limits<std::uint32_t>::max)()};

    win::dos_header dos_header;

    [[nodiscard]] win::dos_header* get_dos_headers() {
      return &dos_header;
    }

    [[nodiscard]] const win::dos_header* get_dos_headers() const {
      return &dos_header;
    }

    [[nodiscard]] file_header* get_file_header() {
      return dos_header.get_file_header();
    }

    [[nodiscard]] const file_header* get_file_header() const {
      return dos_header.get_file_header();
    }

    [[nodiscard]] nt_headers* get_nt_headers() {
      return dos_header.get_nt_headers();
    }

    [[nodiscard]] const nt_headers* get_nt_headers() const {
      return dos_header.get_nt_headers();
    }

    [[nodiscard]] optional_header* get_optional_header() {
      return &get_nt_headers()->optional_header;
    }

    [[nodiscard]] const optional_header* get_optional_header() const {
      return &get_nt_headers()->optional_header;
    }

    [[nodiscard]] data_directory* get_directory(directory_id id) {
      nt_headers* nt_hdrs = get_nt_headers();
      if (nt_hdrs->optional_header.num_data_directories <= id) {
        return nullptr;
      }
      data_directory* dir = &nt_hdrs->optional_header.data_directories.entries[id];
      return dir->present() ? dir : nullptr;
    }

    [[nodiscard]] const data_directory* get_directory(directory_id id) const {
      const nt_headers* nt_hdrs = get_nt_headers();
      if (nt_hdrs->optional_header.num_data_directories <= id) {
        return nullptr;
      }
      const data_directory* dir = &nt_hdrs->optional_header.data_directories.entries[id];
      return dir->present() ? dir : nullptr;
    }

    template <typename T = std::byte>
    [[nodiscard]] T* rva_to_ptr(std::uint32_t rva, std::size_t length = 1) {
      // Find the section, try mapping to header if none found
      section_header* scn = rva_to_section(rva);
      if (!scn) {
        std::uint32_t rva_hdr_end = get_nt_headers()->optional_header.size_headers;
        if (rva < rva_hdr_end && (rva + length) <= rva_hdr_end) {
          return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(&dos_header) + rva);
        }
        return nullptr;
      }

      // Apply the boundary check
      std::size_t offset = rva - scn->virtual_address;
      if ((offset + length) > scn->size_raw_data) {
        return nullptr;
      }

      return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(&dos_header) + scn->ptr_raw_data + offset);
    }

    template <typename T = std::byte>
    [[nodiscard]] const T* rva_to_ptr(std::uint32_t rva, std::size_t length = 1) const {
      // Find the section, try mapping to header if none found
      const section_header* scn = rva_to_section(rva);
      if (!scn) {
        std::uint32_t rva_hdr_end = get_nt_headers()->optional_header.size_headers;
        if (rva < rva_hdr_end && (rva + length) <= rva_hdr_end) {
          return reinterpret_cast<const T*>(reinterpret_cast<const std::byte*>(&dos_header) + rva);
        }
        return nullptr;
      }

      // Apply the boundary check
      std::size_t offset = rva - scn->virtual_address;
      if ((offset + length) > scn->size_raw_data) {
        return nullptr;
      }

      return reinterpret_cast<const T*>(reinterpret_cast<const std::byte*>(&dos_header) + scn->ptr_raw_data + offset);
    }

    [[nodiscard]] section_header* rva_to_section(std::uint32_t rva) {
      for (section_header& section : get_nt_headers()->sections()) {
        if (section.virtual_address <= rva && rva < (section.virtual_address + section.virtual_size)) {
          return std::addressof(section);
        }
      }
      return nullptr;
    }

    [[nodiscard]] const section_header* rva_to_section(std::uint32_t rva) const {
      for (const section_header& section : get_nt_headers()->sections()) {
        if (section.virtual_address <= rva && rva < (section.virtual_address + section.virtual_size)) {
          return std::addressof(section);
        }
      }
      return nullptr;
    }

    [[nodiscard]] std::uint32_t ptr_to_raw(const void* ptr) const {
      if (ptr == nullptr) {
        return npos;
      }

      return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ptr) - reinterpret_cast<std::uintptr_t>(&dos_header));
    }

    [[nodiscard]] std::uint32_t rva_to_fo(std::uint32_t rva, std::size_t length = 1) const {
      return ptr_to_raw(rva_to_ptr(rva, length));
    }
  };

} // namespace omni::win
