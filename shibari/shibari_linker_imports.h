#pragma once

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


bool shibari_linker::process_import(pe_image_expanded& expanded_image) {

    //x32 stub
    //jmp dword ptr [dd relocate to iat]

    //x64 stub
    //push rax
    //mov rax,[dq relocate to iat]
    //xchg rax,[rsp]
    //ret

    pe_section* last_section = expanded_image.image.get_sections()[expanded_image.image.get_sections_number() - 1];

    if (last_section->get_virtual_size() > last_section->get_size_of_raw_data()) {
        pe_section_io(*last_section, expanded_image.image, enma_io_mode_allow_expand).add_size(
            last_section->get_virtual_size() - last_section->get_size_of_raw_data()
        );
    }
    
    last_section->set_executable(true);



    for (unsigned int lib_idx = 0; lib_idx < expanded_image.imports.size(); lib_idx++) {
        imported_library& current_lib = expanded_image.imports.get_libraries()[lib_idx];

        pe_image_io iat_io(expanded_image.image, enma_io_mode_allow_expand);
        pe_image_io iat_wrapper_io(expanded_image.image, enma_io_mode_allow_expand);
        iat_wrapper_io.seek_to_end();

        for (unsigned int func_idx = 0; func_idx < current_lib.size(); func_idx++) {
            imported_func& current_func = current_lib.get_items()[func_idx];

            if (expanded_image.image.is_x32_image()) {
                uint32_t import_wrapper_rva = iat_wrapper_io.get_image_offset();
                uint8_t wrapper_stub[] = { 0xFF ,0x25 ,0,0,0,0 };

                

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