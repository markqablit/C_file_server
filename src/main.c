#include "str.h"
#include "dir.h"
#include "server.h"
#include "arg_parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    ServerConfig* config = create_def_config();
    
    if (parse_arguments(argc, argv, config) != 0) {
        puts("Fatal error");
        return -1;
    }
    
    if (config->show_help) {
        print_help(argv[0]);
        free_config(config);
        return 0;
    }
    
    if (!check_port(config->port)) {
        printf("error: port %hu is busy or inaccessible\n", config->port);
        free_config(config);
        return -1;
    }

    printf("Server start on port %hu, with directory %s and timeout %hu second\n", config->port, config->work_dir, config->timeout);
    return 0;
}