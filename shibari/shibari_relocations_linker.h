#pragma once
class shibari_relocations_linker {
    std::vector<shibari_module*>* extended_modules;
    shibari_module* main_module;
public:
    shibari_relocations_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module);
    ~shibari_relocations_linker();
    
public:
    shibari_linker_errors link_modules();

public:
    static bool process_relocations(shibari_module& target_module);

};

