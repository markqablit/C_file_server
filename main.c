#include "str.h"
#include <stdio.h>
#include <string.h>

int port_is_free(unsigned short port){
    return 1;
}

int is_dir_corect(const char* dir){
    return 1;
}

int main(int argc, char *argv[]) {
    unsigned short port = 8000;
    int tmp = 0;
    char *work_dir = "./",*current_dir = ""; //??????
    
    for (int i = 1; i < argc; ++i) {
        if (str_cmp(argv[i], "-p") || str_cmp(argv[i], "--port") || str_cmp(argv[i], "/p") || str_cmp(argv[i], "/port")) {
            if (i + 1 == argc){
                printf("error: incorect port: port not defined");
                return -1;
            }
            else if (str_to_ushort(argv[i + 1]) == -1){
                printf("error: incorect port: \'%s\'", argv[i + 1]);
                return -1;
            }
            port = str_to_ushort(argv[++i]);
            if (!port_is_free(port)){
                printf("error: incorect port: port is busy");
                return -1;
            }
        } 
        else if (str_includ(argv[i], "-p") || str_includ(argv[i], "--port") || str_includ(argv[i], "/p") || str_includ(argv[i], "/port")) {
            int flag_len = 2;
            if (str_includ(argv[i], "--port")) flag_len = 6;
            else if (str_includ(argv[i], "/port")) flag_len = 5;
            char *new_port = str_cut(argv[i], flag_len);
            if (str_to_ushort(new_port) == -1){
                printf("error: incorect port: \'%s\'", new_port);
                return -1;
            }
            port = str_to_ushort(new_port);
            free(new_port);
        } 
        else if (str_cmp(argv[i], "-d") || str_cmp(argv[i], "--dir") || str_cmp(argv[i], "/d") || str_cmp(argv[i], "/dir")) {
            if (i + 1 == argc){
                printf("error: incorect directory: directory not defined");
                return -1;
            }
            work_dir = argv[++i];
            if (!is_dir_corect(work_dir)){
                printf("error: incorect directory: \'%s\' is not correct directory",work_dir);
                return -1;
            }
        } 
        else if (str_includ(argv[i], "-d") || str_includ(argv[i], "--dir") || str_includ(argv[i], "/d") || str_includ(argv[i], "/dir")) {
            int flag_len = 2;
            if (str_includ(argv[i], "--dir")) flag_len = 5;
            else if (str_includ(argv[i], "/dir")) flag_len = 4;
            work_dir = str_cut(argv[i], flag_len);
            if (!is_dir_corect(work_dir)){
                printf("error: incorect directory: \'%s\' is not correct directory",work_dir);
                return -1;
            }
        } 
        else if (str_cmp(argv[i], "-h") || str_cmp(argv[i], "--help") || str_cmp(argv[i], "/h") || str_cmp(argv[i], "/help")){
            printf("  Using: %s \n  -p <value>\t\tdefine port\n  --port <value>\tdefine port\n  -d <directory>\tset work directory\n  --dir <directory>\tset work directory\n", argv[0]);
        }
        else {
            printf("  error: unknown argument: \'%s\'",argv[i]);
            return -1;
        }
    }
    
    printf("Server start on port %hu and with directory %s", port, work_dir);
    return 0;
}