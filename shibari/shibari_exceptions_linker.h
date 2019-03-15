#pragma once
class shibari_exceptions_linker {
    std::vector<shibari_module*>* extended_modules;
    shibari_module* main_module;
public:
    shibari_exceptions_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module);
    ~shibari_exceptions_linker();

public:
    shibari_linker_errors link_modules();
};

