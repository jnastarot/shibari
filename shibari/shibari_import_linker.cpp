#include "stdafx.h"
#include "shibari_import_linker.h"


shibari_import_linker::shibari_import_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module)
    : extended_modules(extended_modules), main_module(main_module) {}

shibari_import_linker::~shibari_import_linker(){}


shibari_linker_errors shibari_import_linker::link_modules() {

    for (auto& module : *extended_modules) {
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

            auto& main_lib_name = new_import_table.get_libraries()[main_lib_idx].get_library_name();
            auto& current_lib_name = new_import_table.get_libraries()[lib_idx].get_library_name();


            if (std::equal(main_lib_name.begin(), main_lib_name.end(),
                current_lib_name.begin(), current_lib_name.end(),
                [](char a, char b) { return tolower(a) == tolower(b); })) {

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


    if (!switch_import_refs(main_module->get_module_expanded(), new_import_table)) {
        return shibari_linker_errors::shibari_linker_error_import_linking;
    }


    return shibari_linker_errors::shibari_linker_ok;
}


bool shibari_import_linker::switch_import_refs(pe_image_expanded& expanded_image, import_table& new_import_table) {
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


                auto& new_import_lib_name = new_import_lib.get_library_name();
                auto& reloc_ref_lib_name = reloc_ref_lib.get_library_name();

                if (std::equal(new_import_lib_name.begin(), new_import_lib_name.end(),
                    reloc_ref_lib_name.begin(), reloc_ref_lib_name.end(),
                    [](char a, char b) { return tolower(a) == tolower(b); })) {

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

bool shibari_import_linker::process_import(shibari_module& target_module) {
    //x32 stub
    //jmp dword ptr [dd relocate to iat]

    //x64 stub
    //push rax
    //mov rax,[dq relocate to iat]
    //xchg rax,[rsp]
    //ret

    auto& expanded_image = target_module.get_module_expanded();

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

                target_module.get_code_symbols().push_back({ import_wrapper_rva , sizeof(wrapper_stub) });

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

                target_module.get_code_symbols().push_back({ import_wrapper_rva , sizeof(wrapper_stub) });

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

bool shibari_import_linker::get_import_func_index(const import_table& imports,
    const std::string& lib_name,const std::string& funcname,
    uint32_t & lib_idx, uint32_t & func_idx) {

    for (unsigned int current_lib_idx = 0; current_lib_idx < imports.get_libraries().size(); current_lib_idx++) {
        auto& current_library = imports.get_libraries()[current_lib_idx];

        auto& current_lib_name = current_library.get_library_name();

        if (std::equal(current_lib_name.begin(), current_lib_name.end(), 
            lib_name.begin(), lib_name.end(), 
            [](char a, char b) { return tolower(a) == tolower(b); })) {

            for (unsigned int current_func_idx = 0; current_func_idx < current_library.get_items().size(); current_func_idx++) {
                auto& current_func = current_library.get_items()[current_func_idx];

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

bool shibari_import_linker::get_import_func_index(const import_table& imports,
    const std::string& lib_name, uint16_t func_ordinal,
    uint32_t & lib_idx, uint32_t & func_idx) {

    for (unsigned int current_lib_idx = 0; current_lib_idx < imports.get_libraries().size(); current_lib_idx++) {
        auto& current_library = imports.get_libraries()[current_lib_idx];

        auto& current_lib_name = current_library.get_library_name();

        if (std::equal(current_lib_name.begin(), current_lib_name.end(), 
            lib_name.begin(), lib_name.end(), 
            [](char a, char b) { return tolower(a) == tolower(b); })) {

            for (unsigned int current_func_idx = 0; current_func_idx < current_library.get_items().size(); current_func_idx++) {
                auto& current_func = current_library.get_items()[current_func_idx];

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