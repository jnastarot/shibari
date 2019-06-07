#include "stdafx.h"
#include "shibari_relocations_linker.h"


shibari_relocations_linker::shibari_relocations_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module)
    : extended_modules(extended_modules), main_module(main_module) {}


shibari_relocations_linker::~shibari_relocations_linker()
{

}

shibari_linker_errors shibari_relocations_linker::link_modules() {

    for (auto& module : *extended_modules) {

        for (auto& entry : module->get_module_image().get_relocations().get_entries()) { //handle image relocations

            switch (entry.relocation_id) {

            case relocation_index_default: {

                pe_section * target_section = main_module->get_module_image().get_image().get_section_by_rva(
                    entry.relative_virtual_address + module->get_module_position().get_address_offset()
                );

                if (!target_section) {
                    return shibari_linker_errors::shibari_linker_error_relocation_linking;
                }

                if (main_module->get_module_image().get_image().is_x32_image()) {
                    (*(uint32_t*)&target_section->get_section_data()[
                        (entry.relative_virtual_address + module->get_module_position().get_address_offset()) -
                            target_section->get_virtual_address()]) += (uint32_t(main_module->get_module_image().get_image().get_image_base() -
                                module->get_module_image().get_image().get_image_base()) +
                                module->get_module_position().get_address_offset()
                                );
                }
                else {
                    (*(uint64_t*)&target_section->get_section_data()[
                        (entry.relative_virtual_address + module->get_module_position().get_address_offset()) -
                            target_section->get_virtual_address()]) += ((
                                main_module->get_module_image().get_image().get_image_base() -
                                module->get_module_image().get_image().get_image_base()) +
                                module->get_module_position().get_address_offset()
                                );
                }

                main_module->get_module_image().get_relocations().add_entry(
                    entry.relative_virtual_address + module->get_module_position().get_address_offset(), entry.relocation_id);
                break;
            }

            default: {

                if (entry.relocation_id&relocation_index_iat_address) {

                    unsigned int src_lib_idx = GET_HI_NUMBER(entry.relocation_id);
                    unsigned int src_func_idx = GET_LO_NUMBER(entry.relocation_id);

                    pe_import_library& src_library = module->get_module_image().get_imports().get_libraries()[src_lib_idx];
                    pe_import_function&    src_func = src_library.get_entries()[src_func_idx];

                    unsigned int dst_lib_idx;
                    unsigned int dst_func_idx;

                    if (src_func.is_import_by_name()) {
                        if (shibari_import_linker::get_import_func_index(main_module->get_module_image().get_imports(),
                            src_library.get_library_name(), src_func.get_func_name(),
                            dst_lib_idx, dst_func_idx)) {

                            main_module->get_module_image().get_relocations().add_entry(
                                entry.relative_virtual_address + module->get_module_position().get_address_offset(),
                                SET_RELOCATION_ID_IAT(dst_lib_idx, dst_func_idx)
                            );

                            break;
                        }
                    }
                    else {
                        if (shibari_import_linker::get_import_func_index(main_module->get_module_image().get_imports(),
                            src_library.get_library_name(), src_func.get_ordinal(),
                            dst_lib_idx, dst_func_idx)) {

                            main_module->get_module_image().get_relocations().add_entry(
                                entry.relative_virtual_address + module->get_module_position().get_address_offset(),
                                SET_RELOCATION_ID_IAT(dst_lib_idx, dst_func_idx)
                            );

                            break;
                        }
                    }
                }

                return shibari_linker_errors::shibari_linker_error_relocation_linking;
            }
            }
        }
    }

    return shibari_linker_errors::shibari_linker_ok;
}

bool shibari_relocations_linker::process_relocations(shibari_module& target_module) {

    auto& image_full = target_module.get_module_image();

    image_full.get_relocations().sort();
    std::vector<pe_relocation_entry>& entries = image_full.get_relocations().get_entries();

    pe_section * entry_section = 0;
    for (size_t entry_idx = 0, entry_rva_low = 0, entry_rva_high = 0; entry_idx < image_full.get_relocations().size(); entry_idx++) {

        if (!entry_rva_low || !entry_section ||
            entries[entry_idx].relative_virtual_address < entry_rva_low ||
            entries[entry_idx].relative_virtual_address >= entry_rva_high
            ) {

            entry_section = image_full.get_image().get_section_by_rva(entries[entry_idx].relative_virtual_address);
            if (!entry_section) { return false; }

            entry_rva_low = entry_section->get_virtual_address();
            entry_rva_high = ALIGN_UP(entry_section->get_virtual_address() + entry_section->get_virtual_size(), 0x1000);
        }


        if (image_full.get_image().is_x32_image()) {
            entries[entry_idx].data = image_full.get_image().va_to_rva(*(uint32_t*)&entry_section->get_section_data().data()[
                entries[entry_idx].relative_virtual_address - entry_section->get_virtual_address()
            ]);
        }
        else {
            entries[entry_idx].data = image_full.get_image().va_to_rva(*(uint64_t*)&entry_section->get_section_data().data()[
                entries[entry_idx].relative_virtual_address - entry_section->get_virtual_address()
            ]);
        }
    }

    for (auto& entry : image_full.get_relocations().get_entries()) {

        uint32_t relocation_dst_rva = (uint32_t)entry.data;


        for (unsigned int lib_idx = 0; lib_idx < image_full.get_imports().get_libraries().size(); lib_idx++) {//find rva in imports

            if (image_full.get_image().is_x32_image()) {
                if (relocation_dst_rva >= image_full.get_imports().get_libraries()[lib_idx].get_rva_iat() &&
                    relocation_dst_rva < image_full.get_imports().get_libraries()[lib_idx].get_entries().size() * sizeof(uint32_t) + image_full.get_imports().get_libraries()[lib_idx].get_rva_iat()) {

                    entry.relocation_id = SET_RELOCATION_ID_IAT(lib_idx,
                        ((relocation_dst_rva - image_full.get_imports().get_libraries()[lib_idx].get_rva_iat()) / sizeof(uint32_t)));

                    goto go_next_entry_;
                }
            }
            else {
                if (relocation_dst_rva >= image_full.get_imports().get_libraries()[lib_idx].get_rva_iat() &&
                    relocation_dst_rva < image_full.get_imports().get_libraries()[lib_idx].get_entries().size() * sizeof(uint64_t) + image_full.get_imports().get_libraries()[lib_idx].get_rva_iat()) {

                    entry.relocation_id = SET_RELOCATION_ID_IAT(lib_idx,
                        ((relocation_dst_rva - image_full.get_imports().get_libraries()[lib_idx].get_rva_iat()) / sizeof(uint64_t)));

                    goto go_next_entry_;
                }
            }

        }


        entry.relocation_id = relocation_index_default;

    go_next_entry_:;
    }


    return true;
}