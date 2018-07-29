#pragma once
class shibari_builder{
    shibari_module _module;

    std::vector<uint8_t> shibari_builder::get_start_header();
    
    uint32_t    shibari_builder::align_sections(uint32_t start_header_size);
    void        shibari_builder::get_nt_header(uint32_t header_size,  std::vector<uint8_t>& header);
    void        shibari_builder::build_directories(bool build_relocations);

    void        shibari_builder::process_relocations();
public:
    shibari_builder::shibari_builder(const shibari_module& module, bool build_relocations, std::vector<uint8_t>& out_image);
    shibari_builder::~shibari_builder();
};

