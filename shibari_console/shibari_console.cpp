
#include "stdafx.h"


int main(){

    shibari shi;
    std::vector<shibari_module *> modules;
    std::string out_name = "shibari_result.exe";


    //* 
    modules.push_back(new shibari_module(pe_image(std::string("..\\..\\app for test\\Project1.exe"))));
    modules.push_back(new shibari_module(pe_image(std::string("..\\..\\app for test\\ice9.dll"))));
    // */
    shi.set_main_module(modules[0]);
    for (unsigned int module_idx = 1; module_idx < modules.size(); module_idx++) {
        shi.add_extended_module(modules[module_idx]);
    }

    std::vector<uint8_t> out_exe;

    switch (shi.exec_shibari(out_exe)) {

    case shibari_linker_ok: {
        printf("result code: shibari_linker_ok\n");

        FILE* hTargetFile;
        fopen_s(&hTargetFile,out_name.c_str(), "w");

        if (hTargetFile) {
            fwrite(out_exe.data(), out_exe.size(), 1, hTargetFile);
            fclose(hTargetFile);
        }
        break;
    }
    case shibari_linker_error_bad_input: {
        printf("result code: shibari_linker_error_bad_input\n");
        break;
    }
    case shibari_linker_error_export_linking: {
        printf("result code: shibari_linker_error_export_linking\n");
        break;
    }
    case shibari_linker_error_import_linking: {
        printf("result code: shibari_linker_error_import_linking\n");
        break;
    }
    case shibari_linker_error_loadconfig_linking: {
        printf("result code: shibari_linker_error_loadconfig_linking\n");
        break;
    }
    case shibari_linker_error_relocation_linking: {
        printf("result code: shibari_linker_error_relocation_linking\n");
        break;
    }
    case shibari_linker_error_exceptions_linking: {
        printf("result code: shibari_linker_error_exceptions_linking\n");
        break;
    }
    case shibari_linker_error_tls_linking: {
        printf("result code: shibari_linker_error_tls_linking\n");
        break;
    }

    default:
        break;
    }


    system("PAUSE");
    return 0;
}

