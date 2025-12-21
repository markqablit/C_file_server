#include "arg_parser.h"
#include "str.h"
#include "dir.h"
#include <stdio.h>
#include <stdlib.h>

Server_config* create_def_config(){
    LDBG("create_def_config in");
    Server_config* config = malloc(sizeof(Server_config));
    if (!config){
        puts("error: memory allocation failed");
        LERR("memory allocation failed");
        return NULL;
    } 
    config->port = 8000;
    config->work_dir = NULL;
    config->show_help = 0;
    config->timeout = 30; 
    char* current_dir, *buff = malloc(PATH_LEN);
    if (!buff){
        puts("error: memory allocation failed");
        LERR("memory allocation failed");
        return NULL;
    } 
    get_current_dir(buff, PATH_LEN - 1);
    current_dir = str_sum(buff,"\\",PATH_LEN);
    free(buff);
    if (!current_dir){
        puts("error: memory allocation failed");
        LERR("memory allocation failed");
        return NULL;
    }
    config->work_dir = current_dir;
    LDBG("create_def_config out");
    return config;
}

int parse_arguments(int argc, char* argv[], Server_config* config) {  
    LDBG("parse_arguments in");
    unsigned char port_flag = 1, dir_flag = 1, help_flag = 1, timeout_flag = 1;

    for (unsigned char i = 1; i < argc; ++i) {
        if ((str_cmp(argv[i], "-p") || str_cmp(argv[i], "--port") || str_cmp(argv[i], "/p") || str_cmp(argv[i], "/port")) && port_flag) {
            if (i + 1 == argc) {
                puts("error: incorrect port: port not defined");
                LERR("error: incorrect port: port not defined");
                LDBG("parse_arguments out");
                return 1;
            }
            int port_val = str_to_ushort(argv[i + 1]);
            if (port_val == -1 || port_val == 0) {
                printf("error: incorrect port: '%s'\n", argv[i + 1]);
                LERR("error: incorrect port");
                LDBG("parse_arguments out");
                return 1;
            }
            config->port = (unsigned short)port_val;
            port_flag = 0;
            ++i;
        }
        else if ((str_includ(argv[i], "-p") || str_includ(argv[i], "--port") || str_includ(argv[i], "/p") || str_includ(argv[i], "/port")) && port_flag) {
            int flag_len = 2;
            if (str_includ(argv[i], "--port")) flag_len = 6;
            else if (str_includ(argv[i], "/port")) flag_len = 5;
            char* port_str = str_cut(argv[i], flag_len);
            if (!port_str) {
                puts("error: incorrect port format");
                LERR("error: incorrect port format");
                LDBG("parse_arguments out");
                return 1;
            }
            int port_val = str_to_ushort(port_str);
            free(port_str);
            if (port_val == -1 || port_val == 0) {
                puts("error: incorrect port value");
                LERR("error: incorrect port value");
                LDBG("parse_arguments out");
                return 1;
            }
            config->port = (unsigned short)port_val;
            port_flag = 0;
        }
        else if ((str_cmp(argv[i], "-d") || str_cmp(argv[i], "--dir") || str_cmp(argv[i], "/d") || str_cmp(argv[i], "/dir")) && dir_flag) {
            if (i + 1 == argc) {
                puts("error: incorrect directory: directory not defined");
                LERR("error: incorrect directory: directory not defined");
                LDBG("parse_arguments out");
                return 1;
            }
            char* full_path = str_sum(config->work_dir, argv[++i], PATH_LEN);
            if (!full_path) {
                puts("error: memory allocation failed");
                LERR("error: memory allocation failed");
                LDBG("parse_arguments out");
                return 1;
            }
            
            if (is_dir_exists(full_path)) {
                free(config->work_dir);
                config->work_dir = full_path;
            }
            else if (is_dir_exists(argv[i])) {
                free(config->work_dir);
                config->work_dir = malloc(str_len(argv[i]) + 2);
                if (config->work_dir) {
                    for (int j = 0; argv[i][j] != '\0'; ++j) {
                        config->work_dir[j] = argv[i][j];
                    }
                    config->work_dir[str_len(argv[i]) - 1] = '\\';
                    config->work_dir[str_len(argv[i])] = '\0';
                }
                free(full_path);
            }
            else {
                printf("error: incorrect directory: '%s' is not a valid directory\n", argv[i]);
                free(full_path);
                LERR("error: incorrect directory: not a valid directory")
                LDBG("parse_arguments out");
                return 1;
            }
            dir_flag = 0;
        }
        else if ((str_includ(argv[i], "-d") || str_includ(argv[i], "--dir") || str_includ(argv[i], "/d") || str_includ(argv[i], "/dir")) && dir_flag) {
            int flag_len = 2;
            if (str_includ(argv[i], "--dir")) flag_len = 5;
            else if (str_includ(argv[i], "/dir")) flag_len = 4;
            
            char* dir_path = str_cut(argv[i], flag_len);
            if (!dir_path) {
                puts("error: incorrect directory format");
                LERR("error: incorrect directory format");
                LDBG("parse_arguments out");
                return 1;
            }
            
            char* full_path = str_sum(config->work_dir, dir_path, PATH_LEN);
            if (!full_path) {
                free(dir_path);
                LERR("memory allocate error")
                LDBG("parse_arguments out");
                return 1;
            }
            
            if (is_dir_exists(full_path)) {
                free(config->work_dir);
                config->work_dir = full_path;
                free(dir_path);
            }
            else if (is_dir_exists(dir_path)) {
                free(config->work_dir);
                config->work_dir = dir_path;
                free(full_path);
            }
            else {
                printf("error: incorrect directory: '%s' is not a valid directory\n", dir_path);
                free(dir_path);
                free(full_path);
                LERR("error: incorrect directory: not a valid director");
                LDBG("parse_arguments out");
                return 1;
            }
            dir_flag = 0;
        }
        else if ((str_cmp(argv[i], "-t") || str_cmp(argv[i], "--timeout") || str_cmp(argv[i], "/t") || str_cmp(argv[i], "/timeout")) && timeout_flag) {
            if (i + 1 == argc) {
                puts("error: incorrect timeout: timeout not defined");
                LERR("error: incorrect timeout: timeout not defined");
                LDBG("parse_arguments out");
                return 1;
            }
            int timeout_val = str_to_ushort(argv[i + 1]);
            if (timeout_val <= 0) {
                printf("error: incorrect timeout: '%s'\n", argv[i + 1]);
                LERR("error: oncorect timeout");
                LDBG("parse_arguments out");
                return 1;
            }
            config->timeout = (unsigned short)timeout_val;
            timeout_flag = 0;
            ++i;
        }
        else if ((str_cmp(argv[i], "-h") || str_cmp(argv[i], "--help") || str_cmp(argv[i], "/h") || str_cmp(argv[i], "/help")) && help_flag) {
            config->show_help = 1;
            help_flag = 0;
        }
        else {
            printf("error: unknown argument: '%s'\n", argv[i]);
            LERR("error: unknown argument");
            LDBG("parse_arguments out");
            return 1;
        }
    }
    
    normalize_dir(config->work_dir);
    LDBG("parse_arguments out");
    return 0;
}

void free_config(Server_config* config) {
    LDBG("free_config in");
    if (config && config->work_dir) {
        free(config->work_dir);
        config->work_dir = NULL;
    }
    LDBG("free_config out");
}

void print_help(const char* program_name) {
    LDBG("print_help in");
    printf("Usage: %s\n", program_name);
    puts("Options:");
    puts("  -p, --port PORT    Set server port (default: 8000)");
    puts("  -d, --dir DIR      Set working directory (default: current)");
    puts("  -t, --timeout SEC  Set request timeout in seconds (default: 30)");
    puts("  -h, --help         Show this help message\n");
    LDBG("print_help out");
}