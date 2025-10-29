#include "str.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    unsigned short port = 8000;
    char *directory = "./";
    
    for (int i = 1; i < argc; ++i) {
        if (((strcmp(argv[i], "-p") == 0 ) || (strcmp(argv[i], "--port") == 0 )) && i + 1 < argc) {
            if (1023 < atoi(argv[i + 1]) && atoi(argv[i + 1]) < 49152) port = atoi(argv[++i]);
            else printf("error: incorect port: \'%d\'", atoi(argv[i + 1]));
        } 
        else if ((strcmp(argv[i], "-d") == 0) || (strcmp(argv[i], "--dir") == 0) && i + 1 < argc) {
            directory = argv[++i];

        } 
        else if ( str_cmp(argv[i], "-h") || str_cmp(argv[i], "--help") || str_cmp(argv[i], "/h") || str_cmp(argv[i], "/help")){
            printf("  Using: %s \n  -p <value>\t\tdefine port\n  --port <value>\tdefine port\n  -d <directory>\tset work directory\n  --dir <directory>\tset work directory\n", argv[0]);
        }
        else {
            printf("error: unknown argument: \'%s\'",argv[i]);
            return -1;
        }
    }
    
    printf("Server start on port %hu and with directory %s", port, directory);
    return 0;
}