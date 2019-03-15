#pragma once
class shibari_builder{
    shibari_module target_module;

    std::vector<uint8_t> get_dos_headers(size_t& headers_size);
    void    get_nt_header(uint32_t header_size, std::vector<uint8_t>& header);

    void     build_directories(bool build_relocations);
    size_t calculate_headers_size(size_t dos_headers_size, bool build_relocations);
   
    void    align_sections(uint32_t start_header_size);

    void        process_relocations();
public:
    shibari_builder(const shibari_module& target_module, bool build_relocations, std::vector<uint8_t>& out_image);
    ~shibari_builder();
};

