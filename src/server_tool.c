#include "server_tool.h"
#include "str.h"
#include "dir.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <io.h>
#include <direct.h>


static int worker_count = 0;
static volatile BOOL server_running = TRUE;
static ConnectionQueue* connection_queue = NULL;
static HANDLE worker_handles[MAX_WORKERS];

int check_port(unsigned short port) {
    LDBG("check_port in");
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
    LDBG("check_port out");
    return 1; 
}

ConnectionQueue* init_queue() {
    ConnectionQueue* queue = malloc(sizeof(ConnectionQueue));
    if (!queue) return NULL;
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    queue->mutex = CreateMutex(NULL, FALSE, NULL);
    if (!queue->mutex) {
        free(queue);
        return NULL;
    }
    queue->semaphore = CreateSemaphore(NULL, 0, MAX_QUEUE_SIZE, NULL);
    if (!queue->semaphore) {
        CloseHandle(queue->mutex);
        free(queue);
        return NULL;
    }
    return queue;
}

void queue_add_connection(ConnectionQueue* queue, SOCKET socket) {
    WaitForSingleObject(queue->mutex, INFINITE);
    
    if (queue->count < MAX_QUEUE_SIZE) {
        queue->sockets[queue->rear] = socket;
        queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
        queue->count++;
        
        ReleaseSemaphore(queue->semaphore, 1, NULL);
    } else {
        closesocket(socket);
    }
    
    ReleaseMutex(queue->mutex);
}

SOCKET queue_erase_connection(ConnectionQueue* queue) {
    WaitForSingleObject(queue->semaphore, INFINITE);
    WaitForSingleObject(queue->mutex, INFINITE);
    
    SOCKET socket = queue->sockets[queue->front];
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
    queue->count--;
    
    ReleaseMutex(queue->mutex);
    return socket;
}

void destroy_queue(ConnectionQueue* queue) {
    if (!queue) return;
    
    while (queue->count > 0) {
        SOCKET socket = queue_erase_connection(queue);
        closesocket(socket);
    }
    
    CloseHandle(queue->mutex);
    CloseHandle(queue->semaphore);
    free(queue);
    return;
}

DWORD WINAPI worker_process(LPVOID param) {
    WorkerData* worker_data = (WorkerData*)param;
    
    while (server_running) {
        SOCKET client_socket = queue_erase_connection(connection_queue);
        if (client_socket != INVALID_SOCKET) {
            handle_request(client_socket, worker_data->work_dir, worker_data->timeout);
        }
    }
    
    free(worker_data);
    return 0;
}

int parse_http_request(const char* request, char* method, char* path, char* version, long* content_length, char* content_type) {
    char first_line[1024];
    int i = 0;
    
    while (request[i] != '\r' && request[i] != '\n' && i < 1023) {
        first_line[i] = request[i];
        i++;
    }
    first_line[i] = '\0';
    
    if (sscanf(first_line, "%15s %1023s %15s", method, path, version) != 3) {
        return 0;
    }
    
    *content_length = 0;
    content_type[0] = '\0';
    
    const char* ptr = request + i;
    while (*ptr && *ptr != '\r' && *(ptr + 1) != '\n') {
        char header[256];
        char value[256];
        
        if (sscanf(ptr, "%255[^:]: %255[^\r\n]", header, value) == 2) {
            if (str_cmp(header, "Content-Length")) {
                *content_length = atol(value);
            } else if (str_cmp(header, "Content-Type")) {
                str_copy(content_type, value, 255);
            }
        }
        
        while (*ptr && *ptr != '\n') ++ptr;
        if (*ptr) ptr++;
    }
    
    return 1;
}

int is_path_safe(const char* requested_path, const char* work_dir) {
    char full_path[2048];
    
    if (requested_path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", work_dir, requested_path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", work_dir, requested_path);
    }
    
    str_replace(full_path, '/', '\\');
    
    char normalized_path[2048];
    char* components[256];
    int component_count = 0;
    
    char path_copy[2048];
    str_copy(path_copy, full_path, sizeof(path_copy));
    
    char* token = str_parse(path_copy, "\\");
    while (token) {
        if (str_cmp(token, "..")) {
            if (component_count > 0) {
                component_count--;
            }
        } else if (!str_cmp(token, ".") && str_len(token) > 0) {
            components[component_count++] = token;
        }
        token = str_parse(NULL, "\\");
    }
    
    normalized_path[0] = '\0';
    for (int i = 0; i < component_count; i++) {
        strcat(normalized_path, components[i]);
        if (i < component_count - 1) {
            strcat(normalized_path, "\\");
        }
    }
    
    char work_dir_normalized[1024];
    str_copy(work_dir_normalized, work_dir, sizeof(work_dir_normalized));
    str_replace(work_dir_normalized, '/', '\\');
    
    int work_len = str_len(work_dir_normalized);
    if (work_len > 0 && work_dir_normalized[work_len - 1] == '\\') {
        work_dir_normalized[work_len - 1] = '\0';
    }
    
    char final_path[2048];
    snprintf(final_path, sizeof(final_path), "%s\\%s", work_dir_normalized, normalized_path);
    
    return str_include(final_path, work_dir_normalized);
}

const char* get_mime_type(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    
    ext++;
    
    if (str_cmp(ext, "html") || str_cmp(ext, "htm")) {
        return "text/html; charset=utf-8";
    } else if (str_cmp(ext, "css")) {
        return "text/css; charset=utf-8";
    } else if (str_cmp(ext, "js")) {
        return "application/javascript; charset=utf-8";
    } else if (str_cmp(ext, "txt") || str_cmp(ext, "log")) {
        return "text/plain; charset=utf-8";
    } else if (str_cmp(ext, "json")) {
        return "application/json";
    } else if (str_cmp(ext, "xml")) {
        return "application/xml";
    } else if (str_cmp(ext, "jpg") || str_cmp(ext, "jpeg")) {
        return "image/jpeg";
    } else if (str_cmp(ext, "png")) {
        return "image/png";
    } else if (str_cmp(ext, "gif")) {
        return "image/gif";
    } else if (str_cmp(ext, "pdf")) {
        return "application/pdf";
    } else {
        return "application/octet-stream";
    }
}

void send_response(SOCKET client_socket, int status_code, const char* status_text, const char* content_type, const char* body, long body_length) {
    char headers[512];
    int headers_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, content_type, body_length);
    
    send(client_socket, headers, headers_len, 0);
    
    if (body && body_length > 0) {
        send(client_socket, body, body_length, 0);
    }
}

void send_file_response(SOCKET client_socket, const char* file_path, const char* mime_type) {
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        send_response(client_socket, 404, "Not Found", "text/plain", 
                     "404 Not Found", 12);
        return;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char headers[512];
    int headers_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n",
        mime_type, file_size);
    
    send(client_socket, headers, headers_len, 0);
    
    char buffer[8192];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }
    
    fclose(file);
}

char* generate_directory_listing(const char* dir_path, const char* request_path) {
    WIN32_FIND_DATA find_data;
    char search_path[2048];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);
    
    HANDLE hFind = FindFirstFile(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    
    char* json = (char*)malloc(65536);
    if (!json) {
        FindClose(hFind);
        return NULL;
    }
    
    int pos = snprintf(json, 65536, "{\n  \"path\": \"%s\",\n  \"files\": [\n", request_path);
    
    int first_file = 1;
    do {
        if (str_cmp(find_data.cFileName, ".") || str_cmp(find_data.cFileName, "..")) {
            continue;
        }
        
        if (!first_file) {
            pos += snprintf(json + pos, 65536 - pos, ",\n");
        }
        first_file = 0;
        
        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);
        
        LARGE_INTEGER file_size;
        file_size.LowPart = find_data.nFileSizeLow;
        file_size.HighPart = find_data.nFileSizeHigh;
        
        FILETIME ft = find_data.ftLastWriteTime;
        SYSTEMTIME st;
        FileTimeToSystemTime(&ft, &st);
        
        char type[16];
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            str_copy(type, "directory", sizeof(type));
        } else {
            str_copy(type, "file", sizeof(type));
        }
        
        const char* mime_type = get_mime_type(find_data.cFileName);
        
        pos += snprintf(json + pos, 65536 - pos,
            "    {\n"
            "      \"name\": \"%s\",\n"
            "      \"type\": \"%s\",\n"
            "      \"size\": %lld,\n"
            "      \"modified\": \"%04d-%02d-%02dT%02d:%02d:%02d\",\n"
            "      \"mime_type\": \"%s\"\n"
            "    }",
            find_data.cFileName,
            type,
            file_size.QuadPart,
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond,
            mime_type);
        
    } while (FindNextFile(hFind, &find_data));
    
    FindClose(hFind);
    
    pos += snprintf(json + pos, 65536 - pos, "\n  ]\n}\n");
    
    return json;
}


void handle_get(SOCKET client_socket, const char* path, const char* work_dir) {
    char full_path[2048];
    
    if (path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", work_dir, path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", work_dir, path);
    }
    
    str_replace(full_path, '/', '\\');
    
    if (str_len(path) == 0 || str_cmp(path, "/")) {
        char* listing = generate_directory_listing(work_dir, "/");
        if (listing) {
            send_response(client_socket, 200, "OK", "application/json",
                         listing, str_len(listing));
            free(listing);
        } else {
            send_response(client_socket, 500, "Internal Server Error", "text/plain",
                         "500 Internal Server Error", 24);
        }
        return;
    }
    
    DWORD attrib = GetFileAttributes(full_path);
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        send_response(client_socket, 404, "Not Found", "text/plain", 
                     "404 Not Found", 12);
        return;
    }
    
    if (_access(full_path, 4) != 0) {
        send_response(client_socket, 403, "Forbidden", "text/plain",
                     "403 Forbidden", 13);
        return;
    }
    
    if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
        char* listing = generate_directory_listing(full_path, path);
        if (listing) {
            send_response(client_socket, 200, "OK", "application/json",
                         listing, str_len(listing));
            free(listing);
        } else {
            send_response(client_socket, 500, "Internal Server Error", "text/plain",
                         "500 Internal Server Error", 24);
        }
        return;
    }
    
    const char* mime_type = get_mime_type(full_path);
    send_file_response(client_socket, full_path, mime_type);
}

void handle_delete(SOCKET client_socket, const char* path, const char* work_dir) {
    char full_path[2048];
    
    if (path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", work_dir, path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", work_dir, path);
    }
    
    str_replace(full_path, '/', '\\');
    
    if (GetFileAttributes(full_path) == INVALID_FILE_ATTRIBUTES) {
        send_response(client_socket, 404, "Not Found", "text/plain",
                     "404 Not Found", 12);
        return;
    }
    
    if (_access(full_path, 2) != 0) {
        send_response(client_socket, 403, "Forbidden", "text/plain",
                     "403 Forbidden", 13);
        return;
    }
    
    if (remove(full_path) == 0) {
        char response[256];
        int len = snprintf(response, sizeof(response), 
                          "{\"status\": \"success\", \"message\": \"File deleted: %s\"}", path);
        send_response(client_socket, 200, "OK", "application/json", response, len);
    } else {
        send_response(client_socket, 500, "Internal Server Error", "text/plain",
                     "500 Internal Server Error", 24);
    }
}

void handle_post(SOCKET client_socket, const char* path, const char* work_dir, 
                 const char* request_data, long content_length, const char* content_type) {
    char full_path[2048];
    
    if (path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", work_dir, path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", work_dir, path);
    }
    
    str_replace(full_path, '/', '\\');
    
    int is_directory = (path[str_len(path) - 1] == '/');
    
    if (is_directory) {
        // Ищем имя файла в Content-Disposition
        const char* filename_start = strstr(request_data, "filename=\"");
        char filename[256] = {0};
        
        if (filename_start) {
            filename_start += 10;
            int i = 0;
            while (*filename_start != '"' && i < 255) {
                filename[i++] = *filename_start++;
            }
            filename[i] = '\0';
        } else {
            time_t now = time(NULL);
            snprintf(filename, sizeof(filename), "upload_%lld.bin", (long long)now);
        }
        
        char file_path[2048];
        snprintf(file_path, sizeof(file_path), "%s%s", full_path, filename);
        
        if (GetFileAttributes(file_path) != INVALID_FILE_ATTRIBUTES) {
            send_response(client_socket, 409, "Conflict", "text/plain",
                         "409 Conflict - File already exists", 35);
            return;
        }
        
        const char* data_start = strstr(request_data, "\r\n\r\n");
        if (!data_start) {
            send_response(client_socket, 400, "Bad Request", "text/plain",
                         "400 Bad Request - No data found", 31);
            return;
        }
        
        data_start += 4;
        const char* data_end = request_data + content_length;
        
        FILE* file = fopen(file_path, "wb");
        if (!file) {
            send_response(client_socket, 500, "Internal Server Error", "text/plain",
                         "500 Internal Server Error - Cannot create file", 48);
            return;
        }
        
        fwrite(data_start, 1, data_end - data_start, file);
        fclose(file);
        
        char response[512];
        int len = snprintf(response, sizeof(response),
                          "{\"status\": \"success\", \"message\": \"File uploaded\", \"filename\": \"%s\", \"size\": %ld}",
                          filename, (long)(data_end - data_start));
        send_response(client_socket, 201, "Created", "application/json", response, len);
        
    } else {
        if (GetFileAttributes(full_path) != INVALID_FILE_ATTRIBUTES) {
            send_response(client_socket, 409, "Conflict", "text/plain",
                         "409 Conflict - File already exists", 35);
            return;
        }
        
        const char* data_start = strstr(request_data, "\r\n\r\n");
        if (!data_start) {
            data_start = request_data;
        } else {
            data_start += 4;
        }
        
        const char* data_end = request_data + content_length;
        
        FILE* file = fopen(full_path, "wb");
        if (!file) {
            send_response(client_socket, 500, "Internal Server Error", "text/plain",
                         "500 Internal Server Error - Cannot create file", 48);
            return;
        }
        
        fwrite(data_start, 1, data_end - data_start, file);
        fclose(file);
        
        char response[512];
        int len = snprintf(response, sizeof(response),
                          "{\"status\": \"success\", \"message\": \"File created\", \"path\": \"%s\", \"size\": %ld}",
                          path, (long)(data_end - data_start));
        send_response(client_socket, 201, "Created", "application/json", response, len);
    }
}

void handle_mkcol(SOCKET client_socket, const char* path, const char* work_dir) {
    char full_path[2048];
    
    if (path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", work_dir, path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", work_dir, path);
    }
    
    str_replace(full_path, '/', '\\');
    
    int len = str_len(full_path);
    if (len > 0 && full_path[len - 1] == '\\') {
        full_path[len - 1] = '\0';
    }
    
    if (GetFileAttributes(full_path) != INVALID_FILE_ATTRIBUTES) {
        send_response(client_socket, 409, "Conflict", "text/plain",
                     "409 Conflict - Directory already exists", 40);
        return;
    }
    
    if (_mkdir(full_path) == 0) {
        char response[256];
        int len = snprintf(response, sizeof(response),
                          "{\"status\": \"success\", \"message\": \"Directory created: %s\"}", path);
        send_response(client_socket, 201, "Created", "application/json", response, len);
    } else {
        send_response(client_socket, 500, "Internal Server Error", "text/plain",
                     "500 Internal Server Error - Cannot create directory", 52);
    }
}

void handle_request(SOCKET client_socket, const char* work_dir, unsigned int timeout) {
    DWORD timeout_ms = timeout * 1000;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
    
    char buffer[65536];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        send_response(client_socket, 408, "Request Timeout", "text/plain",
                     "408 Request Timeout", 19);
        closesocket(client_socket);
        return;
    }
    
    buffer[bytes_received] = '\0';
    
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    getpeername(client_socket, (struct sockaddr*)&addr, &addr_len);
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    char method[16], path[1024], version[16], content_type[256];
    long content_length = 0;
    
    if (!parse_http_request(buffer, method, path, version, &content_length, content_type)) {
        printf("[%s] 400 Bad Request\n", client_ip);
        send_response(client_socket, 400, "Bad Request", "text/plain",
                     "400 Bad Request", 15);
        closesocket(client_socket);
        return;
    }
    
    printf("[%s] %s %s\n", client_ip, method, path);
    
    if (!is_path_safe(path, work_dir)) {
        printf("[%s] 403 Forbidden (path traversal attempt)\n", client_ip);
        send_response(client_socket, 403, "Forbidden", "text/plain",
                     "403 Forbidden - Path traversal attempt", 42);
        closesocket(client_socket);
        return;
    }
    
    if (str_cmp(method, "GET")) {
        handle_get(client_socket, path, work_dir);
    } else if (str_cmp(method, "DELETE")) {
        handle_delete(client_socket, path, work_dir);
    } else if (str_cmp(method, "POST")) {
        handle_post(client_socket, path, work_dir, buffer, content_length, content_type);
    } else if (str_cmp(method, "MKCOL")) {
        handle_mkcol(client_socket, path, work_dir);
    } else {
        printf("[%s] 405 Method Not Allowed (%s)\n", client_ip, method);
        send_response(client_socket, 405, "Method Not Allowed", "text/plain",
                     "405 Method Not Allowed", 22);
    }
    
    closesocket(client_socket);
}

int start_server(Server_config* config) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        puts("Failed to initialize Winsock");
        return -1;
    }
    
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Failed to create socket: %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(config->port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Failed to bind socket: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return -1;
    }
    
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Failed to listen on socket: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return -1;
    }
    
    printf("Port: %hu\n", config->port);
    printf("Directory: %s\n", config->work_dir);
    printf("Timeout: %u seconds\nServer started....\n\n", config->timeout);
    
    connection_queue = init_queue();
    if (!connection_queue) {
        printf("Failed to initialize connection queue\n");
        closesocket(server_socket);
        WSACleanup();
        return -1;
    }
    
    worker_count = MIN_WORKERS;
    for (int i = 0; i < worker_count; i++) {
        WorkerData* worker_data = (WorkerData*)malloc(sizeof(WorkerData));
        if (!worker_data) {
            printf("Failed to allocate worker data\n");
            LWARN("Failed to allocate worker data");
            continue;
        }
        
        worker_data->client_socket = INVALID_SOCKET;
        str_copy(worker_data->work_dir, config->work_dir, sizeof(worker_data->work_dir));
        worker_data->timeout = config->timeout;
        
        worker_handles[i] = CreateThread(NULL, 0, worker_process, worker_data, 0, NULL);
        if (!worker_handles[i]) {
            printf("Failed to create worker thread %d\n", i);
            free(worker_data);
        }
    }
    
    printf("Worker threads: %d\n", worker_count);
    
    while (server_running) {
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        
        SOCKET client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (client_socket == INVALID_SOCKET) {
            if (server_running) {
                printf("Accept error: %d\n", WSAGetLastError());
            }
            continue;
        }
        
        queue_add_connection(connection_queue, client_socket);
    }
    
    printf("\nShutting down server...\n");
    
    server_running = FALSE;
    
    for (int i = 0; i < worker_count; i++) {
        ReleaseSemaphore(connection_queue->semaphore, 1, NULL);
    }
    
    WaitForMultipleObjects(worker_count, worker_handles, TRUE, INFINITE);
    
    for (int i = 0; i < worker_count; i++) {
        if (worker_handles[i]) {
            CloseHandle(worker_handles[i]);
        }
    }
    
    destroy_queue(connection_queue);
    
    closesocket(server_socket);
    WSACleanup();
    
    printf("Server stopped\n");
    return 0;
}