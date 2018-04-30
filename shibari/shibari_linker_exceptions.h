#pragma once

bool shibari_linker::merge_exceptions() {
    if (main_module->get_image().is_x32_image()) { return true; }

    for (auto& module : extended_modules) {

        for (auto& item : module->get_image_exceptions().get_items()) {

            main_module->get_image_exceptions().add_item(
                item.get_begin_address()    + module->get_module_position().get_address_offset(),
                item.get_end_address()      + module->get_module_position().get_address_offset(),
                item.get_unwind_data_address() + module->get_module_position().get_address_offset()
            );
        }
    }

    return true;
}