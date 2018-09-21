#pragma once
class shibari_export_linker {
    std::vector<shibari_module*>* extended_modules;
    shibari_module* main_module;
public:
    shibari_export_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module);
    ~shibari_export_linker();

public:
    shibari_linker_errors link_modules();

};

