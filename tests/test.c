#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

int check_port(int port) {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in addr;
    
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
    addr.sin_port = htons(port);
    
    int result = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    
    closesocket(sock);
    WSACleanup();
    
    return (result == 0);
}

int main() {
    int test_ports[] = {80, 443, 8080, 3000, 5000, 0};
    
    printf("Port chaeck:\n");
    for (int i = 0; i < 64000; i++) {
        int port = i;
        if (check_port(port)) {
            //printf("Порт %d: СВОБОДЕН ✓\n", port);
        } else {
            printf("Port %d: doesnt free ✗\n", port);
        }
    }
    
    return 0;
}