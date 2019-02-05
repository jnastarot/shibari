#pragma once

class shibari_module;
class shibari_linker;
class shibari_builder;


#include "shibari_module.h"
#include "shibari_linker.h"
#include "shibari_builder.h"


class shibari {
    std::vector<shibari_module*> extended_modules;
    shibari_module* main_module;
public:
    shibari();
    ~shibari();

    shibari_linker_errors exec_shibari(std::vector<uint8_t>& out_image);
public:
    void set_main_module(shibari_module* module);
    void add_extended_module(shibari_module* module);
public:
    std::vector<shibari_module*>& get_extended_modules();
    const std::vector<shibari_module*>& get_extended_modules() const;
    shibari_module* get_main_module();
    const shibari_module* get_main_module() const;
};