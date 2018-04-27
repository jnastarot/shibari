#pragma once

enum shibari_module_code {
    shibari_module_incorrect,
    shibari_module_initialize_failed,
    shibari_module_initialize_success,
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

class shibari_module_position {
    uint32_t current_position;//where first section is started in main module 
    uint32_t address_offset;//current position - original position
public:
    shibari_module_position::shibari_module_position();
    shibari_module_position::shibari_module_position(const shibari_module_position& position);
    shibari_module_position::~shibari_module_position();

    shibari_module_position& shibari_module_position::operator=(const shibari_module_position& position);
public:
    void shibari_module_position::get_current_position(uint32_t position);
    void shibari_module_position::get_address_offset(uint32_t offset);
public:
    uint32_t shibari_module_position::get_current_position() const;
    uint32_t shibari_module_position::get_address_offset() const;
};

class shibari_module_export {
    std::vector<std::string>       extended_names;
    std::vector<export_table_item> export_items;

public:
    shibari_module_export::shibari_module_export();
    shibari_module_export::shibari_module_export(const shibari_module_export& module_export);
    shibari_module_export::~shibari_module_export();


    shibari_module_export& shibari_module_export::operator=(const shibari_module_export& module_export);
public:
    void shibari_module_export::add_name(const std::string& name);
    void shibari_module_export::add_export(const export_table_item& item);
    
    void shibari_module_export::clear_names();
    void shibari_module_export::clear_exports();
public:
    size_t shibari_module_export::get_names_number() const;
    size_t shibari_module_export::get_exports_number() const;

    std::vector<std::string>&       shibari_module_export::get_names();
    std::vector<export_table_item>& shibari_module_export::get_export_items();
};


class shibari_module{
    shibari_module_code     module_code;
    pe_image_expanded       module_expanded;
    shibari_module_position module_position;
    shibari_module_export   module_exports;

    std::vector<shibari_module_entry_point> module_entrys;
public:
    shibari_module::shibari_module(pe_image &image);
    shibari_module::shibari_module(const shibari_module &module);
    shibari_module::~shibari_module();

    shibari_module& shibari_module::operator=(const shibari_module& module);

public:
    pe_image&               shibari_module::get_image();
    export_table&		    shibari_module::get_image_exports();
    import_table&		    shibari_module::get_image_imports();
    resource_directory&	    shibari_module::get_image_resources();
    exceptions_table&	    shibari_module::get_image_exceptions();
    relocation_table&	    shibari_module::get_image_relocations();
    debug_table&	        shibari_module::get_image_debug();
    tls_table&			    shibari_module::get_image_tls();
    load_config_table&	    shibari_module::get_image_load_config();
    delay_import_table&     shibari_module::get_image_delay_imports();
    bound_import_table&     shibari_module::get_image_bound_imports();

public:
    shibari_module_position&                 shibari_module::get_module_position();
    shibari_module_export&                   shibari_module::get_module_exports();
    std::vector<shibari_module_entry_point>& shibari_module::get_module_entrys();

    shibari_module_code shibari_module::get_module_code() const;
};

