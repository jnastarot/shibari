#pragma once
class shibari_builder{
    shibari_module target_module;

    std::vector<uint8_t> shibari_builder::get_dos_headers(uint32_t& headers_size);
    void    shibari_builder::get_nt_header(uint32_t header_size, std::vector<uint8_t>& header);

    void        build_directories(bool build_relocations);

    void        finalize_headers();
    uint32_t    calculate_headers_size(uint32_t dos_headers_size, bool build_relocations);
   
    void    shibari_builder::align_sections(uint32_t start_header_size);


    void        shibari_builder::process_relocations();
public:
    shibari_builder::shibari_builder(const shibari_module& target_module, bool build_relocations, std::vector<uint8_t>& out_image);
    shibari_builder::~shibari_builder();
};

