#include <Windows.h>

#include <optional>
#include <string_view>

#include "omni/api_sets.hpp"
#include "omni/modules.hpp"
#include "test_utils.hpp"

namespace tests = omni::tests;

namespace {

  struct forwarded_export {
    omni::module module;
    tests::manual_export_info export_entry{};
    omni::forwarder_string forwarder{};
    FARPROC resolved_address{};
  };

  template <std::predicate<const omni::forwarder_string&> Pred>
  [[nodiscard]] std::optional<forwarded_export> find_forwarded_export(Pred&& predicate) {
    for (const omni::module& module : omni::modules{}) {
      auto module_handle = static_cast<HMODULE>(module.native_handle());
      auto export_entries = tests::get_export_table_entries(module_handle);

      for (const tests::manual_export_info& export_entry : export_entries) {
        if (!export_entry.is_forwarded || export_entry.name.empty()) {
          continue;
        }

        auto raw_forwarder_string = std::string_view{export_entry.address.ptr<const char>()};
        auto forwarder = omni::forwarder_string::parse(raw_forwarder_string);
        if (!forwarder.present() || !predicate(forwarder)) {
          continue;
        }

        FARPROC resolved_address = ::GetProcAddress(module_handle, export_entry.name.data());
        if (resolved_address == nullptr) {
          continue;
        }

        return forwarded_export{
          .module = module,
          .export_entry = export_entry,
          .forwarder = forwarder,
          .resolved_address = resolved_address,
        };
      }
    }

    return std::nullopt;
  }

  [[nodiscard]] bool targets_api_set(const omni::forwarder_string& forwarder) {
    return omni::get_api_set(omni::hash<omni::default_hash>(forwarder.module)).present();
  }

} // namespace

ut::suite<"omni::get_export"> get_export_suite = [] {
  "name lookup resolves forwarded exports that target regular modules"_test = [] {
    auto forwarded_export = find_forwarded_export(std::not_fn(targets_api_set));

    expect(fatal(forwarded_export.has_value()));

    omni::default_hash export_name_hash{omni::hash<omni::default_hash>(forwarded_export->export_entry.name)};
    omni::default_hash module_name_hash{omni::hash<omni::default_hash>(forwarded_export->module.wname())};

    auto exports = forwarded_export->module.exports();
    auto raw_export = exports.find(export_name_hash);
    auto resolved_by_module = omni::get_export(export_name_hash, forwarded_export->module);
    auto resolved_by_module_name = omni::get_export(export_name_hash, module_name_hash);

    expect(fatal(raw_export != exports.end()));
    expect(raw_export->is_forwarded);
    expect(raw_export->forwarder_string.present());
    expect(raw_export->forwarder_string.module == forwarded_export->forwarder.module);
    expect(raw_export->forwarder_string.function == forwarded_export->forwarder.function);

    expect(resolved_by_module.present());
    expect(resolved_by_module_name.present());
    expect(not resolved_by_module.is_forwarded);
    expect(not resolved_by_module_name.is_forwarded);
    expect(resolved_by_module.address == forwarded_export->resolved_address);
    expect(resolved_by_module_name.address == forwarded_export->resolved_address);
    expect(resolved_by_module.address != raw_export->address);
    expect(resolved_by_module_name.address != raw_export->address);
    expect(resolved_by_module.module_base != forwarded_export->module.base_address());
    expect(resolved_by_module_name.module_base != forwarded_export->module.base_address());
    expect(not resolved_by_module.forwarder_string.present());
    expect(not resolved_by_module_name.forwarder_string.present());
    expect(not resolved_by_module.forwarder_api_set.has_value());
    expect(not resolved_by_module_name.forwarder_api_set.has_value());
  };

  "name and ordinal lookup resolve forwarded exports that target api sets"_test = [] {
    auto forwarded_export = find_forwarded_export(targets_api_set);

    expect(fatal(forwarded_export.has_value()));

    omni::default_hash export_name_hash{omni::hash<omni::default_hash>(forwarded_export->export_entry.name)};
    omni::default_hash module_name_hash{omni::hash<omni::default_hash>(forwarded_export->module.wname())};
    auto forwarder_api_set = omni::get_api_set(omni::hash<omni::default_hash>(forwarded_export->forwarder.module));

    auto exports = forwarded_export->module.exports();
    auto raw_by_name = exports.find(export_name_hash);
    auto raw_by_ordinal = exports.find(forwarded_export->export_entry.ordinal, omni::use_ordinal);
    auto resolved_by_name = omni::get_export(export_name_hash, forwarded_export->module);
    auto resolved_by_module_name = omni::get_export(export_name_hash, module_name_hash);
    auto resolved_by_ordinal =
      omni::get_export(forwarded_export->export_entry.ordinal, forwarded_export->module, omni::use_ordinal);
    auto resolved_by_module_name_ordinal =
      omni::get_export(forwarded_export->export_entry.ordinal, module_name_hash, omni::use_ordinal);

    expect(fatal(forwarder_api_set.present()));
    expect(fatal(raw_by_name != exports.end()));
    expect(fatal(raw_by_ordinal != exports.end()));
    expect(raw_by_name->is_forwarded);
    expect(raw_by_ordinal->is_forwarded);
    expect(raw_by_name->forwarder_string.present());
    expect(raw_by_ordinal->forwarder_string.present());
    expect(raw_by_name->forwarder_string.module == forwarded_export->forwarder.module);
    expect(raw_by_ordinal->forwarder_string.module == forwarded_export->forwarder.module);

    expect(resolved_by_name.present());
    expect(resolved_by_module_name.present());
    expect(resolved_by_ordinal.present());
    expect(resolved_by_module_name_ordinal.present());

    expect(not resolved_by_name.is_forwarded);
    expect(not resolved_by_module_name.is_forwarded);
    expect(not resolved_by_ordinal.is_forwarded);
    expect(not resolved_by_module_name_ordinal.is_forwarded);

    expect(resolved_by_name.address == forwarded_export->resolved_address);
    expect(resolved_by_module_name.address == forwarded_export->resolved_address);
    expect(resolved_by_ordinal.address == forwarded_export->resolved_address);
    expect(resolved_by_module_name_ordinal.address == forwarded_export->resolved_address);

    expect(resolved_by_name.address != raw_by_name->address);
    expect(resolved_by_ordinal.address != raw_by_ordinal->address);
    expect(resolved_by_name.module_base != forwarded_export->module.base_address());
    expect(resolved_by_ordinal.module_base != forwarded_export->module.base_address());
    expect(not resolved_by_name.forwarder_string.present());
    expect(not resolved_by_ordinal.forwarder_string.present());
    expect(not resolved_by_name.forwarder_api_set.has_value());
    expect(not resolved_by_ordinal.forwarder_api_set.has_value());
  };
};
