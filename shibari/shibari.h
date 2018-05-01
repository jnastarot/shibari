#pragma once
#include "..\enma_pe\enma_pe\enma_pe.h"

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
    shibari::shibari();
    shibari::~shibari();

    shibari_linker_errors shibari::exec_shibari(std::vector<uint8_t>& out_image);
public:
    void shibari::set_main_module(shibari_module* module);
    void shibari::add_extended_module(shibari_module* module);
public:
    std::vector<shibari_module*>& shibari::get_extended_modules();
    shibari_module* shibari::get_main_module();
};