#include "server_tool.h"
#include "str.h"
#include "dir.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>


int check_port(unsigned short port) {
    if(port == 0) return 0;
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in addr;
    int result;

    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        return 0;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return 0;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)port);

    result = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    
    closesocket(sock);
    WSACleanup();
    
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        return 0;
    }
    
    return 1; 
}

int start_server(Server_config* conf){
    LDBG("start_server in");

    LDBG("start_server out");
    return 0;
}






















































