#include "stdafx.h"
#include "shibari_exceptions_linker.h"


shibari_exceptions_linker::shibari_exceptions_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module)
    : extended_modules(extended_modules), main_module(main_module) {}


shibari_exceptions_linker::~shibari_exceptions_linker()
{
}


shibari_linker_errors shibari_exceptions_linker::link_modules() {
    if (main_module->get_image().is_x32_image()) { return shibari_linker_errors::shibari_linker_error_bad_input; }

    for (auto& module : *extended_modules) {

        for (auto& item : module->get_image_exceptions().get_items()) {

            main_module->get_image_exceptions().add_item(
                item.get_begin_address() + module->get_module_position().get_address_offset(),
                item.get_end_address() + module->get_module_position().get_address_offset(),
                item.get_unwind_data_address() + module->get_module_position().get_address_offset()
            );
        }
    }

    return shibari_linker_errors::shibari_linker_ok;
}