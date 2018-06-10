#include "stdafx.h"
#include "shibari_builder.h"


shibari_builder::shibari_builder(shibari_module& module, bool build_relocations, std::vector<uint8_t>& out_image){

    out_image = get_start_header(module.get_image());

    size_t   image_intro_size = out_image.size();
    uint32_t nt_header_size = (module.get_image().is_x32_image() ? sizeof(image_nt_headers32) : sizeof(image_nt_headers64));

    uint32_t first_section_pointer = align_sections(module.get_image(), uint32_t(image_intro_size + nt_header_size));

    build_directories(module.get_module_expanded(), build_relocations);

    out_image.resize(first_section_pointer);

    std::vector<uint8_t> nt_header;
    get_nt_header(module.get_image(), first_section_pointer, nt_header);
    memcpy(&out_image.data()[image_intro_size], nt_header.data(), nt_header.size());

    for (unsigned int section_idx = 0; section_idx < module.get_image().get_sections_number(); section_idx++) {
        image_section_header section_hdr = { 0 };

        memcpy(section_hdr.name, module.get_image().get_section_by_idx(section_idx)->get_section_name().c_str(),
            min(module.get_image().get_section_by_idx(section_idx)->get_section_name().length(), 8));

        section_hdr.virtual_size        = module.get_image().get_section_by_idx(section_idx)->get_virtual_size();
        section_hdr.virtual_address     = module.get_image().get_section_by_idx(section_idx)->get_virtual_address();
        section_hdr.size_of_raw_data    = module.get_image().get_section_by_idx(section_idx)->get_size_of_raw_data();
        section_hdr.pointer_to_raw_data = module.get_image().get_section_by_idx(section_idx)->get_pointer_to_raw();
        section_hdr.characteristics     = module.get_image().get_section_by_idx(section_idx)->get_characteristics();

        memcpy(&out_image.data()[image_intro_size + nt_header_size + (sizeof(image_section_header) * section_idx)],
            &section_hdr, sizeof(image_section_header));
    }


    out_image.resize(first_section_pointer +
        module.get_image().get_section_by_idx(uint32_t(module.get_image().get_sections_number()) - 1)->get_pointer_to_raw() +
        ALIGN_UP(
            module.get_image().get_section_by_idx(uint32_t(module.get_image().get_sections_number()) - 1)->get_size_of_raw_data()
            , module.get_image().get_file_align()));

    for (auto& section_ : module.get_image().get_sections()) {
        memcpy(&out_image.data()[section_->get_pointer_to_raw()],
            section_->get_section_data().data(), section_->get_size_of_raw_data());
    }

    *(uint32_t*)&out_image.data()[image_intro_size + offsetof(image_nt_headers32, optional_header.checksum)] =
        calculate_checksum(out_image);
}


shibari_builder::~shibari_builder()
{
}



void shibari_builder::process_relocations(pe_image_expanded& expanded_image) {

    for (auto& reloc_item : expanded_image.relocations.get_items()) {

        if (reloc_item.relocation_id & relocation_index_iat_address) {

            unsigned int reloc_lib_idx  = GET_HI_NUMBER(reloc_item.relocation_id);
            unsigned int reloc_func_idx = GET_LO_NUMBER(reloc_item.relocation_id);

            imported_library& reloc_ref_lib = expanded_image.imports.get_libraries()[reloc_lib_idx];
            imported_func& reloc_ref_func = reloc_ref_lib.get_items()[reloc_func_idx];

            pe_section * target_section = expanded_image.image.get_section_by_rva(reloc_item.relative_virtual_address);

            if (!target_section) { continue; }

            if (expanded_image.image.is_x32_image()) {
                (*(uint32_t*)&target_section->get_section_data()[
                    (reloc_item.relative_virtual_address) - target_section->get_virtual_address()
                ]) = (uint32_t)expanded_image.image.rva_to_va(reloc_ref_func.get_iat_rva());
            }
            else {
                (*(uint64_t*)&target_section->get_section_data()[
                    (reloc_item.relative_virtual_address) - target_section->get_virtual_address()
                ]) = expanded_image.image.rva_to_va(reloc_ref_func.get_iat_rva());
            }

        }
    }
}


void shibari_builder::build_directories(pe_image_expanded& expanded_image, bool build_relocations) {

    if (expanded_image.imports.size() ||
        expanded_image.exports.get_number_of_functions() ||
        expanded_image.relocations.size() ||
        expanded_image.exceptions.size() ||
        (expanded_image.tls.get_address_of_index() || expanded_image.tls.get_callbacks().size()) ||
        (!expanded_image.image.is_x32_image() && expanded_image.exceptions.get_items().size()) ||
        (!expanded_image.load_config.get_size())
        ) {

        bool was_build = false;

        pe_section& dir_section = expanded_image.image.add_section();
        dir_section.set_section_name(std::string(".rdata"));
        dir_section.set_readable(true).set_writeable(true).set_executable(false);

        if ((!expanded_image.image.is_x32_image() && expanded_image.exceptions.size())) {
            build_exceptions_table(expanded_image.image, dir_section, expanded_image.exceptions);  //build exceptions
            was_build = true;
        }
        if (expanded_image.exports.get_number_of_functions()) {                                    //build export
            build_export_table(expanded_image.image, dir_section, expanded_image.exports);
            was_build = true;
        }
        if (expanded_image.imports.size()) {                                                        //build import
            build_import_table_full(expanded_image.image, dir_section, expanded_image.imports);
            was_build = true;
        }
        if (expanded_image.tls.get_address_of_index() || expanded_image.tls.get_callbacks().size()) { //build tls
            build_tls_full(expanded_image.image, dir_section, expanded_image.tls, expanded_image.relocations);
            was_build = true;
        }
        if (!expanded_image.load_config.get_size()) {                                              //build load config
            build_load_config_table_full(expanded_image.image, dir_section, expanded_image.load_config, expanded_image.relocations);
            was_build = true;
        }
        if (expanded_image.relocations.size()) {                                                   //build relocations
            process_relocations(expanded_image);

            if (build_relocations) {
                build_relocation_table(expanded_image.image, dir_section, expanded_image.relocations);
                was_build = true;
            }
        }
    }



    if (expanded_image.resources.get_entry_list().size()) {                                        //build resources
        pe_section& rsrc_section = expanded_image.image.add_section();
        rsrc_section.set_section_name(std::string(".rsrc"));
        rsrc_section.set_readable(true).set_writeable(false).set_executable(false);

        build_resources_table(expanded_image.image, rsrc_section, expanded_image.resources);
    }
}


std::vector<uint8_t> shibari_builder::get_start_header(pe_image& image) {
#define GET_RICH_HASH(x,i) (((x) << (i)) | ((x) >> (32 - (i))))

    std::vector<uint8_t> image_start;

    if (image.get_dos_stub().get_stub().size()) {
        image_start.resize(sizeof(image_dos_header) + image.get_dos_stub().get_stub().size());
        memcpy(image_start.data(), &image.get_dos_header().get_header(), sizeof(image_dos_header));
        memcpy(&image_start.data()[sizeof(image_dos_header)], image.get_dos_stub().get_stub().data()
            , image.get_dos_stub().get_stub().size());
    }
    else {
        image_start.resize(sizeof(image_dos_header));
        memset(image_start.data(),0, sizeof(image_dos_header));
        image_dos_header* dos_header = (image_dos_header*)image_start.data();
        dos_header->e_magic = IMAGE_DOS_SIGNATURE;

        //todo 
    }


    if (image.get_rich_data().is_present()) {
        std::vector<uint32_t> rich_stub;

        size_t image_start_size = image_start.size();

        rich_stub.resize(4 +
            (image.get_rich_data().get_items().size() * 2) +
            4
        );

        uint32_t * rich_dw = rich_stub.data();

        rich_dw[0] = 0x536E6144; //DanS
        for (unsigned int item_idx = 0; item_idx < image.get_rich_data().get_items().size(); item_idx++) {
            rich_dw[4 + (item_idx * 2)] = (image.get_rich_data().get_items()[item_idx].get_compiler_build() & 0xFFFF) |
                ((image.get_rich_data().get_items()[item_idx].get_type() & 0xFFFF) << 16);
            rich_dw[4 + (item_idx * 2) + 1] = image.get_rich_data().get_items()[item_idx].get_count();
        }
        rich_dw[4 + (image.get_rich_data().get_items().size() * 2)] = 0x68636952;//Rich

        uint32_t rich_hash = uint32_t(image_start.size());

        for (unsigned int i = 0; i < image_start.size(); i++) { //dos header + stub
            if (i >= 0x3C && i < 0x40) { continue; }//skip e_lfanew

            rich_hash += GET_RICH_HASH((uint32_t)image_start.data()[i], i);
        }
        for (unsigned int i = 0; i < image.get_rich_data().get_items().size(); i++) { //Rich struct
            rich_hash += GET_RICH_HASH(rich_dw[4 + (i * 2)], rich_dw[4 + (i * 2) + 1]);
        }

        for (unsigned int i = 0; i < 4 + (image.get_rich_data().get_items().size() * 2); i++) {
            rich_dw[i] ^= rich_hash;
        }
        rich_dw[4 + (image.get_rich_data().get_items().size() * 2) + 1] = rich_hash;//Rich hash


        image_start.resize(image_start.size() + ((image.get_rich_data().get_items().size() * 2) + 8) * sizeof(uint32_t));
        memcpy(&image_start.data()[image_start_size], rich_stub.data(), rich_stub.size() * sizeof(uint32_t));
    }

    pimage_dos_header(image_start.data())->e_lfanew = uint32_t(image_start.size());

    return image_start;
}


template<typename image_format>
void _get_nt_header(pe_image& image, uint32_t header_size, std::vector<uint8_t>& header) {

    typename image_format::image_nt_headers nt_header;

    memset(&nt_header, 0, sizeof(typename image_format::image_nt_headers));

    nt_header.signature = IMAGE_NT_SIGNATURE;
   
    nt_header.file_header.machine = image.get_machine();
    nt_header.file_header.number_of_sections = (uint16_t)image.get_sections_number();
    nt_header.file_header.time_date_stamp = 0;
    nt_header.file_header.pointer_to_symbol_table = 0;
    nt_header.file_header.number_of_symbols = 0;
    nt_header.file_header.size_of_optional_header = sizeof(nt_header.optional_header);
    nt_header.file_header.characteristics = image.get_characteristics();
    nt_header.file_header.characteristics &= ~(IMAGE_FILE_RELOCS_STRIPPED | IMAGE_FILE_LINE_NUMS_STRIPPED |
        IMAGE_FILE_LOCAL_SYMS_STRIPPED | IMAGE_FILE_DEBUG_STRIPPED);

    nt_header.optional_header.magic = image_format::image_magic;

    nt_header.optional_header.major_linker_version = image.get_major_linker();
    nt_header.optional_header.minor_linker_version = image.get_minor_linker();

    nt_header.optional_header.size_of_code = 0;
    nt_header.optional_header.size_of_initialized_data = 0;
    nt_header.optional_header.size_of_uninitialized_data = 0;

    nt_header.optional_header.address_of_entry_point = image.get_entry_point();

    nt_header.optional_header.base_of_code = 0;

    nt_header.optional_header.image_base = (typename image_format::ptr_size)image.get_image_base();

    nt_header.optional_header.section_alignment = image.get_section_align();
    nt_header.optional_header.file_alignment    = image.get_file_align();

    nt_header.optional_header.major_operating_system_version = 5;//all windows versions from XP
    nt_header.optional_header.minor_operating_system_version = 1;
    nt_header.optional_header.major_image_version = image.get_image_ver_major();
    nt_header.optional_header.minor_image_version = image.get_image_ver_minor();
    nt_header.optional_header.major_subsystem_version = 5;
    nt_header.optional_header.minor_subsystem_version = 1;

    nt_header.optional_header.win32_version_value = 0;

    nt_header.optional_header.size_of_image = ALIGN_UP(
        (image.get_sections()[image.get_sections_number() - 1]->get_virtual_address() +
            image.get_sections()[image.get_sections_number() - 1]->get_virtual_size()),
        image.get_section_align());

    nt_header.optional_header.size_of_headers = ALIGN_UP(header_size, image.get_file_align());

    nt_header.optional_header.checksum = 0;

    nt_header.optional_header.subsystem             = image.get_sub_system();
    nt_header.optional_header.dll_characteristics   = image.get_characteristics_dll();

    nt_header.optional_header.size_of_stack_reserve = (typename image_format::ptr_size)image.get_stack_reserve_size();
    nt_header.optional_header.size_of_stack_commit  = (typename image_format::ptr_size)image.get_stack_commit_size();
    nt_header.optional_header.size_of_heap_reserve  = (typename image_format::ptr_size)image.get_heap_reserve_size();
    nt_header.optional_header.size_of_heap_commit   = (typename image_format::ptr_size)image.get_heap_commit_size();

    nt_header.optional_header.loader_flags = 0;
    nt_header.optional_header.number_of_rva_and_sizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

    for (unsigned int dir_idx = 0; dir_idx < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; dir_idx++) {
        nt_header.optional_header.data_directory[dir_idx].virtual_address = image.get_directory_virtual_address(dir_idx);
        nt_header.optional_header.data_directory[dir_idx].size = image.get_directory_virtual_size(dir_idx);
    }

    header.resize(sizeof(typename image_format::image_nt_headers));
    memcpy(header.data(), &nt_header, sizeof(nt_header));
}


uint32_t    shibari_builder::align_sections(pe_image& image,uint32_t start_header_size) {

    uint32_t first_section_raw = ALIGN_UP((start_header_size +
        (sizeof(image_section_header) * uint32_t(image.get_sections_number()))),
        image.get_file_align());

    uint32_t current_section_raw = first_section_raw;

    for (auto & section_ : image.get_sections()) {
        if (section_->get_size_of_raw_data() > section_->get_virtual_size()) {
            section_->set_virtual_size(section_->get_size_of_raw_data());
        }
        section_->set_pointer_to_raw(current_section_raw);

        current_section_raw += ALIGN_UP(section_->get_size_of_raw_data(),image.get_file_align());
    }

    return first_section_raw;
}

void shibari_builder::get_nt_header(pe_image& image, uint32_t header_size, std::vector<uint8_t>& header) {

    if (image.is_x32_image()) {
        _get_nt_header<image_32>(image, header_size, header);
    }
    else {
        _get_nt_header<image_64>(image, header_size, header);
    }
}