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
    this->export_entries = module_export.export_entries;

    return *this;
}

void shibari_module_export::add_name(const std::string& name) {
    this->extended_names.push_back(name);
}

void shibari_module_export::add_export(const pe_export_entry& entry) {
    this->export_entries.push_back(entry);
}

void shibari_module_export::clear_names() {
    this->extended_names.clear();
}
void shibari_module_export::clear_exports() {
    this->export_entries.clear();
}

size_t shibari_module_export::get_names_number() const {
    return this->extended_names.size();
}
size_t shibari_module_export::get_exports_number() const {
    return this->export_entries.size();
}

std::vector<std::string>&       shibari_module_export::get_names() {
    return this->extended_names;
}
const std::vector<std::string>& shibari_module_export::get_names() const {
    return this->extended_names;
}

std::vector<pe_export_entry>& shibari_module_export::get_export_items() {
    return this->export_entries;
}
const std::vector<pe_export_entry>& shibari_module_export::get_export_items() const {
    return this->export_entries;
}

shibari_module::shibari_module() {
    this->module_code = shibari_module_code::shibari_module_incorrect;
}

shibari_module::shibari_module(const pe_image& image) {

    if (image.get_image_status() == pe_image_status::pe_image_status_ok) {

        this->image_full = image;

        get_extended_exception_info(this->image_full);
        get_runtime_type_information(this->image_full, this->rtti);

        if (this->image_full.get_directory_code() != pe_directory_code_success) {
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
shibari_module::~shibari_module() { }

shibari_module& shibari_module::operator=(const shibari_module& _module) {

    this->module_code     = _module.module_code;
    this->image_full      = _module.image_full;
    this->module_position = _module.module_position;
    this->module_exports  = _module.module_exports;
    this->module_entrys   = _module.module_entrys;

    this->rtti = _module.rtti;

    return *this;
}

void shibari_module::set_module_code(shibari_module_code code) {
    this->module_code = code;
}

msvc_rtti_desc& shibari_module::get_rtti() {
    return this->rtti;
}
pe_placement& shibari_module::get_free_space() {
    return this->free_space;
}

const msvc_rtti_desc& shibari_module::get_rtti() const {
    return this->rtti;
}

const pe_placement& shibari_module::get_free_space() const {
    return this->free_space;
}

pe_image_full&                       shibari_module::get_module_image() {
    return this->image_full;
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

const pe_image_full&                       shibari_module::get_module_image() const {
    return this->image_full;
}
const shibari_module_position&                 shibari_module::get_module_position() const {
    return this->module_position;
}
const shibari_module_export&                   shibari_module::get_module_exports() const {
    return this->module_exports;
}
const std::vector<shibari_module_entry_point>& shibari_module::get_module_entrys() const {
    return this->module_entrys;
}
const std::vector<shibari_module_symbol_info>& shibari_module::get_code_symbols() const {
    return this->code_symbols;
}
const std::vector<shibari_module_symbol_info>& shibari_module::get_data_symbols() const {
    return this->data_symbols;
}

shibari_module_code shibari_module::get_module_code() const {
    return this->module_code;
}