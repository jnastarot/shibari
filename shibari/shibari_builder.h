#pragma once
class shibari_builder{
    std::vector<uint8_t> shibari_builder::get_start_header(pe_image& image);
    
    uint32_t    shibari_builder::align_sections(pe_image& image, uint32_t start_header_size);
    void        shibari_builder::get_nt_header(pe_image& image, uint32_t header_size,  std::vector<uint8_t>& header);
    void        shibari_builder::build_directories(pe_image_expanded& expanded_image, bool build_relocations);

    void        shibari_builder::process_relocations(pe_image_expanded& expanded_image);
public:
    shibari_builder::shibari_builder(shibari_module& module, bool build_relocations, std::vector<uint8_t>& out_image);
    shibari_builder::~shibari_builder();
};

