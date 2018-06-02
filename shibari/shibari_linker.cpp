#include "stdafx.h"
#include "shibari_linker.h"

inline bool NC_string_compare(const std::string& str1, const std::string& str2);
inline bool get_import_func_index(import_table& imports,
    std::string lib_name, std::string funcname,
    uint32_t & lib_idx, uint32_t & func_idx);
inline bool get_import_func_index(import_table& imports,
    std::string lib_name, uint16_t func_ordinal,
    uint32_t & lib_idx, uint32_t & func_idx);



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

    if (!process_relocations(module)) { return false; }
    if (!process_import(module)) { return false; }


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


bool shibari_linker::merge_exceptions() {
    if (main_module->get_image().is_x32_image()) { return true; }

    for (auto& module : extended_modules) {

        for (auto& item : module->get_image_exceptions().get_items()) {

            main_module->get_image_exceptions().add_item(
                item.get_begin_address() + module->get_module_position().get_address_offset(),
                item.get_end_address() + module->get_module_position().get_address_offset(),
                item.get_unwind_data_address() + module->get_module_position().get_address_offset()
            );
        }
    }

    return true;
}

bool shibari_linker::get_export_references(std::vector<export_references>& export_refs) {
    import_table imports = main_module->get_image_imports();
    std::vector<shibari_module*> modules_pool;

    modules_pool.push_back(main_module);
    for (auto& module_ : extended_modules) { modules_pool.push_back(module_); }


    for (unsigned int module_idx = 0; module_idx < modules_pool.size(); module_idx++) {
        shibari_module* current_module = modules_pool[module_idx];
        shibari_module_export& module_export = current_module->get_module_exports();

        if (module_export.get_exports_number()) {

            for (unsigned int import_lib_idx = 0; import_lib_idx < imports.get_libraries().size(); import_lib_idx++) {
                imported_library & import_lib = imports.get_libraries()[import_lib_idx];
                std::string current_export_lib_name;

                bool is_lib_name_match = false;

                for (auto& exp_name : module_export.get_names()) {
                    if (NC_string_compare(exp_name, import_lib.get_library_name())) { //check for library name
                        current_export_lib_name = exp_name;
                        is_lib_name_match = true;
                    }
                }

                if (is_lib_name_match) {

                    for (unsigned int import_func_idx = 0; import_func_idx < import_lib.get_items().size(); import_func_idx++) {
                        imported_func& import_func = import_lib.get_items()[import_func_idx];

                        for (auto & export_item : module_export.get_export_items()) {

                            if (import_func.is_import_by_name() == export_item.has_name()) {

                                if (import_func.is_import_by_name()) {

                                    if (import_func.get_func_name() == export_item.get_func_name()) {

                                        export_refs.push_back({
                                            0,
                                            current_export_lib_name,
                                            export_item,
                                            module_idx - 1
                                        });

                                        import_lib.get_items().erase(import_lib.get_items().begin() + import_func_idx);
                                        import_func_idx--;

                                        break;
                                    }
                                }
                                else {

                                    if (import_func.get_ordinal() == export_item.get_name_ordinal()) {
                                        export_refs.push_back({
                                            0,
                                            current_export_lib_name,
                                            export_item,
                                            module_idx - 1
                                        });

                                        import_lib.get_items().erase(import_lib.get_items().begin() + import_func_idx);
                                        import_func_idx--;
                                        break;
                                    }
                                }
                            }
                        }

                    next_main_import_item:;
                    }
                    if (!import_lib.get_items().size()) { //delete import lib if its empty
                        imports.get_libraries().erase(imports.get_libraries().begin() + import_lib_idx);
                        import_lib_idx--;
                    }
                }
            }
        }
    }

    return export_refs.size() != 0;
}



void shibari_linker::initialize_export_redirect_table(std::vector<export_references>& export_refs) {


    pe_section *table_section = main_module->get_image().get_section_by_idx(
        main_module->get_image().get_sections_number() - 1
    );//get last section

    uint32_t redirect_table_rva = table_section->get_virtual_address() + table_section->get_virtual_size();
    pe_image_io redirect_io = pe_image_io(main_module->get_image(), enma_io_mode_allow_expand);
    redirect_io.set_image_offset(redirect_table_rva);

    for (unsigned int i = 0; i < export_refs.size(); i++) { //initialize redirect table

        if (main_module->get_image().is_x32_image()) {
            export_refs[i].wrapper_rva = redirect_io.get_image_offset();

            uint32_t reloc = (uint32_t)main_module->get_image().rva_to_va(export_refs[i].export_item.get_rva());

            if (export_refs[i].module_export_owner != -1) {
                reloc += extended_modules[export_refs[i].module_export_owner]->get_module_position().get_address_offset();
            }


            redirect_io.write(&reloc, sizeof(reloc));
        }
        else {
            export_refs[i].wrapper_rva = redirect_io.get_image_offset();

            uint64_t reloc = main_module->get_image().rva_to_va(export_refs[i].export_item.get_rva());

            if (export_refs[i].module_export_owner != -1) {
                reloc += extended_modules[export_refs[i].module_export_owner]->get_module_position().get_address_offset();
            }

            redirect_io.write(&reloc, sizeof(reloc));
        }

        main_module->get_image_relocations().add_item(export_refs[i].wrapper_rva, relocation_index_default);
    }
}


bool shibari_linker::merge_exports() {

    std::vector<export_references> export_refs;
    if (!get_export_references(export_refs)) { return true; }

    initialize_export_redirect_table(export_refs);       //create table in last section for export redirect


    for (auto& ref : export_refs) {                      //fix relocations for redirected import 
        unsigned int ref_lib_idx;
        unsigned int ref_func_idx;

        if (ref.export_item.has_name()) {
            if (!get_import_func_index(main_module->get_image_imports(),
                ref.lib_name, ref.export_item.get_func_name(),
                ref_lib_idx, ref_func_idx)) {

                return false; // =\ why?
            }
        }
        else {
            if (!get_import_func_index(main_module->get_image_imports(),
                ref.lib_name, ref.export_item.get_ordinal(),
                ref_lib_idx, ref_func_idx)) {

                return false; // =\ why?
            }
        }

        std::vector<relocation_item*> found_relocs;
        main_module->get_image_relocations().get_items_by_relocation_id(found_relocs, SET_RELOCATION_ID_IAT(ref_lib_idx, ref_func_idx));

        for (auto & found_item : found_relocs) {

            if (main_module->get_image().is_x32_image()) {
                uint32_t new_rel = (uint32_t)main_module->get_module_expanded().image.rva_to_va(ref.wrapper_rva);

                pe_image_io(main_module->get_image(), enma_io_mode_allow_expand).set_image_offset(found_item->relative_virtual_address).write(
                    &new_rel, sizeof(new_rel));
            }
            else {
                uint64_t new_rel = main_module->get_module_expanded().image.rva_to_va(ref.wrapper_rva);

                pe_image_io(main_module->get_image(), enma_io_mode_allow_expand).set_image_offset(found_item->relative_virtual_address).write(
                    &new_rel, sizeof(new_rel));
            }

            found_item->relocation_id = relocation_index_default;
        }
    }


    import_table new_import_table = main_module->get_image_imports();
    for (auto& ref : export_refs) {                             //erase items from import by export lib name and export item 

        for (unsigned int import_lib_idx = 0, original_import_lib_idx = 0;
            import_lib_idx < new_import_table.get_libraries().size();
            import_lib_idx++, original_import_lib_idx++) {

            auto & import_lib = new_import_table.get_libraries()[import_lib_idx];

            if (NC_string_compare(ref.lib_name, import_lib.get_library_name())) {

                for (unsigned int import_func_idx = 0, original_import_func_idx = 0;
                    import_func_idx < import_lib.get_items().size();
                    import_func_idx++, original_import_func_idx++) {

                    auto & import_func = import_lib.get_items()[import_func_idx];

                    if (ref.export_item.has_name()) {
                        if (import_func.is_import_by_name() && import_func.get_func_name() == ref.export_item.get_func_name()) {
                            import_lib.get_items().erase(import_lib.get_items().begin() + import_func_idx);
                            import_func_idx--;
                        }
                        else {
                            continue;
                        }
                    }
                    else {
                        if (!import_func.is_import_by_name() && import_func.get_ordinal() == ref.export_item.get_ordinal()) {
                            import_lib.get_items().erase(import_lib.get_items().begin() + import_func_idx);
                            import_func_idx--;
                        }
                        else {
                            continue;
                        }
                    }
                }

                if (!import_lib.get_items().size()) {                    //delete if lib empty
                    new_import_table.get_libraries().erase(new_import_table.get_libraries().begin() + import_lib_idx);
                    import_lib_idx--;
                }
            }
        }
    }

    switch_import_refs(main_module->get_module_expanded(), new_import_table);

    return true;
}

bool shibari_linker::switch_import_refs(pe_image_expanded& expanded_image, import_table& new_import_table) {
    //rebuild relocations refs to import and switch import to new_import_table
    for (unsigned int reloc_idx = 0; reloc_idx < expanded_image.relocations.size(); reloc_idx++) {
        relocation_item & reloc_item = expanded_image.relocations.get_items()[reloc_idx];

        if (reloc_item.relocation_id & relocation_index_iat_address) {

            unsigned int reloc_lib_idx = GET_HI_NUMBER(reloc_item.relocation_id);
            unsigned int reloc_func_idx = GET_LO_NUMBER(reloc_item.relocation_id);

            imported_library& reloc_ref_lib = expanded_image.imports.get_libraries()[reloc_lib_idx];
            imported_func& reloc_ref_func = reloc_ref_lib.get_items()[reloc_func_idx];


            //handle refs to import
            for (unsigned int new_import_lib_idx = 0; new_import_lib_idx < new_import_table.size(); new_import_lib_idx++) {
                imported_library& new_import_lib = new_import_table.get_libraries()[new_import_lib_idx];

                if (NC_string_compare(new_import_lib.get_library_name(), reloc_ref_lib.get_library_name())) {

                    for (unsigned int new_import_func_idx = 0; new_import_func_idx < new_import_lib.size(); new_import_func_idx++) {
                        imported_func& new_import_func = new_import_lib.get_items()[new_import_func_idx];

                        if (reloc_ref_func.is_import_by_name()) {

                            if (new_import_func.is_import_by_name() && new_import_func.get_func_name() == reloc_ref_func.get_func_name()) {
                                reloc_item.relocation_id = SET_RELOCATION_ID_IAT(new_import_lib_idx, new_import_func_idx);
                                goto next_reloc_ref;
                            }
                        }
                        else {
                            if (!new_import_func.is_import_by_name() && new_import_func.get_ordinal() == reloc_ref_func.get_ordinal()) {
                                reloc_item.relocation_id = SET_RELOCATION_ID_IAT(new_import_lib_idx, new_import_func_idx);
                                goto next_reloc_ref;
                            }
                        }
                    }
                }
            }
            return false;
        next_reloc_ref:;
        }
    }

    expanded_image.imports = new_import_table;

    return true;
}


bool shibari_linker::process_import(shibari_module * module) {

    //x32 stub
    //jmp dword ptr [dd relocate to iat]

    //x64 stub
    //push rax
    //mov rax,[dq relocate to iat]
    //xchg rax,[rsp]
    //ret

    auto& expanded_image = module->get_module_expanded();

    pe_section* last_section = expanded_image.image.get_sections()[expanded_image.image.get_sections_number() - 1];

    if (last_section->get_virtual_size() > last_section->get_size_of_raw_data()) {
        pe_section_io(*last_section, expanded_image.image, enma_io_mode_allow_expand).add_size(
            last_section->get_virtual_size() - last_section->get_size_of_raw_data()
        );
    }

    last_section->set_executable(true);

    pe_image_io iat_wrapper_io(expanded_image.image, enma_io_mode_allow_expand);
    iat_wrapper_io.seek_to_end();

    for (unsigned int lib_idx = 0; lib_idx < expanded_image.imports.size(); lib_idx++) {
        imported_library& current_lib = expanded_image.imports.get_libraries()[lib_idx];

        pe_image_io iat_io(expanded_image.image, enma_io_mode_allow_expand);
        

        for (unsigned int func_idx = 0; func_idx < current_lib.size(); func_idx++) {
            imported_func& current_func = current_lib.get_items()[func_idx];

            if (expanded_image.image.is_x32_image()) {
                uint32_t import_wrapper_rva = iat_wrapper_io.get_image_offset();
                uint8_t wrapper_stub[] = { 0xFF ,0x25 ,0,0,0,0 };

                module->get_code_symbols().push_back({ import_wrapper_rva , sizeof(wrapper_stub) });

                uint32_t iat_reloc = (uint32_t)expanded_image.image.rva_to_va(import_wrapper_rva);


                if (iat_io.set_image_offset(current_func.get_iat_rva()).write(
                    &iat_reloc, sizeof(iat_reloc)) == enma_io_success) {
                    expanded_image.relocations.add_item(current_func.get_iat_rva(), relocation_index_default);
                }
                else {
                    return false;
                }

                if (iat_wrapper_io.write(
                    wrapper_stub, sizeof(wrapper_stub)) == enma_io_success) {
                    expanded_image.relocations.add_item(import_wrapper_rva + 2, SET_RELOCATION_ID_IAT(lib_idx, func_idx));
                }
                else {
                    return false;
                }
            }
            else {
                uint32_t import_wrapper_rva = iat_wrapper_io.get_image_offset();
                uint8_t wrapper_stub[] = {
                    0x50,                       //push rax
                    0x48 ,0xA1 ,0,0,0,0,0,0,0,0,//mov rax,[dq relocate to iat]
                    0x48 ,0x87 ,4,0x24,         //xchg [rsp],rax
                    0xC3,                       //ret
                };

                module->get_code_symbols().push_back({ import_wrapper_rva , sizeof(wrapper_stub) });

                uint64_t iat_reloc = expanded_image.image.rva_to_va(import_wrapper_rva);

                if (iat_io.set_image_offset(current_func.get_iat_rva()).write(
                    &iat_reloc, sizeof(iat_reloc)) == enma_io_success) {
                    expanded_image.relocations.add_item(current_func.get_iat_rva(), relocation_index_default);
                }
                else {
                    return false;
                }

                if (iat_wrapper_io.write(
                    wrapper_stub, sizeof(wrapper_stub)) == enma_io_success) {
                    expanded_image.relocations.add_item(import_wrapper_rva + 3, SET_RELOCATION_ID_IAT(lib_idx, func_idx));
                }
                else {
                    return false;
                }
            }
        }
    }

    return true;
}


bool shibari_linker::merge_imports() {

    for (auto& module : extended_modules) {
        for (auto & lib : module->get_image_imports().get_libraries()) {
            imported_library* main_lib;
            if (main_module->get_image_imports().get_imported_lib(lib.get_library_name(), main_lib)) {

                for (auto & func : lib.get_items()) {
                    imported_library* main_lib_;
                    imported_func * main_func_;
                    if (func.is_import_by_name()) {
                        if (!main_module->get_image_imports().get_imported_func(lib.get_library_name(), func.get_func_name(), main_lib_, main_func_)) {
                            main_lib->add_item(func);
                        }
                    }
                    else {
                        if (!main_module->get_image_imports().get_imported_func(lib.get_library_name(), func.get_ordinal(), main_lib_, main_func_)) {
                            main_lib->add_item(func);
                        }
                    }
                }
            }
            else {
                main_module->get_image_imports().add_library(lib);
            }
        }
    }


    //
    import_table new_import_table = main_module->get_image_imports();

    for (unsigned int main_lib_idx = 0; main_lib_idx < new_import_table.get_libraries().size(); main_lib_idx++) {

        for (unsigned int lib_idx = main_lib_idx + 1; lib_idx < new_import_table.get_libraries().size(); lib_idx++) {

            if (NC_string_compare(new_import_table.get_libraries()[main_lib_idx].get_library_name(),
                new_import_table.get_libraries()[lib_idx].get_library_name())) {

                for (auto& func_item : new_import_table.get_libraries()[lib_idx].get_items()) {
                    for (auto& func_item_main : new_import_table.get_libraries()[main_lib_idx].get_items()) {
                        if (func_item.is_import_by_name() == func_item_main.is_import_by_name()) {

                            if (func_item.is_import_by_name()) {
                                if (func_item.get_func_name() == func_item_main.get_func_name()) {
                                    goto func_found;
                                }
                            }
                            else {
                                if (func_item.get_ordinal() == func_item_main.get_ordinal()) {
                                    goto func_found;
                                }
                            }


                        }
                    }
                    new_import_table.get_libraries()[main_lib_idx].get_items().push_back(func_item);
                func_found:;
                }

                new_import_table.get_libraries().erase(
                    new_import_table.get_libraries().begin() + lib_idx
                );

                lib_idx--;
            }
        }
    }


    return switch_import_refs(main_module->get_module_expanded(), new_import_table);
}

bool shibari_linker::merge_loadconfig() {

    for (auto & module : extended_modules) {
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

    return true;
}
bool shibari_linker::process_relocations(shibari_module * module) {

    auto& expanded_image = module->get_module_expanded();

    expanded_image.relocations.sort();
    std::vector<relocation_item>& items = expanded_image.relocations.get_items();

    pe_section * item_section = 0;
    for (size_t item_idx = 0, item_rva_low = 0, item_rva_high = 0; item_idx < expanded_image.relocations.size(); item_idx++) {

        if (!item_rva_low || !item_section ||
            items[item_idx].relative_virtual_address < item_rva_low ||
            items[item_idx].relative_virtual_address >= item_rva_high
            ) {

            item_section = expanded_image.image.get_section_by_rva(items[item_idx].relative_virtual_address);
            if (!item_section) { return false; }

            item_rva_low = item_section->get_virtual_address();
            item_rva_high = ALIGN_UP(item_section->get_virtual_address() + item_section->get_virtual_size(), 0x1000);
        }


        if (expanded_image.image.is_x32_image()) {
            items[item_idx].data = expanded_image.image.va_to_rva(*(uint32_t*)&item_section->get_section_data().data()[
                items[item_idx].relative_virtual_address - item_section->get_virtual_address()
            ]);
        }
        else {
            items[item_idx].data = expanded_image.image.va_to_rva(*(uint64_t*)&item_section->get_section_data().data()[
                items[item_idx].relative_virtual_address - item_section->get_virtual_address()
            ]);
        }
    }

    for (auto& item : expanded_image.relocations.get_items()) {

        uint32_t relocation_dst_rva = (uint32_t)item.data;


        for (unsigned int lib_idx = 0; lib_idx < expanded_image.imports.get_libraries().size(); lib_idx++) {//find rva in imports

            if (expanded_image.image.is_x32_image()) {
                if (relocation_dst_rva >= expanded_image.imports.get_libraries()[lib_idx].get_rva_iat() &&
                    relocation_dst_rva < expanded_image.imports.get_libraries()[lib_idx].get_items().size() * sizeof(uint32_t) + expanded_image.imports.get_libraries()[lib_idx].get_rva_iat()) {

                    item.relocation_id = SET_RELOCATION_ID_IAT(lib_idx,
                        ((relocation_dst_rva - expanded_image.imports.get_libraries()[lib_idx].get_rva_iat()) / sizeof(uint32_t)));

                    goto go_next_item_;
                }
            }
            else {
                if (relocation_dst_rva >= expanded_image.imports.get_libraries()[lib_idx].get_rva_iat() &&
                    relocation_dst_rva < expanded_image.imports.get_libraries()[lib_idx].get_items().size() * sizeof(uint64_t) + expanded_image.imports.get_libraries()[lib_idx].get_rva_iat()) {

                    item.relocation_id = SET_RELOCATION_ID_IAT(lib_idx,
                        ((relocation_dst_rva - expanded_image.imports.get_libraries()[lib_idx].get_rva_iat()) / sizeof(uint64_t)));

                    goto go_next_item_;
                }
            }

        }


        item.relocation_id = relocation_index_default;

    go_next_item_:;
    }


    return true;
}


bool shibari_linker::merge_relocations() {

    for (auto& module : extended_modules) {

        for (auto& item : module->get_image_relocations().get_items()) { //handle image relocations

            switch (item.relocation_id) {

            case relocation_index_default: {

                pe_section * target_section = main_module->get_image().get_section_by_rva(
                    item.relative_virtual_address + module->get_module_position().get_address_offset()
                );

                if (!target_section) {
                    return false;
                }

                if (main_module->get_image().is_x32_image()) {
                    (*(uint32_t*)&target_section->get_section_data()[
                        (item.relative_virtual_address + module->get_module_position().get_address_offset()) -
                            target_section->get_virtual_address()]) += (uint32_t(main_module->get_image().get_image_base() -
                                module->get_image().get_image_base()) +
                                module->get_module_position().get_address_offset()
                                );
                }
                else {
                    (*(uint64_t*)&target_section->get_section_data()[
                        (item.relative_virtual_address + module->get_module_position().get_address_offset()) -
                            target_section->get_virtual_address()]) += ((
                                main_module->get_image().get_image_base() -
                                module->get_image().get_image_base()) +
                                module->get_module_position().get_address_offset()
                                );
                }

                main_module->get_image_relocations().add_item(
                    item.relative_virtual_address + module->get_module_position().get_address_offset(), item.relocation_id);
                break;
            }

            default: {

                if (item.relocation_id&relocation_index_iat_address) {

                    unsigned int src_lib_idx = GET_HI_NUMBER(item.relocation_id);
                    unsigned int src_func_idx = GET_LO_NUMBER(item.relocation_id);

                    imported_library& src_library = module->get_image_imports().get_libraries()[src_lib_idx];
                    imported_func&    src_func = src_library.get_items()[src_func_idx];

                    unsigned int dst_lib_idx;
                    unsigned int dst_func_idx;

                    if (src_func.is_import_by_name()) {
                        if (get_import_func_index(main_module->get_image_imports(),
                            src_library.get_library_name(), src_func.get_func_name(),
                            dst_lib_idx, dst_func_idx)) {

                            main_module->get_image_relocations().add_item(
                                item.relative_virtual_address + module->get_module_position().get_address_offset(),
                                SET_RELOCATION_ID_IAT(dst_lib_idx, dst_func_idx)
                            );

                            break;
                        }
                    }
                    else {
                        if (get_import_func_index(main_module->get_image_imports(),
                            src_library.get_library_name(), src_func.get_ordinal(),
                            dst_lib_idx, dst_func_idx)) {

                            main_module->get_image_relocations().add_item(
                                item.relative_virtual_address + module->get_module_position().get_address_offset(),
                                SET_RELOCATION_ID_IAT(dst_lib_idx, dst_func_idx)
                            );

                            break;
                        }
                    }
                }
                return false;
            }
            }

        }
    }

    return true;
}


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
