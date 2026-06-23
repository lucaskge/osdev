#ifndef FS_H
#define FS_H

void fs_init(void);
void fs_list_directory(void);
void fs_read_file(const char* filename);
void fs_create_file(const char* filename, const char* content);

#endif