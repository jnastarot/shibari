#include "stdafx.h"
#include "shibari_data_linker.h"
#include "shibari_data_linker_rtti_msvc.h"


shibari_data_linker::shibari_data_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module)
    : extended_modules(extended_modules), main_module(main_module){}


shibari_data_linker::~shibari_data_linker()
{
}

shibari_linker_errors shibari_data_linker::link_modules_start() {

    for (auto& module_ : *extended_modules) {            //merge sections

        module_->get_module_position().set_current_position(
            ALIGN_UP(main_module->get_module_image().get_image().get_sections()[
                main_module->get_module_image().get_image().get_sections_number() - 1
            ]->get_virtual_address() + main_module->get_module_image().get_image().get_sections()[
                main_module->get_module_image().get_image().get_sections_number() - 1]->get_virtual_size(),
                    main_module->get_module_image().get_image().get_section_align())
        );

        module_->get_module_position().set_address_offset(
            module_->get_module_position().get_current_position() - module_->get_module_image().get_image().get_sections()[0]->get_virtual_address()
        );

        uint32_t section_top_rva = module_->get_module_position().get_current_position();
        uint32_t section_top_raw = ALIGN_UP(main_module->get_module_image().get_image().get_sections()[
            main_module->get_module_image().get_image().get_sections_number() - 1
        ]->get_pointer_to_raw() + main_module->get_module_image().get_image().get_sections()[
            main_module->get_module_image().get_image().get_sections_number() - 1]->get_size_of_raw_data(),
                main_module->get_module_image().get_image().get_section_align());

        for (auto& section_ : module_->get_module_image().get_image().get_sections()) { //add sections
            pe_section pre_section = *section_;
            pre_section.set_virtual_address(section_top_rva);
            pre_section.set_pointer_to_raw(section_top_raw);
            main_module->get_module_image().get_image().add_section(pre_section);

            section_top_raw += ALIGN_UP(pre_section.get_pointer_to_raw() + pre_section.get_size_of_raw_data(),
                main_module->get_module_image().get_image().get_section_align());

            section_top_rva += ALIGN_UP(pre_section.get_virtual_size(),
                main_module->get_module_image().get_image().get_section_align());
        }
    }

    for (uint32_t dir_idx = 0; dir_idx < 16; dir_idx++) {
        main_module->get_module_image().get_image().set_directory_virtual_address(dir_idx, 0);
        main_module->get_module_image().get_image().set_directory_virtual_size(dir_idx, 0);
    }

    return shibari_linker_errors::shibari_linker_ok;
}

shibari_linker_errors shibari_data_linker::link_modules_finalize() {

    pe_image_io image_io(main_module->get_module_image().get_image(), enma_io_mode::enma_io_mode_allow_expand);
    pe_section *last_section = main_module->get_module_image().get_image().get_last_section();

    image_io.set_image_offset(
        last_section->get_virtual_address() +
        max(last_section->get_virtual_size(), last_section->get_size_of_raw_data())
    );

    last_section->set_executable(true); //TODO: FIXIT last section can be not last ;)

    for (auto& module_ : *extended_modules) {

        for (auto& item : module_->get_free_space()) {//link free spaces
            main_module->get_free_space()[item.first + module_->get_module_position().get_address_offset()] = item.second;
        }

        shift_rtti_data(module_, main_module); //link rtti data

        { //link rtti exports

            shibari_module_export& module_export = module_->get_module_exports();

            if (module_export.get_exports_number()) {
                for (auto& exp_item : module_export.get_export_items()) {
                    pe_export_entry entry = exp_item;
                    entry.set_rva(exp_item.get_rva() + module_->get_module_position().get_address_offset());

                    main_module->get_module_exports().add_export(entry);
                }

                for (auto& exp_name : module_export.get_names()) {
                    main_module->get_module_exports().add_name(exp_name);
                }
            }
        }

        { //link sumbols

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
        }

        if (module_->get_module_image().get_image().get_characteristics()&IMAGE_FILE_DLL) {
            main_module->get_module_entrys().push_back({ shibari_entry_point_dll,
                module_->get_module_position().get_address_offset() + module_->get_module_image().get_image().get_entry_point()
                });


            main_module->get_module_image().get_tls().get_callbacks().push_back({
                module_->get_module_image().get_image().get_entry_point() + module_->get_module_position().get_address_offset(), true
                });

        }
        else {
            main_module->get_module_entrys().push_back({ shibari_entry_point_exe,
                module_->get_module_position().get_address_offset() + module_->get_module_image().get_image().get_entry_point()
                });

            main_module->get_module_image().get_tls().get_callbacks().push_back({
                image_io.get_image_offset(), true
                });

            if (this->main_module->get_module_image().get_image().is_x32_image()) {

                uint8_t wrapper_stub[] = {
                    0x55,                       //push ebp
                    0x8B,0xEC,                  //mov ebp,esp
                    0x6A,0x00,                  //push 0
                    0x6A,0x00,                  //push 0
                    0x6A,0x00,                  //push 0
                    0xFF,0x75,0x08,             //push[ebp + 08]
                    0xB8,0x78,0x56,0x34,0x12,   //mov eax,12345678
                    0xFF,0xD0,                  //call eax
                    0x5D,                       //pop ebp
                    0xC2,0x0C,0x00              //ret 0xC
                };

                *(uint32_t*)&wrapper_stub[13] = uint32_t(main_module->get_module_image().get_image().get_image_base() +
                    module_->get_module_position().get_address_offset() + module_->get_module_image().get_image().get_entry_point());

                main_module->get_module_image().get_relocations().add_entry(image_io.get_image_offset() + 13, 0);

                main_module->get_code_symbols().push_back({
                    image_io.get_image_offset() , sizeof(wrapper_stub)
                    });

                image_io.write(wrapper_stub, sizeof(wrapper_stub));

            }
            else {

                uint8_t wrapper_stub[] = {
                    0x45,0x31,0xC9,              //xor r9d,r9d
                    0x45,0x31,0xC0,              //xor r8d,r8d
                    0x31,0xD2,                   //xor edx,edx
                    0xFF,0x25,0x00,0x00,0x00,    //jmp qword ptr [$+5] ---||
                                                 //     ||
                                                 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00   //   <-//
                };

                *(uint64_t*)&wrapper_stub[13] = main_module->get_module_image().get_image().get_image_base() +
                    module_->get_module_position().get_address_offset() + module_->get_module_image().get_image().get_entry_point();

                main_module->get_module_image().get_relocations().add_entry(image_io.get_image_offset() + 13, 0);

                main_module->get_code_symbols().push_back({
                    image_io.get_image_offset() , sizeof(wrapper_stub)
                    });

                image_io.write(wrapper_stub, sizeof(wrapper_stub));
            }
        }
    }

    return shibari_linker_errors::shibari_linker_ok;
}


void shibari_data_linker::shift_rtti_data(shibari_module* extended_module, shibari_module* main_module) {

    msvc_shift_module_rtti(extended_module, main_module);

    


}