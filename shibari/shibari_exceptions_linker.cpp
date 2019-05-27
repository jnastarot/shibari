#include "stdafx.h"
#include "shibari_exceptions_linker.h"


shibari_exceptions_linker::shibari_exceptions_linker(std::vector<shibari_module*>* extended_modules, shibari_module* main_module)
    : extended_modules(extended_modules), main_module(main_module) {}


shibari_exceptions_linker::~shibari_exceptions_linker()
{
}


shibari_linker_errors shibari_exceptions_linker::link_modules() {
    if (main_module->get_module_image().get_image().is_x32_image()) { return shibari_linker_errors::shibari_linker_error_bad_input; }

    for (auto& ex_module : *extended_modules) {
        uint32_t module_offset = ex_module->get_module_position().get_address_offset();


        for (auto& unwind_entry_ : ex_module->get_module_image().get_exceptions().get_unwind_entries()) {

            main_module->get_module_image().get_exceptions().get_unwind_entries().push_back(unwind_entry_);

            auto& temp_unwind_entry = main_module->get_module_image().get_exceptions().get_unwind_entries()[
                main_module->get_module_image().get_exceptions().get_unwind_entries().size() - 1
            ];

            temp_unwind_entry.set_unwind_info_rva(temp_unwind_entry.get_unwind_info_rva() + module_offset);
            temp_unwind_entry.set_handler_rva(temp_unwind_entry.get_handler_rva() + module_offset);

            if (temp_unwind_entry.get_chained_entry()) {
                auto *chained_entry = temp_unwind_entry.get_chained_entry();
                
                chained_entry->set_begin_address(chained_entry->get_begin_address() + module_offset);
                chained_entry->set_end_address(chained_entry->get_end_address() + module_offset);
                chained_entry->set_unwind_data_address(chained_entry->get_unwind_data_address() + module_offset);
            }


            if (temp_unwind_entry.get_custom_parameter().get_data_type() != unknown_handler) {

                switch (temp_unwind_entry.get_custom_parameter().get_data_type()) {

                case unknown_handler: { break; }

                case __c_specific_handler: {
                    auto* data_ = temp_unwind_entry.get_custom_parameter().get_c_specific_handler_parameters_data();

                    for (auto& scope_entry : data_->table) {
                        scope_entry.begin_address += module_offset;
                        scope_entry.end_address += module_offset;

                        if (scope_entry.handler_address) {
                            scope_entry.handler_address += module_offset;
                        }
                        if (scope_entry.jump_target) {
                            scope_entry.jump_target += module_offset;
                        }
                    }

                    break;
                }
                case __delphi_specific_handler: {
                    auto* data_ = unwind_entry_.get_custom_parameter().get_delphi_specific_handler_parameters_data();

                    for (auto& scope_entry : data_->table) {
                        scope_entry.begin_address += module_offset;
                        scope_entry.end_address += module_offset;

                        if (scope_entry.handle_type > 2) {
                            scope_entry.handle_type += module_offset;
                        }
                        if (scope_entry.jump_target) {
                            scope_entry.jump_target += module_offset;
                        }
                    }
                    break;
                }
                case __llvm_specific_handler: {
                    auto* data_ = unwind_entry_.get_custom_parameter().get_llvm_specific_handler_parameters_data();

                    data_->data_rva += module_offset;

                    break;
                }
                case __gs_handler_check: {
                    auto* data_ = unwind_entry_.get_custom_parameter().get_gs_handler_check_parameters_data();
                    break;
                }
                case __gs_handler_check_seh: {
                    auto* data_ = unwind_entry_.get_custom_parameter().get_gs_handler_check_seh_parameters_data();

                    for (auto& scope_entry : data_->table) {
                        scope_entry.begin_address += module_offset;
                        scope_entry.end_address += module_offset;

                        if (scope_entry.handler_address) {
                            scope_entry.handler_address += module_offset;
                        }
                        if (scope_entry.jump_target) {
                            scope_entry.jump_target += module_offset;
                        }
                    }

                    break;
                }
                case __cxx_frame_handler3: {
                    auto* data_ = unwind_entry_.get_custom_parameter().get_cxx_frame_handler3_parameters_data();


                    for (auto& unwind_map_entry : data_->func_info.get_unwind_map_entries()) {
                        if (unwind_map_entry.p_action) {
                            unwind_map_entry.p_action += module_offset;
                        }
                    }

                    for (auto& try_block_entry : data_->func_info.get_try_block_map_entries()) {
                        for (auto& handler_type_entry : try_block_entry.get_handler_entries()) {
                            if (handler_type_entry.p_type) {
                                handler_type_entry.p_type += module_offset;
                            }
                            if (handler_type_entry.p_address_of_handler) {
                                handler_type_entry.p_address_of_handler += module_offset;
                            }
                        }

                        try_block_entry.set_p_handler_array(try_block_entry.get_p_handler_array() + module_offset);
                    }

                    for (auto& state_map_entry : data_->func_info.get_ip_to_state_map_entries()) {
                        if (state_map_entry.ip) {
                            state_map_entry.ip += module_offset;
                        }
                    }

                    break;
                }
                case __gs_handler_check_eh: {
                    auto* data_ = unwind_entry_.get_custom_parameter().get_gs_handler_check_eh_parameters_data();

                    for (auto& unwind_map_entry : data_->func_info.get_unwind_map_entries()) {
                        if (unwind_map_entry.p_action) {
                            unwind_map_entry.p_action += module_offset;
                        }
                    }

                    for (auto& try_block_entry : data_->func_info.get_try_block_map_entries()) {
                        for (auto& handler_type_entry : try_block_entry.get_handler_entries()) {
                            if (handler_type_entry.p_type) {
                                handler_type_entry.p_type += module_offset;
                            }
                            if (handler_type_entry.p_address_of_handler) {
                                handler_type_entry.p_address_of_handler += module_offset;
                            }
                        }

                        try_block_entry.set_p_handler_array(try_block_entry.get_p_handler_array() + module_offset);
                    }

                    for (auto& state_map_entry : data_->func_info.get_ip_to_state_map_entries()) {
                        if (state_map_entry.ip) {
                            state_map_entry.ip += module_offset;
                        }
                    }

                    break;
                }
                }
            }

        }

        for (auto& exception_entry : ex_module->get_module_image().get_exceptions().get_exception_entries()) {
            auto temp_entry = exception_entry;
            temp_entry.set_begin_address(temp_entry.get_begin_address() + module_offset);
            temp_entry.set_end_address(temp_entry.get_end_address() + module_offset);
            temp_entry.set_unwind_data_address(temp_entry.get_unwind_data_address() + module_offset);

            main_module->get_module_image().get_exceptions().add_exception_entry(temp_entry);
        }
    }

    return shibari_linker_errors::shibari_linker_ok;
}


