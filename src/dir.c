#include "dir.h"
#include "str.h"

#ifdef _WIN32
    #include <windows.h>
#else
    //чот для линукса
#endif


void get_current_dir(char *buffer, unsigned int size) {
    LDBG("get_current_dir in");
#ifdef _WIN32
    GetCurrentDirectory(size, buffer);
#else
    // эта фн для линукса
#endif
    LDBG("get_current_dir out");
}

void normalize_dir(char* dir){
    LDBG("normalize_dir in");
#ifdef _WIN32
    str_replace(dir, '/', '\\');
#else
    str_replace(dir, '\\', '/');
#endif
    LDBG("normalize_dir out");
}

int is_dir_exists(const char* path) {
    LDBG("is_dir_exists in");
#ifdef _WIN32
    unsigned int attrib = GetFileAttributes(path);
    LDBG("is_dir_exists out");
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
#else

#endif
}









