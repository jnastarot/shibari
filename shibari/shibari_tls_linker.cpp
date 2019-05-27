#include "stdafx.h"
#include "shibari_tls_linker.h"


shibari_tls_linker::shibari_tls_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module)
    : extended_modules(extended_modules), main_module(main_module) {}


shibari_tls_linker::~shibari_tls_linker()
{
}


shibari_linker_errors shibari_tls_linker::link_modules() {

    for (auto& module : *extended_modules) {

        if (module->get_module_image().get_tls().get_callbacks().size()) {

            for (auto& item : module->get_module_image().get_tls().get_callbacks()) {
                main_module->get_module_image().get_tls().get_callbacks().push_back({
                    module->get_module_position().get_address_offset() + item.rva_callback ,item.use_relocation
                    });
            }
        }
    }

    return shibari_linker_errors::shibari_linker_ok;
}