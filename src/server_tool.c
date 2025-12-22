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
#include <sys/stat.h>

static int worker_count = 0;
static volatile BOOL server_running = TRUE;
static ConnectionQueue* connection_queue = NULL;
static HANDLE worker_handles[MAX_WORKERS];

const char* HTML_TEMPLATE = 
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"    <title>File Server - %s</title>\n"
"    <style>\n"
"        body { font-family: Arial, sans-serif; margin: 40px; }\n"
"        h1 { color: #333; }\n"
"        table { border-collapse: collapse; width: 100%%; margin-top: 20px; }\n"
"        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n"
"        th { background-color: #f2f2f2; }\n"
"        tr:hover { background-color: #f5f5f5; }\n"
"        a { text-decoration: none; color: #0066cc; }\n"
"        a:hover { text-decoration: underline; }\n"
"        .upload-form { margin-top: 30px; padding: 20px; border: 1px solid #ddd; background-color: #f9f9f9; }\n"
"        .error { color: #cc0000; margin-top: 10px; }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <h1>Directory: %s</h1>\n"
"    %s\n"  
"    <div class=\"upload-form\">\n"
"        <h3>Upload File</h3>\n"
"        <form action=\"%s\" method=\"post\" enctype=\"multipart/form-data\">\n"
"            <input type=\"file\" name=\"file\" required>\n"
"            <input type=\"submit\" value=\"Upload\">\n"
"        </form>\n"
"    </div>\n"
"</body>\n"
"</html>\n";

int check_port(unsigned short port) {
    LDBG("check_port in");
    if(port == 0) return 0;
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in addr;
    int result;

    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        LERR("cant wsasstartup");
        LDBG("check_port out");
        return 0;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        LERR("invalid socket");
        LDBG("check_port out");
        return 0;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)port);

    result = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    
    closesocket(sock);
    WSACleanup();
    
    if (result == SOCKET_ERROR) {
        LERR("socket bind error");
        LDBG("check_port out");
        return 0;
    }
    LDBG("check_port out");
    return 1; 
}

ConnectionQueue* init_queue() {
    LDBG("init_queue");
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

int parse_http_request(const char* request, char* method, char* path, char* version, long* content_length, char* content_type, char* boundary) {
    LDBG("parse_http_request in");
    char first_line[1024];
    int i = 0;

    while (request[i] != '\r' && request[i] != '\n' && i < 1023) {
        first_line[i] = request[i];
        i++;
    }
    first_line[i] = '\0';
    
    if (sscanf(first_line, "%15s %1023s %15s", method, path, version) != 3) {
        LDBG("parse_http_request in");
        return 0;
    }
    
    *content_length = 0;
    content_type[0] = '\0';
    if (boundary) boundary[0] = '\0';

    const char* ptr = request + i;
    while (*ptr == '\r' || *ptr == '\n') ptr++;
    
    while (*ptr && *ptr != '\r' && (ptr[0] != '\r' || ptr[1] != '\n')) {
        while (*ptr == ' ' || *ptr == '\t') ptr++;
        
        const char* line_end = ptr;
        while (*line_end && *line_end != '\r' && *line_end != '\n') line_end++;

        char line[1024];
        int line_len = line_end - ptr;
        if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
        strncpy(line, ptr, line_len);
        line[line_len] = '\0';

        char* colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char* value = colon + 1;
            while (*value == ' ') value++;

            if (str_cmpi(line, "Content-Length")) {
                *content_length = atol(value);
            } else if (str_cmpi(line, "Content-Type")) {
                str_copy(content_type, value, 511);
                
                if (boundary && str_search_str(value, "multipart/form-data")) {
                    const char* b = str_search_str(value, "boundary=");
                    if (b) {
                        b += 9;
                        int j = 0;
                        while (*b && *b != ';' && *b != ' ' && *b != '\r' && *b != '\n' && j < 127) {
                            boundary[j++] = *b++;
                        }
                        boundary[j] = '\0';
                    }
                }
            }
        }

        ptr = line_end;
        while (*ptr == '\r' || *ptr == '\n') ptr++;
        if (*ptr == '\r' && *(ptr + 1) == '\n') break;
    }
    LDBG("parse_http_request in");
    return 1;
}

int is_path_safe(const char* requested_path, const char* work_dir) {
    LDBG("is_path_safe in");
    char work_dir_normalized[1024];
    str_copy(work_dir_normalized, work_dir, sizeof(work_dir_normalized));
    normalize_dir(work_dir_normalized);

    int work_len = str_len(work_dir_normalized);
    if (work_len > 0 && (work_dir_normalized[work_len - 1] == '\\' || work_dir_normalized[work_len - 1] == '/')) {
        work_dir_normalized[work_len - 1] = '\0';
    }
    
    char full_path[2048];
    if (requested_path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", work_dir_normalized, requested_path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", work_dir_normalized, requested_path);
    }
    
    normalize_dir(full_path);
    LDBG("is_path_safe out");
    return str_include(full_path, work_dir_normalized);
}

int has_read_permission(const char* file_path) {
    return (_access(file_path, 4) == 0); 
}

int has_write_permission(const char* file_path) {
    return (_access(file_path, 2) == 0); 
}

const char* get_mime_type(const char* filename) {
    LDBG("get_mime_type in");
    const char* ext = strrchr(filename, '.');
    if (!ext){
        LDBG("get_mime_type out");
        return "application/octet-stream";
    }
    
    ext++;
    
    if (str_cmp(ext, "html") || str_cmp(ext, "htm")) {
        LDBG("get_mime_type out");
        return "text/html; charset=utf-8";
    } else if (str_cmp(ext, "css")) {
        LDBG("get_mime_type out");
        return "text/css; charset=utf-8";
    } else if (str_cmp(ext, "js")) {
        LDBG("get_mime_type out");
        return "application/javascript; charset=utf-8";
    } else if (str_cmp(ext, "txt") || str_cmp(ext, "log")) {
        LDBG("get_mime_type out");
        return "text/plain; charset=utf-8";
    } else {
        LDBG("get_mime_type out");
        return "application/octet-stream";
    }
}

void send_response(SOCKET client_socket, int status_code, const char* status_text, const char* content_type, const char* body, long body_length) {
    LDBG("send_response in");
    char headers[512];
    int headers_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, content_type, body_length);
    
    send(client_socket, headers, headers_len, 0);
    LDBG("send_response out");
    if (body && body_length > 0) {
        send(client_socket, body, body_length, 0);
    }
    return;
}

void send_file_response(SOCKET client_socket, const char* file_path) {
    LDBG("send_file_response in");
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        send_response(client_socket, 404, "Not Found", "text/plain", "404 Not Found", 12);
        LDBG("send_file_response out");
        return;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    const char* mime_type = get_mime_type(file_path);
    
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
    LDBG("send_file_response out");
    fclose(file);
    return;
}

char* generate_directory_html(const char* dir_path, const char* request_path) {
    LDBG("generate_directory_html in");
    WIN32_FIND_DATA find_data;
    char search_path[2048];
    
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);
    str_replace(search_path, '/', '\\');  
    
    HANDLE hFind = FindFirstFile(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("DEBUG| Failed to open directory: %s\n", dir_path);
        LDBG("generate_directory_html out");
        return NULL;
    }
    
    char* html = malloc(131072);  
    if (!html) {
        FindClose(hFind);
        LDBG("generate_directory_html out");
        return NULL;
    }
    
    int pos = 0;
    
    pos += snprintf(html + pos, 131072 - pos,
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>File Server - %s</title>\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; margin: 40px; }\n"
        "        h1 { color: #333; }\n"
        "        table { border-collapse: collapse; width: 100%%; margin-top: 20px; }\n"
        "        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n"
        "        th { background-color: #f2f2f2; }\n"
        "        tr:hover { background-color: #f5f5f5; }\n"
        "        a { text-decoration: none; color: #0066cc; }\n"
        "        a:hover { text-decoration: underline; }\n"
        "        .upload-form { margin-top: 30px; padding: 20px; border: 1px solid #ddd; background-color: #f9f9f9; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <h1>Directory: %s</h1>\n"
        "    <table>\n"
        "        <tr>\n"
        "            <th>Name</th>\n"
        "            <th>Size</th>\n"
        "            <th>Type</th>\n"
        "            <th>Modified</th>\n"
        "        </tr>\n",
        request_path, request_path);
    
    if (str_len(request_path) > 1 && !str_cmp(request_path, "/")) {
        char parent_path[1024];
        str_copy(parent_path, request_path, sizeof(parent_path));
        
        int len = str_len(parent_path);
        if (len > 0 && parent_path[len - 1] == '/') {
            parent_path[len - 1] = '\0';
            len--;
        }
        
        char* last_slash = strrchr(parent_path, '/');
        if (last_slash) {
            if (last_slash != parent_path) {
                *last_slash = '\0';
            } else {
                parent_path[1] = '\0';
            }
        }
        
        if (str_len(parent_path) == 0) {
            str_copy(parent_path, "/", sizeof(parent_path));
        }
        
        printf("DEBUG| Parent path: '%s' -> '%s'\n", request_path, parent_path);
        
        pos += snprintf(html + pos, 131072 - pos,
            "        <tr>\n"
            "            <td><a href=\"%s\">../</a></td>\n"
            "            <td>-</td>\n"
            "            <td>Directory</td>\n"
            "            <td>-</td>\n"
            "        </tr>\n",
            parent_path);
    }

    typedef struct {
        char name[260];
        int is_dir;
        LARGE_INTEGER size;
        FILETIME mtime;
    } FileEntry;
    
    FileEntry entries[1000];
    int entry_count = 0;
    
    do {
        if (str_cmp(find_data.cFileName, ".") || str_cmp(find_data.cFileName, "..")) {
            continue;
        }

        if (entry_count < 1000) {
            str_copy(entries[entry_count].name, find_data.cFileName, sizeof(entries[entry_count].name));
            
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                entries[entry_count].is_dir = 1;
                strcat(entries[entry_count].name, "/");
            } else {
                entries[entry_count].is_dir = 0;
            }
            
            entries[entry_count].size.LowPart = find_data.nFileSizeLow;
            entries[entry_count].size.HighPart = find_data.nFileSizeHigh;
            entries[entry_count].mtime = find_data.ftLastWriteTime;
            
            entry_count++;
        }
    } while (FindNextFile(hFind, &find_data));
    
    FindClose(hFind);

    for (int i = 0; i < entry_count - 1; i++) {
        for (int j = i + 1; j < entry_count; j++) {
            if ((!entries[i].is_dir && entries[j].is_dir) ||
                (entries[i].is_dir == entries[j].is_dir && 
                 strcmp(entries[i].name, entries[j].name) > 0)) {
                FileEntry temp = entries[i];
                entries[i] = entries[j];
                entries[j] = temp;
            }
        }
    }

    for (int i = 0; i < entry_count; i++) {

        char href[1024];

        if (str_cmp(request_path, "/")) {
            snprintf(href, sizeof(href), "/%s", entries[i].name);
        } else {
            char clean_path[1024];
            str_copy(clean_path, request_path, sizeof(clean_path));
            
            int len = str_len(clean_path);
            if (len > 1 && clean_path[len - 1] == '/') {
                clean_path[len - 1] = '\0';
            }
            
            snprintf(href, sizeof(href), "%s/%s", clean_path, entries[i].name);
        }
        
        printf("DEBUG| Creating link: '%s' -> '%s'\n", entries[i].name, href);

        char size_str[32];
        if (entries[i].is_dir) {
            str_copy(size_str, "-", sizeof(size_str));
        } else if (entries[i].size.QuadPart < 1024) {
            snprintf(size_str, sizeof(size_str), "%lld B", entries[i].size.QuadPart);
        } else if (entries[i].size.QuadPart < 1024 * 1024) {
            snprintf(size_str, sizeof(size_str), "%.1f KB", entries[i].size.QuadPart / 1024.0);
        } else {
            snprintf(size_str, sizeof(size_str), "%.1f MB", entries[i].size.QuadPart / (1024.0 * 1024.0));
        }
        
        SYSTEMTIME st;
        FileTimeToSystemTime(&entries[i].mtime, &st);
        
        const char* type = entries[i].is_dir ? "Directory" : "File";
        
        pos += snprintf(html + pos, 131072 - pos,
            "        <tr>\n"
            "            <td><a href=\"%s\">%s</a></td>\n"
            "            <td>%s</td>\n"
            "            <td>%s</td>\n"
            "            <td>%04d-%02d-%02d %02d:%02d</td>\n"
            "        </tr>\n",
            href, entries[i].name,
            size_str, type,
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    }
    
    pos += snprintf(html + pos, 131072 - pos,
        "    </table>\n"
        "    <div class=\"create-folder-form\">\n"
        "        <h3>Create New Folder</h3>\n"
        "        <form onsubmit=\"return createFolder(event)\">\n"
        "            <input type=\"text\" id=\"folderName\" name=\"folderName\" placeholder=\"Folder name\" required>\n"
        "            <input type=\"submit\" value=\"Create Folder\">\n"
        "        </form>\n"
        "        <div id=\"folderMessage\" style=\"margin-top:10px; color:green;\"></div>\n"
        "    </div>\n"
        "    <div class=\"upload-form\">\n"
        "        <h3>Upload File</h3>\n"
        "        <form action=\".\" method=\"post\" enctype=\"multipart/form-data\">\n"
        "            <input type=\"file\" name=\"file\" required>\n"
        "            <input type=\"submit\" value=\"Upload\">\n"
        "        </form>\n"
        "    </div>\n"
        "    <script>\n"
        "        async function createFolder(event) {\n"
        "            event.preventDefault();\n"
        "            const folderName = document.getElementById('folderName').value.trim();\n"
        "            const messageDiv = document.getElementById('folderMessage');\n"
        "            \n"
        "            if (!folderName) {\n"
        "                messageDiv.style.color = 'red';\n"
        "                messageDiv.textContent = 'Folder name is required';\n"
        "                return false;\n"
        "            }\n"
        "            \n"
        "            // Проверяем, что имя папки не содержит запрещенных символов\n"
        "            if (/[\\\\/:*?\"<>|]/.test(folderName)) {\n"
        "                messageDiv.style.color = 'red';\n"
        "                messageDiv.textContent = 'Invalid folder name. Cannot contain \\\\/:*?\"<>|';\n"
        "                return false;\n"
        "            }\n"
        "            \n"
        "            messageDiv.style.color = 'blue';\n"
        "            messageDiv.textContent = 'Creating folder...';\n"
        "            \n"
        "            try {\n"
        "                // Определяем текущий путь\n"
        "                const currentPath = window.location.pathname;\n"
        "                const folderPath = currentPath === '/' ? \n"
        "                    '/' + encodeURIComponent(folderName) : \n"
        "                    currentPath + '/' + encodeURIComponent(folderName);\n"
        "                \n"
        "                // Отправляем MKDIR запрос\n"
        "                const response = await fetch(folderPath, {\n"
        "                    method: 'MKDIR',\n"
        "                    headers: {\n"
        "                        'Content-Type': 'application/json'\n"
        "                    }\n"
        "                });\n"
        "                \n"
        "                const result = await response.json();\n"
        "                \n"
        "                if (response.ok) {\n"
        "                    messageDiv.style.color = 'green';\n"
        "                    messageDiv.textContent = result.message || 'Folder created successfully';\n"
        "                    // Обновляем страницу через 1 секунду\n"
        "                    setTimeout(() => {\n"
        "                        window.location.reload();\n"
        "                    }, 1000);\n"
        "                } else {\n"
        "                    messageDiv.style.color = 'red';\n"
        "                    messageDiv.textContent = result.message || 'Failed to create folder';\n"
        "                }\n"
        "            } catch (error) {\n"
        "                messageDiv.style.color = 'red';\n"
        "                messageDiv.textContent = 'Error: ' + error.message;\n"
        "            }\n"
        "            \n"
        "            return false;\n"
        "        }\n"
        "    </script>\n"
        "</body>\n"
        "</html>\n");
    LDBG("generate_directory_html out");
    return html;
}

void handle_get(SOCKET client_socket, const char* path, const char* work_dir) {
     LDBG("handle_get int");
    char full_path[2048];

    if (path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", work_dir, path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", work_dir, path);
    }

    int len = str_len(full_path);
    if (len > 1 && (full_path[len - 1] == '/' || full_path[len - 1] == '\\')) {
        full_path[len - 1] = '\0';
    }

    if (str_len(path) == 0 || str_cmp(path, "/")) {
        char* html = generate_directory_html(work_dir, "/");
        if (html) {
            send_response(client_socket, 200, "OK", "text/html; charset=utf-8", html, str_len(html));
            free(html);
        } else {
            send_response(client_socket, 500, "Internal Server Error", "text/plain", "500 Internal Server Error", 24);
        }
        LDBG("handle_get out");
        return;
    }
    
    DWORD attrib = GetFileAttributes(full_path);
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        send_response(client_socket, 404, "Not Found", "text/plain", "404 Not Found", 12);
        LDBG("handle_get out");
        return;
    }
    
    if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
        char* html = generate_directory_html(full_path, path);
        if (html) {
            send_response(client_socket, 200, "OK", "text/html; charset=utf-8",
                         html, str_len(html));
            free(html);
        } else {
            send_response(client_socket, 500, "Internal Server Error", "text/plain","500 Internal Server Error", 24);
        }
        LDBG("handle_get out");
        return;
    }
    
    if (!has_read_permission(full_path)) {
        send_response(client_socket, 403, "Forbidden", "text/plain","403 Forbidden - No read permission", 36);
        LDBG("handle_get out");
        return;
    }

    send_file_response(client_socket, full_path);
    LDBG("handle_get out");
    return;
}

void handle_delete(SOCKET client_socket, const char* path, const char* work_dir) {
    LDBG("handle_delete in");
    char full_path[2048];
    
    if (path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", work_dir, path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", work_dir, path);
    }
    
    normalize_dir(full_path);
    
    if (GetFileAttributes(full_path) == INVALID_FILE_ATTRIBUTES) {
        send_response(client_socket, 404, "Not Found", "text/plain", "404 Not Found", 12);
        LDBG("handle_delete out");
        return;
    }
    
    if (!has_write_permission(full_path)) {
        send_response(client_socket, 403, "Forbidden", "text/plain", "403 Forbidden - No write permission", 37);
        LDBG("handle_delete out");
        return;
    }
    
    if (remove(full_path) == 0) {
        char response[256];
        int len = snprintf(response, sizeof(response), "{\"status\": \"success\", \"message\": \"File deleted: %s\"}", path);
        send_response(client_socket, 200, "OK", "application/json", response, len);
    } else {
        send_response(client_socket, 500, "Internal Server Error", "text/plain", "500 Internal Server Error", 24);
    }
    LDBG("handle_delete out");
    return;
}

char* create_temp_file(const char* work_dir) {
    LDBG("create_temp_file in");
    char temp_path[MAX_PATH];
    char temp_file[MAX_PATH];
    
    if (GetTempPath(MAX_PATH, temp_path) == 0) {
        LDBG("create_temp_file out");
        return NULL;
    }
    
    if (GetTempFileName(temp_path, "upl", 0, temp_file) == 0) {
        LDBG("create_temp_file out");
        return NULL;
    }
    
    char* result = malloc(str_len(temp_file) + 1);
    if (result) {
        str_copy(result, temp_file, str_len(temp_file) + 1);
    }
    LDBG("create_temp_file out");
    return result;
}

char* extract_filename(const char* content_disposition) {
    LDBG("extract_filename in");
    const char* filename_start = str_search_str(content_disposition, "filename=\"");
    int i = 0;
    if (!filename_start){
        return NULL;
        LDBG("extract_filename out");
    }
    
    filename_start += 10; 
    
    char* filename = malloc(256);
    if (!filename){
        return NULL;
        LDBG("extract_filename out");
    }
    while (*filename_start != '"' && *filename_start != '\0' && i < 255) {
        filename[i++] = *filename_start++;
    }
    filename[i] = '\0';
    LDBG("extract_filename out");
    return filename;
}

int process_multipart_data(const char* request_data, long content_length, const char* boundary, char** filename, char** temp_file_path) {
    LDBG("process_multipart_data in");
    if (!boundary || str_len(boundary) == 0) {
        return 0;
    }
    
    char full_boundary[256];
    snprintf(full_boundary, sizeof(full_boundary), "--%s", boundary);
    char end_boundary[256];
    snprintf(end_boundary, sizeof(end_boundary), "--%s--", boundary);
    
    const char* data_start = str_search_str(request_data, full_boundary);
    if (!data_start) {
        LDBG("process_multipart_data out");
        return 0;
    }
    data_start += str_len(full_boundary);
    if (*data_start == '\r') data_start++;
    if (*data_start == '\n') data_start++;
    
    const char* disposition_start = str_search_str(data_start, "Content-Disposition:");
    if (!disposition_start) {
        LDBG("process_multipart_data out");
        return 0;
    }
    const char* filename_start = str_search_str(disposition_start, "filename=\"");
    if (!filename_start) {
        LDBG("process_multipart_data out");
        return 0;
    }
    *filename = extract_filename(filename_start);
    if (!*filename) {
        LDBG("process_multipart_data out");
        return 0;
    }
    const char* file_data_start = str_search_str(disposition_start, "\r\n\r\n");
    if (!file_data_start) {
        free(*filename);
        LDBG("process_multipart_data out");
        return 0;
    }
    
    file_data_start += 4;
    
    const char* file_data_end = str_search_str(file_data_start, full_boundary);
    if (!file_data_end) {
        file_data_end = str_search_str(file_data_start, end_boundary);
        if (!file_data_end) {
            file_data_end = request_data + content_length;
        }
    }
    *temp_file_path = create_temp_file(NULL);
    if (!*temp_file_path) {
        free(*filename);
        LDBG("process_multipart_data out");
        return 0;
    }
    
    FILE* temp_file = fopen(*temp_file_path, "wb");
    if (!temp_file) {
        free(*filename);
        free(*temp_file_path);
        LDBG("process_multipart_data out");
        return 0;
    }
    
    fwrite(file_data_start, 1, file_data_end - file_data_start, temp_file);
    fclose(temp_file);
    LDBG("process_multipart_data out");
    return 1;
}

void handle_post(SOCKET client_socket, const char* path, const char* work_dir, const char* request_data, long content_length, const char* content_type) {
    LDBG("handle_post in");
    char boundary[128] = {0};
    int i = 0;
    if (!str_search_str(content_type, "multipart/form-data")) {
        send_response(client_socket, 400, "Bad Request", "text/plain", "400 Bad Request - Expected multipart/form-data", 52);
        return;
    }
    
    const char* boundary_start = str_search_str(content_type, "boundary=");
    if (!boundary_start) {
        send_response(client_socket, 400, "Bad Request", "text/plain", "400 Bad Request - No boundary specified", 43);
        LDBG("handle_post out");
        return;
    }
    
    boundary_start += 9;
    while (*boundary_start && *boundary_start != ';' && *boundary_start != ' ' && i < 127) {
        boundary[i++] = *boundary_start++;
    }
    boundary[i] = '\0';
    
    char* filename = NULL;
    char* temp_file_path = NULL;
    
    if (!process_multipart_data(request_data, content_length, boundary, &filename, &temp_file_path)) {
        send_response(client_socket, 400, "Bad Request", "text/plain", "400 Bad Request - Invalid multipart data", 44);
        LDBG("handle_post out");
        return;
    }
    
    char dest_path[2048];
    if (path[0] == '/') {
        snprintf(dest_path, sizeof(dest_path), "%s%s%s", work_dir, (path[str_len(path)-1] == '/' ? "" : "/"), filename);
    } else {
        snprintf(dest_path, sizeof(dest_path), "%s/%s%s", work_dir, path, (path[str_len(path)-1] == '/' ? "" : "/"), filename);
    }
    
    normalize_dir(dest_path);

    if (GetFileAttributes(dest_path) != INVALID_FILE_ATTRIBUTES) {
        send_response(client_socket, 403, "Forbidden", "text/plain","403 Forbidden - File already exists", 40);
        free(filename);
        free(temp_file_path);
        LDBG("handle_post out");
        return;
    }

    char dest_dir[MAX_PATH];
    str_copy(dest_dir, dest_path, sizeof(dest_dir));
    char* last_slash = strrchr(dest_dir, '\\');
    if (last_slash) {
        *last_slash = '\0';
        if (!has_write_permission(dest_dir)) {
            send_response(client_socket, 403, "Forbidden", "text/plain", "403 Forbidden - No write permission in directory", 55);
            free(filename);
            free(temp_file_path);
            LDBG("handle_post out");
            return;
        }
    }

    if (rename(temp_file_path, dest_path) != 0) {
        FILE* src = fopen(temp_file_path, "rb");
        FILE* dst = fopen(dest_path, "wb");
        
        if (!src || !dst) {
            if (src) fclose(src);
            if (dst) fclose(dst);
            send_response(client_socket, 500, "Internal Server Error", "text/plain", "500 Internal Server Error - Cannot save file", 51);
            free(filename);
            free(temp_file_path);
            LDBG("handle_post out");
            return;
        }
        
        char buffer[8192];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, bytes_read, dst);
        }
        LDBG("handle_post out");
        fclose(src);
        fclose(dst);
        remove(temp_file_path);
    }

    char response[512];
    int len = snprintf(response, sizeof(response),"{\"status\": \"success\", \"message\": \"File uploaded\", ""\"filename\": \"%s\", \"path\": \"%s\"}",filename, path);
    send_response(client_socket, 201, "Created", "application/json", response, len);
    
    free(filename);
    free(temp_file_path);
    LDBG("handle_post out");
}

void handle_mkdir(SOCKET client_socket, const char* path, const char* work_dir) {
    LDBG("handle_mkdir in");
    char full_path[2048];
    
    if (path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", work_dir, path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", work_dir, path);
    }
    
    normalize_dir(full_path);
    
    int len = str_len(full_path);
    if (len > 0 && (full_path[len - 1] == '\\' || full_path[len - 1] == '/')) {
        full_path[len - 1] = '\0';
    }
    
    if (GetFileAttributes(full_path) != INVALID_FILE_ATTRIBUTES) {
        send_response(client_socket, 409, "Conflict", "text/plain", "409 Conflict - Directory already exists", 45);
        LDBG("handle_mkdir out");
        return;
    }
    
    if (_mkdir(full_path) == 0) {
        char response[256];
        int len = snprintf(response, sizeof(response), "{\"status\": \"success\", \"message\": \"Directory created: %s\"}", path);
        send_response(client_socket, 201, "Created", "application/json", response, len);
    } else {
        send_response(client_socket, 500, "Internal Server Error", "text/plain", "500 Internal Server Error - Cannot create directory", 57);
    }
    LDBG("handle_mkdir out");
    return;
}

void handle_request(SOCKET client_socket, const char* work_dir, unsigned int timeout) {
    LDBG("handle_request in");
    DWORD timeout_ms = timeout * 1000;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
    
    char buffer[65536];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        printf("[ERROR] Timeout or no data received\n");
        send_response(client_socket, 408, "Request Timeout", "text/plain", "408 Request Timeout", 19);
        closesocket(client_socket);
        LDBG("handle_request out");
        return;
    }
    
    buffer[bytes_received] = '\0';
    
    printf("[REQUEST] First 500 chars:\n%.500s\n", buffer);
    
    char method[16], path[1024], version[16], content_type[256];
    long content_length = 0;
    char boundary[128] = {0};
    
    if (!parse_http_request(buffer, method, path, version, &content_length, content_type, boundary)) {
        printf("[ERROR] Failed to parse HTTP request\n");
        send_response(client_socket, 400, "Bad Request", "text/plain", "400 Bad Request", 15);
        closesocket(client_socket);
        LDBG("handle_request out");
        return;
    }
    
    printf("[REQUEST] Method: %s, Path: %s, Version: %s\n", method, path, version);
    printf("[REQUEST] Content-Type: '%s', Content-Length: %ld\n", content_type, content_length);
    
    if (str_cmpi(method, "POST") && content_length > 0) {
        printf("[POST] Need to read more data: %ld > %d\n", content_length, bytes_received);
        if (content_length > sizeof(buffer) - 1) {
            printf("[POST] Content too large: %ld bytes (max: %zu)\n", content_length, sizeof(buffer) - 1);
            
            char* full_buffer = malloc(content_length + 1);
            if (!full_buffer) {
                send_response(client_socket, 500, "Internal Server Error", "text/plain", "500 Internal Server Error", 24);
                closesocket(client_socket);
                LDBG("handle_request out");
                return;
            }
            memcpy(full_buffer, buffer, bytes_received);
            
            int remaining = content_length - bytes_received;
            int total_received = bytes_received;
            
            while (remaining > 0) {
                int more = recv(client_socket, full_buffer + total_received, remaining, 0);
                if (more <= 0) {
                    printf("[POST] Failed to read remaining data\n");
                    free(full_buffer);
                    send_response(client_socket, 400, "Bad Request", "text/plain", "400 Bad Request - Incomplete data", 33);
                    closesocket(client_socket);
                    LDBG("handle_request out");
                    return;
                }
                total_received += more;
                remaining -= more;
            }
            
            full_buffer[total_received] = '\0';
            printf("[POST] Total received: %d bytes\n", total_received);
        
            handle_post(client_socket, path, work_dir, full_buffer, content_length, content_type);
            free(full_buffer);
        } else {
            int remaining = content_length - bytes_received;
            while (remaining > 0) {
                int more = recv(client_socket, buffer + bytes_received, sizeof(buffer) - bytes_received - 1, 0);
                if (more <= 0) {
                    printf("[POST] Failed to read remaining data\n");
                    send_response(client_socket, 400, "Bad Request", "text/plain", "400 Bad Request - Incomplete data", 33);
                    closesocket(client_socket);
                    LDBG("handle_request out");
                    return;
                }
                bytes_received += more;
                remaining -= more;
            }
            
            buffer[bytes_received] = '\0';
            printf("[POST] Total received: %d bytes\n", bytes_received);
            printf("[POST] First 200 chars of body:\n%.200s\n", buffer + (bytes_received - content_length));
            
            handle_post(client_socket, path, work_dir, buffer, content_length, content_type);
        }
    } else if (str_cmpi(method, "GET")) {
        handle_get(client_socket, path, work_dir);
    } else if (str_cmpi(method, "DELETE")) {
        handle_delete(client_socket, path, work_dir);
    } else if (str_cmpi(method, "MKDIR") || str_cmpi(method, "MKCOL")) {
        handle_mkdir(client_socket, path, work_dir);
    } else {
        printf("[ERROR] Method not allowed: %s\n", method);
        send_response(client_socket, 405, "Method Not Allowed", "text/plain", "405 Method Not Allowed", 22);
    }
    LDBG("handle_request out");
    closesocket(client_socket);
    return;
}

int start_server(Server_config* config) {
    LDBG("server start in");
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        puts("Failed to initialize Winsock");
        LDBG("server start out");
        return -1;
    }
    
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Failed to create socket: %d\n", WSAGetLastError());
        WSACleanup();
        LDBG("server start out");
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
        LDBG("server start out");
        return -1;
    }
    
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Failed to listen on socket: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        LDBG("server start out");
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
        LDBG("server start out");
        return -1;
    }
    
    worker_count = MIN_WORKERS;
    for (int i = 0; i < worker_count; i++) {
        WorkerData* worker_data = (WorkerData*)malloc(sizeof(WorkerData));
        if (!worker_data) {
            printf("Failed to allocate worker data\n");
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
    LDBG("server start out");
    printf("Server stopped\n");
    return 0;
}