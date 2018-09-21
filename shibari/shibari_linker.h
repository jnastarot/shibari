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


#include "shibari_import_linker.h"
#include "shibari_export_linker.h"
#include "shibari_loadconfigs_linker.h"
#include "shibari_exceptions_linker.h"
#include "shibari_relocations_linker.h"
#include "shibari_tls_linker.h"
#include "shibari_data_linker.h"

class shibari_linker{
    std::vector<shibari_module*> extended_modules;
    shibari_module* main_module;

    bool shibari_linker::explore_module(shibari_module * module);
public:
    shibari_linker::shibari_linker(std::vector<shibari_module*>& extended_modules,shibari_module* main_module);
    shibari_linker::~shibari_linker();

    shibari_linker_errors shibari_linker::link_modules();

};

