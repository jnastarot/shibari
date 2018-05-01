#include "stdafx.h"
#include "shibari_linker.h"

inline bool NC_string_compare(const std::string& str1, const std::string& str2);
inline bool get_import_func_index(import_table& imports,
    std::string lib_name, std::string funcname,
    uint32_t & lib_idx, uint32_t & func_idx);
inline bool get_import_func_index(import_table& imports,
    std::string lib_name, uint16_t func_ordinal,
    uint32_t & lib_idx, uint32_t & func_idx);


#include "shibari_linker_exceptions.h"
#include "shibari_linker_relocations.h"
#include "shibari_linker_imports.h"
#include "shibari_linker_exports.h"
#include "shibari_linker_tls.h"
#include "shibari_linker_loadconfigs.h"

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

    merge_sections();

    if (!merge_imports()) {
        return shibari_linker_errors::shibari_linker_error_import_linking;
    }
    if (!merge_relocations()) {
        return shibari_linker_errors::shibari_linker_error_relocation_linking;
    }
    if (!merge_exports()) {
        return shibari_linker_errors::shibari_linker_error_export_linking;
    }
    if (!merge_tls()) {
        return shibari_linker_errors::shibari_linker_error_tls_linking;
    }
    if (!merge_loadconfig()) {
        return shibari_linker_errors::shibari_linker_error_loadconfig_linking;
    }
    if (!merge_exceptions()) {
        return shibari_linker_errors::shibari_linker_error_exceptions_linking;
    }

    merge_module_data();

    for (uint32_t dir_idx = 0; dir_idx < 16; dir_idx++) {
        main_module->get_image().set_directory_virtual_address(dir_idx, 0);
        main_module->get_image().set_directory_virtual_size(dir_idx, 0);
    }

    return shibari_linker_errors::shibari_linker_ok;
}


bool shibari_linker::explore_module(shibari_module * module) {

    if (!module) { return false; }

    switch (module->get_module_code()) {

    case shibari_module_code::shibari_module_incorrect:
    case shibari_module_code::shibari_module_initialize_failed: {
        return false;
    }

    case shibari_module_code::shibari_module_initialize_success: {
        return true;
    }

    default: {break; }
    }


    std::vector<directory_placement> placement;
    directory_code code = get_directories_placement(module->get_image(), placement, &module->get_image_bound_imports());
    if (code != directory_code::directory_code_success) { return false; }

    erase_directories_placement(module->get_image(), placement, &module->get_image_relocations(), true);


    if (module->get_image_delay_imports().get_libraries().size()) {//merge delay import
        for (auto &item : module->get_image_delay_imports().get_libraries()) {
            module->get_image_imports().add_library(item.convert_to_imported_library());
        }
    }

    if (!process_relocations(module->get_module_expanded())) { return false; }
    if (!process_import(module->get_module_expanded())) { return false; }


    for (auto& section_ : module->get_image().get_sections()) {
        if (section_->get_virtual_size() < section_->get_size_of_raw_data()) {
            section_->set_virtual_size(section_->get_size_of_raw_data());
        }
    }

    if (module->get_image_exports().get_items().size()) {
        for (auto& export_func : module->get_image_exports().get_items()) {
            module->get_module_exports().add_export(export_func);
        }

        module->get_module_exports().add_name(module->get_image_exports().get_library_name());
    }

    module->set_module_code(shibari_module_initialize_success);

    return true; 
}


void shibari_linker::merge_sections() {
    for (auto& module_ : extended_modules) {

        module_->get_module_position().set_current_position(
            ALIGN_UP(main_module->get_image().get_sections()[
                main_module->get_image().get_sections_number() - 1
            ]->get_virtual_address() + main_module->get_image().get_sections()[
                main_module->get_image().get_sections_number() - 1]->get_virtual_size(), main_module->get_image().get_section_align())
        );

        module_->get_module_position().set_address_offset(
            module_->get_module_position().get_current_position() - module_->get_image().get_sections()[0]->get_virtual_address()
        );

        uint32_t section_top_rva = module_->get_module_position().get_current_position();
        uint32_t section_top_raw = ALIGN_UP(main_module->get_image().get_sections()[
            main_module->get_image().get_sections_number() - 1
        ]->get_pointer_to_raw() + main_module->get_image().get_sections()[
            main_module->get_image().get_sections_number() - 1]->get_size_of_raw_data(), 
                main_module->get_image().get_section_align());

        for (auto& section_ : module_->get_image().get_sections()) { //add sections
            pe_section pre_section = *section_;
            pre_section.set_virtual_address(section_top_rva);
            pre_section.set_pointer_to_raw(section_top_raw);
            main_module->get_image().add_section(pre_section);

            section_top_raw += ALIGN_UP(pre_section.get_pointer_to_raw() + pre_section.get_size_of_raw_data(), 
                main_module->get_image().get_section_align());

            section_top_rva += ALIGN_UP(pre_section.get_virtual_size(), 
                main_module->get_image().get_section_align());
        }
    }
}



void shibari_linker::merge_module_data() {

    for (auto& module_ : extended_modules) {
        shibari_module_export& module_export = module_->get_module_exports();

        if (module_export.get_exports_number()) {
            for (auto& exp_item : module_export.get_export_items()) {
                export_table_item item = exp_item;
                item.set_rva(exp_item.get_rva() + module_->get_module_position().get_address_offset());

                main_module->get_module_exports().add_export(item);
            }

            for (auto& exp_name : module_export.get_names()) {
                main_module->get_module_exports().add_name(exp_name);
            }         
        }


        for (auto& code_symbol : module_->get_code_symbols()) {
            main_module->get_code_symbols().push_back({
                code_symbol.symbol_info_rva + module_->get_module_position().get_address_offset() ,
                code_symbol.symbol_info_size
            });
        }

        for (auto& data_symbol : module_->get_data_symbols()) {
            main_module->get_data_symbols().push_back({
                data_symbol.symbol_info_rva + module_->get_module_position().get_address_offset() ,
                data_symbol.symbol_info_size
            });
        }

        if (module_->get_image().get_characteristics()&IMAGE_FILE_DLL) {
            main_module->get_module_entrys().push_back({ shibari_entry_point_dll,
                module_->get_module_position().get_address_offset() + module_->get_image().get_entry_point()
            });

            //fix it
            main_module->get_image_tls().set_address_of_index(1);
            main_module->get_image_tls().get_callbacks().push_back({ 
                module_->get_image().get_entry_point() + module_->get_module_position().get_address_offset(), true 
            });

        }
        else {
            main_module->get_module_entrys().push_back({ shibari_entry_point_exe,
                module_->get_module_position().get_address_offset() + module_->get_image().get_entry_point()
            });
        }      
    }
}






inline bool NC_string_compare(const std::string& str1, const std::string& str2) {
    if (str2.size() != str1.size()) { return false; }
    for (size_t i = 0; i < str1.size(); ++i) {
        if (tolower(str1[i]) != tolower(str2[i])) {
            return false;
        }
    }
    return true;
}

inline bool get_import_func_index(import_table& imports,
    std::string lib_name, std::string funcname,
    uint32_t & lib_idx, uint32_t & func_idx) {


    for (unsigned int current_lib_idx = 0; current_lib_idx < imports.get_libraries().size(); current_lib_idx++) {
        imported_library & current_library = imports.get_libraries()[current_lib_idx];

        if (NC_string_compare(current_library.get_library_name(), lib_name)) {

            for (unsigned int current_func_idx = 0; current_func_idx < current_library.get_items().size(); current_func_idx++) {
                imported_func& current_func = current_library.get_items()[current_func_idx];

                if (current_func.is_import_by_name() && current_func.get_func_name() == funcname) {

                    lib_idx = current_lib_idx;
                    func_idx = current_func_idx;
                    return true;
                }
            }
        }

    }

    return false;
}

inline bool get_import_func_index(import_table& imports,
    std::string lib_name, uint16_t func_ordinal,
    uint32_t & lib_idx, uint32_t & func_idx) {

    for (unsigned int current_lib_idx = 0; current_lib_idx < imports.get_libraries().size(); current_lib_idx++) {
        imported_library & current_library = imports.get_libraries()[current_lib_idx];

        if (NC_string_compare(current_library.get_library_name(), lib_name)) {

            for (unsigned int current_func_idx = 0; current_func_idx < current_library.get_items().size(); current_func_idx++) {
                imported_func& current_func = current_library.get_items()[current_func_idx];

                if (!current_func.is_import_by_name() && current_func.get_ordinal() == func_ordinal) {

                    lib_idx = current_lib_idx;
                    func_idx = current_func_idx;
                    return true;
                }
            }
        }

    }

    return false;
}
