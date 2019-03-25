#pragma once


#define MSVC_GET_RESULT_OFFSET(fixed_data) (\
    ( (fixed_data) + extended_module->get_module_position().get_address_offset()) + (main_module->get_image().is_x32_image() ? main_module->get_image().get_image_base() : 0) \
)

#define MSVC_RTTI_FIX_OFFSET(fix_rva, fixed_data) {\
    uint32_t fixed_data_ = uint32_t(MSVC_GET_RESULT_OFFSET(fixed_data)); \
    image_io.set_image_offset( uint32_t(fix_rva) + extended_module->get_module_position().get_address_offset()).write(&fixed_data_, sizeof(fixed_data_)); \
}

#define MSVC_RTTI_FIX_ABSOLUTE64(fix_rva, fixed_data) {\
    uint64_t fixed_data_ = main_module->get_image().get_image_base() + (fixed_data) + extended_module->get_module_position().get_address_offset(); \
    image_io.set_image_offset( uint32_t(fix_rva) + extended_module->get_module_position().get_address_offset()).write(&fixed_data_, sizeof(fixed_data_)); \
}



void msvc_shift_module_rtti(shibari_module* extended_module, shibari_module* main_module) {

    auto& msvc_rtti = extended_module->get_rtti();

    pe_image_io image_io(main_module->get_image());
    
    
    for (auto& comp_obj_loc : msvc_rtti.complete_object_locator_entries) {

        if (comp_obj_loc.second.get_class_descriptor_addr_rva()) {
            MSVC_RTTI_FIX_OFFSET(
                comp_obj_loc.first + offsetof(msvc_rtti_complete_object_locator, type_descriptor_addr),
                comp_obj_loc.second.get_class_descriptor_addr_rva()
            )
        }

        if (comp_obj_loc.second.get_type_descriptor_addr_rva()) {
            MSVC_RTTI_FIX_OFFSET(
                comp_obj_loc.first + offsetof(msvc_rtti_complete_object_locator, class_descriptor_addr),
                comp_obj_loc.second.get_type_descriptor_addr_rva()
            )
        }

        if (!main_module->get_image().is_x32_image()) {
            
            if (comp_obj_loc.second.get_object_base_rva()) {
                MSVC_RTTI_FIX_OFFSET(
                    comp_obj_loc.first + offsetof(msvc_rtti_complete_object_locator, object_base),
                    comp_obj_loc.second.get_object_base_rva()
                )
            }
        }
    }

    for (auto& class_hier_entry : msvc_rtti.class_hierarchy_descriptor_entries) {

        if (class_hier_entry.second.get_base_class_array_addr_rva()) {
            MSVC_RTTI_FIX_OFFSET(
                class_hier_entry.first + offsetof(msvc_rtti_class_hierarchy_descriptor, base_class_array_addr),
                class_hier_entry.second.get_base_class_array_addr_rva()
            )


            for (size_t base_entry_idx = 0; base_entry_idx < class_hier_entry.second.get_num_base_classes(); base_entry_idx++) {
                
                MSVC_RTTI_FIX_OFFSET(
                    class_hier_entry.second.get_base_class_array_addr_rva() + sizeof(uint32_t) * base_entry_idx,
                    (class_hier_entry.second.get_base_class_entries())[base_entry_idx]
                )           
            }
        }
    }


    for (auto& base_entry : msvc_rtti.base_class_descriptor_entries) {

        if (base_entry.second.get_type_descriptor_addr_rva()) {
            MSVC_RTTI_FIX_OFFSET(
                base_entry.first + offsetof(msvc_rtti_base_class_descriptor, type_descriptor_addr),
                base_entry.second.get_type_descriptor_addr_rva()
            )
        }

        if (base_entry.second.get_hierarchy_descriptor_ref()) {
            MSVC_RTTI_FIX_OFFSET(
                base_entry.first + offsetof(msvc_rtti_base_class_descriptor, hierarchy_descriptor_ref),
                base_entry.second.get_hierarchy_descriptor_ref()
            )
        }
    }

    if (main_module->get_image().is_x32_image()) {
        for (auto& type_entry : msvc_rtti.type_descriptor_entries) {

            if (type_entry.second.get_vtable_addr_rva()) {
                MSVC_RTTI_FIX_OFFSET(
                    type_entry.first + offsetof(msvc_rtti_32_type_descriptor, vtable_addr),
                    type_entry.second.get_vtable_addr_rva()
                )
            }

            if (type_entry.second.get_spare_rva()) {
                MSVC_RTTI_FIX_OFFSET(
                    type_entry.first + offsetof(msvc_rtti_32_type_descriptor, spare),
                    type_entry.second.get_spare_rva()
                )
            }
        }
    }
    else {

        for (auto& type_entry : msvc_rtti.type_descriptor_entries) {

            if (type_entry.second.get_vtable_addr_rva()) {
                MSVC_RTTI_FIX_ABSOLUTE64(
                    type_entry.first + offsetof(msvc_rtti_64_type_descriptor, vtable_addr),
                    type_entry.second.get_vtable_addr_rva()
                )
            }

            if (type_entry.second.get_spare_rva()) {
                MSVC_RTTI_FIX_ABSOLUTE64(
                    type_entry.first + offsetof(msvc_rtti_64_type_descriptor, spare),
                    type_entry.second.get_spare_rva()
                )
            }
        }
    }
}
