#include "stdafx.h"
#include "shibari_loadconfigs_linker.h"


shibari_loadconfigs_linker::shibari_loadconfigs_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module)
    : extended_modules(extended_modules), main_module(main_module) {}


shibari_loadconfigs_linker::~shibari_loadconfigs_linker()
{
}

shibari_linker_errors shibari_loadconfigs_linker::link_modules() {

    for (auto & module : *extended_modules) {
        for (auto& se_handler : module->get_image_load_config().get_se_handlers()) {
            main_module->get_image_load_config().get_se_handlers().push_back(se_handler + module->get_module_position().get_address_offset());
        }
        for (auto& lock_prefix_rva : module->get_image_load_config().get_lock_prefixes()) {
            main_module->get_image_load_config().get_lock_prefixes().push_back(lock_prefix_rva + module->get_module_position().get_address_offset());
        }
        for (auto& guard_cf_func_rva : module->get_image_load_config().get_guard_cf_functions()) {
            main_module->get_image_load_config().get_guard_cf_functions().push_back(guard_cf_func_rva + module->get_module_position().get_address_offset());
        }
        for (auto& guard_iat_rva : module->get_image_load_config().get_guard_iat_entries()) {
            main_module->get_image_load_config().get_guard_iat_entries().push_back(guard_iat_rva + module->get_module_position().get_address_offset());
        }
        for (auto& guard_long_jump_rva : module->get_image_load_config().get_guard_long_jump_targets()) {
            main_module->get_image_load_config().get_guard_iat_entries().push_back(guard_long_jump_rva + module->get_module_position().get_address_offset());
        }
    }

    return shibari_linker_errors::shibari_linker_ok;
}