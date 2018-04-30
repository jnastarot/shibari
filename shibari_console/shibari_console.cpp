
#include "stdafx.h"


int main(int argc, const char **argv){

    shibari shi;
    std::vector<shibari_module *> modules;
    std::string out_name = "shibari_result.exe";


     //*
    if (argc < 3) {
        printf("need more parameters !\n");
        system("PAUSE");
        return 0;
    }

    
    for (unsigned int arg_idx = 1; arg_idx < argc; arg_idx++) {

        //module=
        if (strlen(argv[arg_idx]) > 7 && !strncmp(argv[arg_idx],"module=",7)) {
            pe_image image = pe_image(std::string(argv[arg_idx]+7));
            if (image.get_image_status() == pe_image_status::pe_image_status_ok) {
                modules.push_back(new shibari_module(image));
                continue;
            }
            printf("[%s] image error!\n", std::string(argv[arg_idx] + 7).c_str());
            system("PAUSE");
            return 0;
        }
        //extname=
        if (strlen(argv[arg_idx]) > 8 && !strncmp(argv[arg_idx], "extname=", 8)) {
            if (modules.size()) {
                modules[modules.size() - 1]->get_module_exports().add_name(std::string(argv[arg_idx] + 8));
                continue;
            }
            printf("[%s] extname error!\n", std::string(argv[arg_idx] + 8).c_str());
            system("PAUSE");
            return 0;
        }
        //outname=
        if (strlen(argv[arg_idx]) > 8 && !strncmp(argv[arg_idx], "outname=", 8)) {
            out_name = std::string(argv[arg_idx] + 8);
        }
    }
    //*/

    /* 
    modules.push_back(new shibari_module(pe_image(std::string("..\\..\\app for test\\Project1.exe"))));
    modules.push_back(new shibari_module(pe_image(std::string("..\\..\\app for test\\ice9.dll"))));
    modules[modules.size() - 1]->get_module_exports().add_name("user32.dll");
    // */

    shi.set_main_module(modules[0]);
    for (unsigned int module_idx = 1; module_idx < modules.size(); module_idx++) {
        shi.add_extended_module(modules[module_idx]);
    }

    std::vector<uint8_t> out_exe;

    unsigned int exec_start_time = clock();
    switch (shi.exec_shibari(out_exe)) {

    case shibari_linker_ok: {
        printf("finished in %f\n",(clock() - exec_start_time)/1000.f);
        printf("result code: shibari_linker_ok\n");

        FILE* hTargetFile;
        fopen_s(&hTargetFile,out_name.c_str(), "wb");

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

