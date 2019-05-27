#pragma once

#define SET_HI_NUMBER(x,num) (((x)&0xF000FFFF)|(((num)&0xFFF)<<16))
#define SET_LO_NUMBER(x,num) (((x)&0xFFFF0000)| ((num)&0xFFFF))

#define GET_HI_NUMBER(x) (((x)&0x0FFF0000)>>16)
#define GET_LO_NUMBER(x) (((x)&0x0000FFFF))

#define SET_RELOCATION_ID_IAT(lib_num,func_num) ((((lib_num)&0xFFF)<<16)|((func_num)&0xFFFF)|relocation_index_iat_address)

enum shibari_module_relocation_index {
    relocation_index_default = 1,
    relocation_index_iat_address = 0xF0000000,//((reloc_id&0x0FFF0000)>>16) module id (reloc_id&0x0000FFFF) function id
};

enum shibari_module_code {
    shibari_module_incorrect,
    shibari_module_correct,
    shibari_module_initialization_failed,
    shibari_module_initialization_success,
};

enum shibari_entry_point {
    shibari_entry_point_exe = 1,
    shibari_entry_point_dll = 2,
    shibari_entry_point_sys = 3,
};

struct shibari_module_entry_point {
    shibari_entry_point type;
    uint32_t entry_point_rva;
};

struct shibari_module_symbol_info {
    uint32_t symbol_info_rva;
    uint32_t symbol_info_size;
};

class shibari_module_position {
    uint32_t current_position;//where first section is started in main module 
    uint32_t address_offset;//current position - original position
public:
    shibari_module_position();
    shibari_module_position(const shibari_module_position& position);
    ~shibari_module_position();

    shibari_module_position& operator=(const shibari_module_position& position);
public:
    void set_current_position(uint32_t position);
    void set_address_offset(uint32_t offset);
public:
    uint32_t get_current_position() const;
    uint32_t get_address_offset() const;
};

class shibari_module_export {
    std::vector<std::string>       extended_names;
    std::vector<pe_export_entry>   export_entries;
public:
    shibari_module_export();
    shibari_module_export(const shibari_module_export& module_export);
    ~shibari_module_export();


    shibari_module_export& operator=(const shibari_module_export& module_export);
public:
    void add_name(const std::string& name);
    void add_export(const pe_export_entry& item);
    
    void clear_names();
    void clear_exports();
public:
    size_t get_names_number() const;
    size_t get_exports_number() const;

    std::vector<std::string>&       get_names() ;
    const std::vector<std::string>&       get_names() const;
    std::vector<pe_export_entry>& get_export_items();
    const std::vector<pe_export_entry>& get_export_items() const;
};


class shibari_module{
    shibari_module_code     module_code;
    pe_image_full           image_full;
    shibari_module_position module_position;
    shibari_module_export   module_exports;

    msvc_rtti_desc rtti;

    std::vector<shibari_module_entry_point> module_entrys;

    std::vector<shibari_module_symbol_info> code_symbols;
    std::vector<shibari_module_symbol_info> data_symbols;

    pe_placement free_space;
public:
    shibari_module();
    shibari_module(const std::string& path);
    shibari_module(const pe_image& image);
    shibari_module(const shibari_module &module);
    ~shibari_module();

    shibari_module& operator=(const shibari_module& module);

    void set_module_code(shibari_module_code code);
public:
    msvc_rtti_desc& get_rtti();
    pe_placement& get_free_space();

public:
    const msvc_rtti_desc& get_rtti() const;
    const pe_placement& get_free_space() const;
public:
    pe_image_full&                           get_module_image();
    shibari_module_position&                 get_module_position();
    shibari_module_export&                   get_module_exports();
    std::vector<shibari_module_entry_point>& get_module_entrys();
    std::vector<shibari_module_symbol_info>& get_code_symbols();
    std::vector<shibari_module_symbol_info>& get_data_symbols();

public:
    const pe_image_full&                           get_module_image() const;
    const shibari_module_position&                 get_module_position() const;
    const shibari_module_export&                   get_module_exports() const;
    const std::vector<shibari_module_entry_point>& get_module_entrys() const;
    const std::vector<shibari_module_symbol_info>& get_code_symbols() const;
    const std::vector<shibari_module_symbol_info>& get_data_symbols() const;

    shibari_module_code get_module_code() const;
};

