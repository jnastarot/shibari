#pragma once
class shibari_loadconfigs_linker {
    std::vector<shibari_module*>* extended_modules;
    shibari_module* main_module;
public:
    shibari_loadconfigs_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module);
    ~shibari_loadconfigs_linker();

public:
    shibari_linker_errors link_modules();

};

