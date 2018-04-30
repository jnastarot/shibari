#pragma once

enum shibari_linker_errors {
    shibari_linker_ok,
    shibari_linker_error_bad_input,
    shibari_linker_error_export_linking,
    shibari_linker_error_import_linking,
    shibari_linker_error_loadconfig_linking,
    shibari_linker_error_relocation_linking,
    shibari_linker_error_exceptions_linking,
    shibari_linker_error_tls_linking,
};

struct export_references {
    uint32_t wrapper_rva;
    std::string lib_name;
    export_table_item export_item;
    unsigned int module_export_owner;//-1 = main module other in bound modules
};

class shibari_linker{
    std::vector<shibari_module*> extended_modules;
    shibari_module* main_module;

    bool shibari_linker::explore_module(shibari_module * module);
    bool shibari_linker::process_import(pe_image_expanded& expanded_image);
    bool shibari_linker::process_relocations(pe_image_expanded& expanded_image);

    bool shibari_linker::switch_import_refs(pe_image_expanded& expanded_image, import_table& new_import_table);

    bool shibari_linker::get_export_references(std::vector<export_references>& export_refs);
    void shibari_linker::initialize_export_redirect_table(std::vector<export_references>& export_refs);

    void shibari_linker::merge_sections();
    bool shibari_linker::merge_relocations();
    bool shibari_linker::merge_import();
    bool shibari_linker::merge_export();
    bool shibari_linker::merge_tls();
    bool shibari_linker::merge_loadconfig();
    bool shibari_linker::merge_exceptions();
    void shibari_linker::merge_module_data();
public:
    shibari_linker::shibari_linker(std::vector<shibari_module*>& extended_modules,shibari_module* main_module);
    shibari_linker::~shibari_linker();

    shibari_linker_errors shibari_linker::link_modules();

};

