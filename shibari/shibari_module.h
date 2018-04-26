#pragma once

enum shibari_module_code {
    shibari_module_incorrect,
    shibari_module_uninitialized,
    shibari_module_initialized,
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
    shibari_module_position::~shibari_module_position();
};

class shibari_module_export {
    std::vector<std::string>       extended_names;
    std::vector<export_table_item> export_items;

public:
    shibari_module_export::shibari_module_export();
    shibari_module_export::~shibari_module_export();
};


class shibari_module{
    shibari_module_code     module_code;
    pe_image_expanded       module_expanded;
    shibari_module_position module_position;
    shibari_module_export   module_export;

    std::vector<std::string>                module_names;
    std::vector<shibari_module_entry_point> module_entrys;
public:
    shibari_module::shibari_module();
    shibari_module::~shibari_module();


};

