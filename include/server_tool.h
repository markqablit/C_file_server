#ifndef SERVER_TOOL_H
#define SERVER_TOOL_H

#include "arg_parser.h"
#include "logger.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
#endif

#define MAX_WORKERS 10
#define MIN_WORKERS 3
#define MAX_QUEUE_SIZE 100

int check_port(unsigned short port);

int start_server(Server_config* config);

#endif