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
    std::vector<export_table_item> export_items;
public:
    shibari_module_export();
    shibari_module_export(const shibari_module_export& module_export);
    ~shibari_module_export();


    shibari_module_export& operator=(const shibari_module_export& module_export);
public:
    void add_name(const std::string& name);
    void add_export(const export_table_item& item);
    
    void clear_names();
    void clear_exports();
public:
    size_t get_names_number() const;
    size_t get_exports_number() const;

    std::vector<std::string>&       get_names() ;
    const std::vector<std::string>&       get_names() const;
    std::vector<export_table_item>& get_export_items();
    const std::vector<export_table_item>& get_export_items() const;
};


class shibari_module{
    shibari_module_code     module_code;
    pe_image_expanded       module_expanded;
    shibari_module_position module_position;
    shibari_module_export   module_exports;

    std::vector<shibari_module_entry_point> module_entrys;

    std::vector<shibari_module_symbol_info> code_symbols;
    std::vector<shibari_module_symbol_info> data_symbols;
public:
    shibari_module();
    shibari_module(const std::string& path);
    shibari_module(const pe_image& image);
    shibari_module(const shibari_module &module);
    ~shibari_module();

    shibari_module& operator=(const shibari_module& module);

    void set_module_code(shibari_module_code code);
public:
    pe_image&               get_image();
    export_table&		    get_image_exports();
    import_table&		    get_image_imports();
    resource_directory&	    get_image_resources();
    exceptions_table&	    get_image_exceptions();
    relocation_table&	    get_image_relocations();
    debug_table&	        get_image_debug();
    tls_table&			    get_image_tls();
    load_config_table&	    get_image_load_config();
    delay_import_table&     get_image_delay_imports();
    bound_import_table&     get_image_bound_imports();


public:
    const pe_image&             get_image() const;
    const export_table&         get_image_exports() const;
    const import_table&         get_image_imports() const;
    const resource_directory&   get_image_resources() const;
    const exceptions_table&	    get_image_exceptions() const;
    const relocation_table&	    get_image_relocations() const;
    const debug_table&	        get_image_debug() const;
    const tls_table&            get_image_tls() const;
    const load_config_table&    get_image_load_config() const;
    const delay_import_table&   get_image_delay_imports() const;
    const bound_import_table&   get_image_bound_imports() const;

public:
    pe_image_expanded&                       get_module_expanded();
    shibari_module_position&                 get_module_position();
    shibari_module_export&                   get_module_exports();
    std::vector<shibari_module_entry_point>& get_module_entrys();
    std::vector<shibari_module_symbol_info>& get_code_symbols();
    std::vector<shibari_module_symbol_info>& get_data_symbols();

public:
    const pe_image_expanded&                       get_module_expanded() const;
    const shibari_module_position&                 get_module_position() const;
    const shibari_module_export&                   get_module_exports() const;
    const std::vector<shibari_module_entry_point>& get_module_entrys() const;
    const std::vector<shibari_module_symbol_info>& get_code_symbols() const;
    const std::vector<shibari_module_symbol_info>& get_data_symbols() const;

    shibari_module_code get_module_code() const;
};

