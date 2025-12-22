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

typedef struct {
    SOCKET client_socket;
    char work_dir[PATH_LEN];
    unsigned int timeout;
} WorkerData;

typedef struct {
    SOCKET sockets[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    HANDLE mutex;
    HANDLE semaphore;
} ConnectionQueue;

int check_port(unsigned short port);
int start_server(Server_config* config);
ConnectionQueue* init_queue();
void queue_add_connection(ConnectionQueue* queue, SOCKET socket);
SOCKET queue_pop_connection(ConnectionQueue* queue);
void destroy_queue(ConnectionQueue* queue);
DWORD WINAPI worker_process(LPVOID param);
int parse_http_request(const char* request, char* method, char* path, char* version, long* content_length, char* content_type, char* boundary);
int is_path_safe(const char* requested_path, const char* work_dir);
const char* get_mime_type(const char* filename);
void handle_request(SOCKET client_socket, const char* work_dir, unsigned int timeout);
void send_response(SOCKET client_socket, int status_code, const char* status_text, const char* content_type, const char* body, long body_length);
void send_file_response(SOCKET client_socket, const char* file_path);
char* generate_directory_listing(const char* dir_path, const char* request_path);
void handle_get(SOCKET client_socket, const char* path, const char* work_dir);
void handle_delete(SOCKET client_socket, const char* path, const char* work_dir);
void handle_post(SOCKET client_socket, const char* path, const char* work_dir, const char* request_data, long content_length, const char* content_type);
void handle_mkdir(SOCKET client_socket, const char* path, const char* work_dir);


#endif