#include "stdafx.h"
#include "shibari_export_linker.h"


struct export_references {
    uint32_t wrapper_rva;
    std::string lib_name;
    export_table_item export_item;
    unsigned int module_export_owner;//-1 = main module other in bound modules
};

bool get_export_references(std::vector<shibari_module*>& extended_modules, shibari_module& main_module, std::vector<export_references>& export_refs);
void initialize_export_redirect_table(std::vector<shibari_module*>& extended_modules, shibari_module& main_module, std::vector<export_references>& export_refs);


shibari_export_linker::shibari_export_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module)
    : extended_modules(extended_modules), main_module(main_module) {}

shibari_export_linker::~shibari_export_linker(){}

shibari_linker_errors shibari_export_linker::link_modules() {

    std::vector<export_references> export_refs;

    if (!get_export_references(*extended_modules, *main_module, export_refs)) { 
        return shibari_linker_errors::shibari_linker_ok;
    }

    initialize_export_redirect_table(*extended_modules, *main_module, export_refs);       //create table in last section for export redirect


    for (auto& ref : export_refs) {                      //fix relocations for redirected import 
        unsigned int ref_lib_idx;
        unsigned int ref_func_idx;

        if (ref.export_item.has_name()) {
            if (!shibari_import_linker::get_import_func_index(main_module->get_image_imports(),
                ref.lib_name, ref.export_item.get_func_name(),
                ref_lib_idx, ref_func_idx)) {

                return shibari_linker_errors::shibari_linker_error_export_linking; // =\ why?
            }
        }
        else {
            if (!shibari_import_linker::get_import_func_index(main_module->get_image_imports(),
                ref.lib_name, ref.export_item.get_ordinal(),
                ref_lib_idx, ref_func_idx)) {

                return shibari_linker_errors::shibari_linker_error_export_linking; // =\ why?
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
            auto& import_lib_name = import_lib.get_library_name();
            
            if (std::equal(ref.lib_name.begin(), ref.lib_name.end(),
                import_lib_name.begin(), import_lib_name.end(),
                [](char a, char b) { return tolower(a) == tolower(b); })) {

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

    if (!shibari_import_linker::switch_import_refs(main_module->get_module_expanded(), new_import_table)) {
        return shibari_linker_errors::shibari_linker_error_export_linking;
    }

    return shibari_linker_errors::shibari_linker_ok;
}

bool get_export_references(std::vector<shibari_module*>& extended_modules, shibari_module& main_module, std::vector<export_references>& export_refs) {
    import_table imports = main_module.get_image_imports();
    std::vector<shibari_module*> modules_pool;

    modules_pool.push_back(&main_module);
    for (auto& module_ : extended_modules) { modules_pool.push_back(module_); }


    for (unsigned int module_idx = 0; module_idx < modules_pool.size(); module_idx++) {
        shibari_module* current_module = modules_pool[module_idx];
        shibari_module_export& module_export = current_module->get_module_exports();

        if (module_export.get_exports_number()) {

            for (unsigned int import_lib_idx = 0; import_lib_idx < imports.get_libraries().size(); import_lib_idx++) {
                imported_library & import_lib = imports.get_libraries()[import_lib_idx];
                std::string current_export_lib_name;

                bool is_lib_name_match = false;

                auto& import_lib_name = import_lib.get_library_name();

                for (auto& exp_name : module_export.get_names()) {

                    if (std::equal(exp_name.begin(), exp_name.end(),
                        import_lib_name.begin(), import_lib_name.end(),
                        [](char a, char b) { return tolower(a) == tolower(b); })) { //check for library name

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



void initialize_export_redirect_table(std::vector<shibari_module*>& extended_modules, shibari_module& main_module, std::vector<export_references>& export_refs) {

    pe_section *table_section = main_module.get_image().get_section_by_idx(    //get last section
        uint32_t(main_module.get_image().get_sections_number()) - 1
    );

    uint32_t redirect_table_rva = table_section->get_virtual_address() + table_section->get_virtual_size();
    pe_image_io redirect_io = pe_image_io(main_module.get_image(), enma_io_mode_allow_expand);
    redirect_io.set_image_offset(redirect_table_rva);

    for (unsigned int i = 0; i < export_refs.size(); i++) { //initialize redirect table

        if (main_module.get_image().is_x32_image()) {
            export_refs[i].wrapper_rva = redirect_io.get_image_offset();

            uint32_t reloc = (uint32_t)main_module.get_image().rva_to_va(export_refs[i].export_item.get_rva());

            if (export_refs[i].module_export_owner != -1) {
                reloc += extended_modules[export_refs[i].module_export_owner]->get_module_position().get_address_offset();
            }


            redirect_io.write(&reloc, sizeof(reloc));
        }
        else {
            export_refs[i].wrapper_rva = redirect_io.get_image_offset();

            uint64_t reloc = main_module.get_image().rva_to_va(export_refs[i].export_item.get_rva());

            if (export_refs[i].module_export_owner != -1) {
                reloc += extended_modules[export_refs[i].module_export_owner]->get_module_position().get_address_offset();
            }

            redirect_io.write(&reloc, sizeof(reloc));
        }

        main_module.get_image_relocations().add_item(export_refs[i].wrapper_rva, relocation_index_default);
    }
}