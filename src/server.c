#include "server.h"
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>


int check_port(unsigned short port) {
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
        int error = WSAGetLastError();// WSAEADDRINUSE = 10048 (порт занят) WSAEACCES = 10013 (нет прав на порт)
        return 0;
    }
    
    return 1; 
}