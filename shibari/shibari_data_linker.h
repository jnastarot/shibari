#pragma once
class shibari_data_linker {
    std::vector<shibari_module*>* extended_modules;
    shibari_module* main_module;

public:
    shibari_data_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module);
    ~shibari_data_linker();

public:
    shibari_linker_errors link_modules_start();
    shibari_linker_errors link_modules_finalize();

};

