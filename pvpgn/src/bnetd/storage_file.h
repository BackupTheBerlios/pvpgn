#ifndef INClUDED_STORAGE_FILE_TYPES
#define INClUDED_STORAGE_FILE_TYPES

#include "storage.h"

typedef const char t_file_info;

typedef struct {
    void * (*read_attr)(const char *filename, const char *key);
    int (*read_attrs)(const char *filename, t_read_attr_func cb, void *data);
    int (*write_attrs)(const char *filename, void *attributes);
} t_file_engine;

#endif /* INClUDED_STORAGE_FILE_TYPES */

#ifndef JUST_NEED_TYPES
#ifndef INClUDED_STORAGE_FILE_PROTOS
#define INClUDED_STORAGE_FILE_PROTOS

extern t_storage storage_file;

#endif /* INClUDED_STORAGE_FILE_PROTOS */
#endif /* JUST_NEED_TYPES */
