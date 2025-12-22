#include "str.h"
#include "dir.h"
#include "logger.h"
#include "server_tool.h"
#include "arg_parser.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    LDBG("PROGRAM START");
    int result = 0;
    Server_config* config = create_def_config();

    if(config == NULL){
        puts("argument create error");
        LERR("server config error");
        LDBG("PROGRAM END\n");
        return 1;
    }
    
    if (parse_arguments(argc, argv, config) != 0) {
        puts("argument parsing error");
        LERR("server config error");
        LDBG("PROGRAM END\n");
        return 1;
    }
    
    if (config->show_help) {
        print_help(argv[0]);
        free_config(config);
        LDBG("PROGRAM END\n");
        return 0;
    }
    
    if (!check_port(config->port)) {
        printf("error: port %hu is busy or inaccessible\n", config->port);
        free_config(config);
        LERR("error: port is busy");
        LDBG("PROGRAM END\n");
        return 1;
    }

    result = start_server(config);
    
    free_config(config);
    LDBG("PROGRAM END\n");
    return result;
}