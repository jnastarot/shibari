#include "stdafx.h"
#include "shibari_linker.h"

shibari_linker::shibari_linker(std::vector<shibari_module*>& extended_modules, shibari_module* main_module){
    this->extended_modules = extended_modules;
    this->main_module = main_module;
}


shibari_linker::~shibari_linker(){

}


shibari_linker_errors shibari_linker::link_modules() {

    if (!main_module) {
        return shibari_linker_errors::shibari_linker_error_bad_input;
    }
    else {
        if (!explore_module(main_module)) {
            return shibari_linker_errors::shibari_linker_error_bad_input;
        }
    }

    for (auto &module : extended_modules) {
        if (!module || main_module->get_image().is_x32_image() != module->get_image().is_x32_image() || !explore_module(module)) {
            return shibari_linker_errors::shibari_linker_error_bad_input;
        }
    }

    shibari_data_linker        linker_data(&extended_modules, main_module);
    shibari_import_linker      linker_imports(&extended_modules, main_module);
    shibari_export_linker      linker_exports(&extended_modules, main_module);
    shibari_relocations_linker linker_relocations(&extended_modules, main_module);
    shibari_loadconfigs_linker linker_loadconfigs(&extended_modules, main_module);
    shibari_tls_linker         linker_tls(&extended_modules, main_module);
    shibari_exceptions_linker  linker_exceptions(&extended_modules, main_module);


    shibari_linker_errors result = shibari_linker_errors::shibari_linker_ok;

    result = linker_data.link_modules_start();
    if (result != shibari_linker_errors::shibari_linker_ok) { return result; }

    result = linker_imports.link_modules();
    if (result != shibari_linker_errors::shibari_linker_ok) { return result; }

    result = linker_relocations.link_modules();
    if (result != shibari_linker_errors::shibari_linker_ok) { return result; }

    result = linker_exports.link_modules();
    if (result != shibari_linker_errors::shibari_linker_ok) { return result; }

    result = linker_tls.link_modules();
    if (result != shibari_linker_errors::shibari_linker_ok) { return result; }

    result = linker_loadconfigs.link_modules();
    if (result != shibari_linker_errors::shibari_linker_ok) { return result; }

    if (!main_module->get_image().is_x32_image()) {
        result = linker_exceptions.link_modules();
        if (result != shibari_linker_errors::shibari_linker_ok) { return result; }
    }

    result = linker_data.link_modules_finalize();
    if (result != shibari_linker_errors::shibari_linker_ok) { return result; }


    return shibari_linker_errors::shibari_linker_ok;
}


bool shibari_linker::explore_module(shibari_module * target_module) {

    if (!target_module) { 
        return false; 
    }

    switch (target_module->get_module_code()) {

        case shibari_module_code::shibari_module_incorrect:
        case shibari_module_code::shibari_module_initialization_failed: {
            return false;
        }

        case shibari_module_code::shibari_module_initialization_success: {
            return true;
        }

        default: {
            break; 
        }

    }

    

    pe_directory_placement placement;
    directory_code code = get_directories_placement(target_module->get_image(), placement, &target_module->get_image_bound_imports());
    get_extended_exception_info_placement(target_module->get_module_expanded(), placement);


    if (code != directory_code::directory_code_success) { 
        return false; 
    }

    erase_directories_placement(target_module->get_image(), placement, &target_module->get_image_relocations(), true);

    if (target_module->get_image_delay_imports().get_libraries().size()) {//merge delay import
        for (auto &item : target_module->get_image_delay_imports().get_libraries()) {
            target_module->get_image_imports().add_library(item.convert_to_imported_library());
        }
    }

    if (!shibari_relocations_linker::process_relocations(*target_module)) {
        return false; 
    }

    if (!shibari_import_linker::process_import(*target_module)) {
        return false; 
    }


    for (auto& section_ : target_module->get_image().get_sections()) {
        if (section_->get_virtual_size() < section_->get_size_of_raw_data()) {
            section_->set_virtual_size(section_->get_size_of_raw_data());
        }
    }

    if (target_module->get_image_exports().get_items().size()) {
        for (auto& export_func : target_module->get_image_exports().get_items()) {
            target_module->get_module_exports().add_export(export_func);
        }

        target_module->get_module_exports().add_name(target_module->get_image_exports().get_library_name());
    }

    target_module->set_module_code(shibari_module_initialization_success);

    return true; 
}
