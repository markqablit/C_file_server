#include "str.h"
#include "dir.h"
#include <stdio.h>
#include <stdlib.h>

int port_is_free(unsigned short port){
    return 1;
}

int main(int argc, char *argv[]) {

    const unsigned int PATH_LEN = 1024;
    unsigned char dir_flag = 1, help_flag = 1, port_flag = 1;
    unsigned short port = 8000;
    char *work_dir, *current_dir, *buff = malloc(PATH_LEN);

    get_current_dir(buff, PATH_LEN - 1);
    current_dir = str_sum(buff,"\\",PATH_LEN);
    free(buff);
    work_dir = current_dir;

    for (int i = 1; i < argc; ++i) {
        if ((str_cmp(argv[i], "-p") || str_cmp(argv[i], "--port") || str_cmp(argv[i], "/p") || str_cmp(argv[i], "/port")) && port_flag) {
            if (i + 1 == argc){
                printf("error: incorect port: port not defined");
                return -1;
            }
            else if (str_to_ushort(argv[i + 1]) == -1){
                printf("error: incorect port: \'%s\'", argv[i + 1]);
                return -1;
            }
            port = str_to_ushort(argv[++i]);
            port_flag = 0;
            if (!port_is_free(port)){
                printf("error: incorect port: port is busy");
                return -1;
            }
        } 
        else if ((str_includ(argv[i], "-p") || str_includ(argv[i], "--port") || str_includ(argv[i], "/p") || str_includ(argv[i], "/port")) && port_flag) {
            int flag_len = 2;
            if (str_includ(argv[i], "--port")) flag_len = 6;
            else if (str_includ(argv[i], "/port")) flag_len = 5;
            char *new_port = str_cut(argv[i], flag_len);
            if (str_to_ushort(new_port) == -1){
                printf("error: incorect port: \'%s\'", new_port);
                return -1;
            }
            port = str_to_ushort(new_port);
            port_flag = 0;
            free(new_port);
        } 
        else if ((str_cmp(argv[i], "-d") || str_cmp(argv[i], "--dir") || str_cmp(argv[i], "/d") || str_cmp(argv[i], "/dir")) && dir_flag) {
            if (i + 1 == argc){
                printf("error: incorect directory: directory not defined");
                return -1;
            }
            dir_flag = 0;
            char *full_path = str_sum(current_dir, argv[++i], PATH_LEN);
            if(is_dir_exists(full_path)){
                work_dir = full_path;
            }
            else if (is_dir_exists(argv[i])){
                work_dir = argv[i];
                free(full_path);
            }
            else {
                printf("error: incorect directory: \'%s\' is not correct directory", argv[i]);
                return -1;
            }
        } 
        else if ((str_includ(argv[i], "-d") || str_includ(argv[i], "--dir") || str_includ(argv[i], "/d") || str_includ(argv[i], "/dir")) && dir_flag) {
            int flag_len = 2;
            if (str_includ(argv[i], "--dir")) flag_len = 5;
            else if (str_includ(argv[i], "/dir")) flag_len = 4;
            dir_flag = 0;
            buff = str_cut(argv[i], flag_len);
            char *full_path = str_sum(current_dir, buff, PATH_LEN);
            if(is_dir_exists(full_path)){
                work_dir = full_path;
                free(buff);
            }
            else if (is_dir_exists(buff)){
                work_dir = buff;
                free(full_path);
            }
            else{
                printf("error: incorect directory: \'%s\' is not correct directory", buff);
                return -1;
            }
        } 
        else if ((str_cmp(argv[i], "-h") || str_cmp(argv[i], "--help") || str_cmp(argv[i], "/h") || str_cmp(argv[i], "/help")) && help_flag){
            help_flag = 0;
            printf("Using: %s \n  -p <value>\t\tdefine port\n  --port <value>\tdefine port\n  -d <directory>\tset work directory\n  --dir <directory>\tset work directory\n", argv[0]);
        }
        else {
            printf("error: unknown argument: \'%s\'",argv[i]);
            return -1;
        }
    }
    
    printf("Server start on port %hu and with directory %s", port, work_dir);
    return 0;
}