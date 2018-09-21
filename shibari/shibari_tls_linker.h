#pragma once
class shibari_tls_linker {
    std::vector<shibari_module*>* extended_modules;
    shibari_module* main_module;
public:
    shibari_tls_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module);
    ~shibari_tls_linker();

public:
    shibari_linker_errors link_modules();

};

