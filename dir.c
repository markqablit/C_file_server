#include "dir.h"

#ifdef _WIN32
    #include <windows.h>
#else
    //чот для линукса
#endif


void get_current_dir(char *buffer, unsigned int size) {
#ifdef _WIN32
    GetCurrentDirectory(size, buffer);
#else
    // эта фн для линукса
#endif

}

int is_dir_exists(const char* path) {
#ifdef _WIN32
    unsigned int attrib = GetFileAttributes(path);
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
#else

#endif
}
