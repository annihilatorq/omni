#pragma once

#include "omni/api_set.hpp"
#include "omni/api_sets.hpp"
#include "omni/hash.hpp"
#include "omni/module_export.hpp"
#include "omni/modules.hpp"

namespace omni {

  namespace detail {

    template <concepts::hash Hasher>
    inline module_export resolve_forwarded_export(omni::address export_address) {
      // Learn more here: https://devblogs.microsoft.com/oldnewthing/20060719-24/?p=30473
      // In a forwarded export, the address is a string containing
      // information about the actual export and its location
      // They are always presented as "module_name.export_name"
      const auto* forwarder_str = export_address.ptr<const char>();

      // Split forwarded export to module name and real export name
      forwarder_string forwarder{forwarder_string::parse(forwarder_str)};
      if (forwarder.function.empty()) {
        return {};
      }

      const auto resolve_export_in_module = [&forwarder](Hasher module_name_hash) -> module_export {
        if (forwarder.is_ordinal()) {
          return omni::get_export<Hasher>(forwarder.to_ordinal(), module_name_hash, omni::use_ordinal);
        }
        return omni::get_export<Hasher>(omni::hash<Hasher>(forwarder.function), module_name_hash);
      };

      // Try to search for the real export with a pre-known module name
      module_export real_export = resolve_export_in_module(omni::hash<Hasher>(forwarder.module));
      if (real_export.present()) {
        return real_export;
      }

      // Direct lookup by forwarder target failed, try api-set resolution next
      module_export forwarded_export{
        .is_forwarded = true,
        .forwarder_string = forwarder,
      };

      // We cannot know whether a module is an API-set. The only way
      // to find out is to scan the names of API-set contracts to
      // see if its name is among them
      auto api_set = omni::get_api_set(omni::hash<Hasher>(forwarder.module));
      if (!api_set.present()) {
        return forwarded_export;
      }

      // At this stage, we know that the module pointed to by the forwarder
      // is an API set. We will iterate through its list of hosts to find
      // any loaded module, in which we will attempt to locate the actual
      // export name given by forwarder
      for (const omni::api_set_host& host : api_set.hosts()) {
        bool host_name_empty = not host.present() or host.value.empty();
        if (host_name_empty) {
          continue;
        }

        auto module_name_hash = omni::hash<Hasher>(host.value);
        module_export host_export = resolve_export_in_module(module_name_hash);
        if (host_export.present()) {
          return host_export;
        }
      }

      // The API-set host specified by the forwarder has not been loaded
      // into the process. We've done everything we can, so we return
      // the API-set that we couldn't resolve to the caller
      forwarded_export.forwarder_api_set = api_set;

      return forwarded_export;
    }

  } // namespace detail

  inline omni::module base_module() {
    return *omni::modules{}.begin();
  }

  inline omni::module get_module(concepts::hash auto module_name) {
    omni::modules modules{};
    auto it = modules.find(module_name);
    if (it == modules.end()) {
      return {};
    }

    return *it;
  }

  inline omni::module get_module(default_hash module_name) {
    return get_module<default_hash>(module_name);
  }

  inline omni::module get_module(omni::address base_address) {
    omni::modules modules{};
    auto it = modules.find(base_address);
    if (it == modules.end()) {
      return {};
    }

    return *it;
  }

  inline module_export get_export(concepts::hash auto export_name, omni::module module) {
    if (!module.present()) {
      return {};
    }

    auto exports = module.exports();
    auto export_it = exports.find(export_name);
    if (export_it == exports.end()) {
      // The named export that the caller is looking for was
      // not found in the expected module
      return {};
    }

    if (export_it->is_forwarded) {
      return detail::resolve_forwarded_export<decltype(export_name)>(export_it->address);
    }

    return *export_it;
  }

  inline module_export get_export(default_hash export_name, omni::module module) {
    return omni::get_export<default_hash>(export_name, module);
  }

  template <concepts::hash Hasher>
  inline module_export get_export(Hasher export_name, Hasher module_name) {
    return omni::get_export<Hasher>(export_name, omni::get_module(module_name));
  }

  inline module_export get_export(default_hash export_name, default_hash module_name) {
    return omni::get_export<default_hash>(export_name, omni::get_module(module_name));
  }

  inline module_export get_export(concepts::hash auto export_name) {
    for (const omni::module& module : omni::modules{}) {
      if (auto module_export = omni::get_export(export_name, module); module_export) {
        return module_export;
      }
    }
    return {};
  }

  inline module_export get_export(default_hash export_name) {
    return omni::get_export<default_hash>(export_name);
  }

  template <concepts::hash Hasher>
  inline module_export get_export(std::uint32_t ordinal, omni::module module, omni::use_ordinal_t) {
    if (!module.present()) {
      return {};
    }

    auto exports = module.exports();
    auto export_it = exports.find(ordinal, omni::use_ordinal);
    if (export_it == exports.end()) {
      // The ordinal export that the caller is looking for was
      // not found in the expected module
      return {};
    }

    if (export_it->is_forwarded) {
      return detail::resolve_forwarded_export<Hasher>(export_it->address);
    }

    return *export_it;
  }

  inline module_export get_export(std::uint32_t ordinal, omni::module module, omni::use_ordinal_t) {
    return omni::get_export<default_hash>(ordinal, module, omni::use_ordinal);
  }

  inline module_export get_export(std::uint32_t ordinal, concepts::hash auto module_name, omni::use_ordinal_t) {
    return omni::get_export<decltype(module_name)>(ordinal, omni::get_module(module_name), omni::use_ordinal);
  }

  inline module_export get_export(std::uint32_t ordinal, default_hash module_name, omni::use_ordinal_t) {
    return omni::get_export<default_hash>(ordinal, module_name, omni::use_ordinal);
  }

} // namespace omni
