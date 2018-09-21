#pragma once

class shibari_import_linker {
    std::vector<shibari_module*>* extended_modules;
    shibari_module* main_module;
public:
    shibari_import_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module);
    ~shibari_import_linker();

public:
    shibari_linker_errors link_modules();


public:
    static bool process_import(shibari_module& target_module);

    static bool shibari_import_linker::switch_import_refs(pe_image_expanded& expanded_image,
        import_table& new_import_table);

    static bool get_import_func_index(import_table& imports,
        std::string lib_name, std::string funcname,
        uint32_t & lib_idx, uint32_t & func_idx);

    static bool get_import_func_index(import_table& imports,
        std::string lib_name, uint16_t func_ordinal,
        uint32_t & lib_idx, uint32_t & func_idx);
};

