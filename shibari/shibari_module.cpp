#include "stdafx.h"
#include "shibari_module.h"


shibari_module_position::shibari_module_position() {
    this->current_position = 0;
    this->address_offset = 0;
}
shibari_module_position::shibari_module_position(const shibari_module_position& position) {
    this->operator=(position);
}
shibari_module_position::~shibari_module_position() {

}
shibari_module_position& shibari_module_position::operator=(const shibari_module_position& position) {

    this->current_position = position.current_position;
    this->address_offset = position.address_offset;

    return *this;
}

void shibari_module_position::set_current_position(uint32_t position) {
    this->current_position = position;
}
void shibari_module_position::set_address_offset(uint32_t offset) {
    this->address_offset = offset;
}

uint32_t shibari_module_position::get_current_position() const {
    return this->current_position;
}
uint32_t shibari_module_position::get_address_offset() const {
    return this->address_offset;
}


shibari_module_export::shibari_module_export() {

}
shibari_module_export::shibari_module_export(const shibari_module_export& module_export) {
    this->operator=(module_export);
}
shibari_module_export::~shibari_module_export() {

}

shibari_module_export& shibari_module_export::operator=(const shibari_module_export& module_export) {
    this->extended_names = module_export.extended_names;
    this->export_items   = module_export.export_items;

    return *this;
}

void shibari_module_export::add_name(const std::string& name) {
    this->extended_names.push_back(name);
}

void shibari_module_export::add_export(const export_table_item& item) {
    this->export_items.push_back(item);
}

void shibari_module_export::clear_names() {
    this->extended_names.clear();
}
void shibari_module_export::clear_exports() {
    this->export_items.clear();
}

size_t shibari_module_export::get_names_number() const {
    return this->extended_names.size();
}
size_t shibari_module_export::get_exports_number() const {
    return this->export_items.size();
}

std::vector<std::string>&       shibari_module_export::get_names() {
    return this->extended_names;
}
std::vector<export_table_item>& shibari_module_export::get_export_items() {
    return this->export_items;
}


shibari_module::shibari_module(const pe_image& image) {

    if (image.get_image_status() == pe_image_status::pe_image_status_ok) {
        do_expanded_pe_image(this->module_expanded, image);

        if (this->module_expanded.code != directory_code::directory_code_success) {
            this->module_code = shibari_module_code::shibari_module_incorrect;
        }
        else {
            this->module_code = shibari_module_code::shibari_module_correct;
        }
    }
    else {
        this->module_code = shibari_module_code::shibari_module_incorrect;
    }
}

shibari_module::shibari_module(const std::string& path) {
    this->operator=(shibari_module(pe_image(path)));
}
shibari_module::shibari_module(const shibari_module &module) {
    this->operator=(module);
}
shibari_module::~shibari_module() {

}

shibari_module& shibari_module::operator=(const shibari_module& module) {

    this->module_code     = module.module_code;
    this->module_expanded = module.module_expanded;
    this->module_position = module.module_position;
    this->module_exports  = module.module_exports;
    this->module_entrys   = module.module_entrys;

    return *this;
}

void shibari_module::set_module_code(shibari_module_code code) {
    this->module_code = code;
}

pe_image&               shibari_module::get_image() {
    return this->module_expanded.image;
}
export_table&		    shibari_module::get_image_exports() {
    return this->module_expanded.exports;
}
import_table&		    shibari_module::get_image_imports() {
    return this->module_expanded.imports;
}
resource_directory&	    shibari_module::get_image_resources() {
    return this->module_expanded.resources;
}
exceptions_table&	    shibari_module::get_image_exceptions() {
    return this->module_expanded.exceptions;
}
relocation_table&	    shibari_module::get_image_relocations() {
    return this->module_expanded.relocations;
}
debug_table&	        shibari_module::get_image_debug() {
    return this->module_expanded.debug;
}
tls_table&			    shibari_module::get_image_tls() {
    return this->module_expanded.tls;
}
load_config_table&	    shibari_module::get_image_load_config() {
    return this->module_expanded.load_config;
}
delay_import_table&     shibari_module::get_image_delay_imports() {
    return this->module_expanded.delay_imports;
}
bound_import_table&     shibari_module::get_image_bound_imports() {
    return this->module_expanded.bound_imports;
}

pe_image_expanded&                       shibari_module::get_module_expanded() {
    return this->module_expanded;
}
shibari_module_position&                 shibari_module::get_module_position() {
    return this->module_position;
}
shibari_module_export&                   shibari_module::get_module_exports() {
    return this->module_exports;
}
std::vector<shibari_module_entry_point>& shibari_module::get_module_entrys() {
    return this->module_entrys;
}
std::vector<shibari_module_symbol_info>& shibari_module::get_code_symbols() {
    return this->code_symbols;
}
std::vector<shibari_module_symbol_info>& shibari_module::get_data_symbols() {
    return this->data_symbols;
}
shibari_module_code shibari_module::get_module_code() const {
    return this->module_code;
}