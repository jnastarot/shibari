#pragma once

bool shibari_linker::merge_tls() {

    for (auto& module : extended_modules) {

        if (module->get_image_tls().get_callbacks().size()) {

            for (auto& item : module->get_image_tls().get_callbacks()) {
                main_module->get_image_tls().get_callbacks().push_back({
                    module->get_module_position().get_address_offset() + item.rva_callback ,item.use_relocation             
                });
            }
        }
    }

    return true;
}