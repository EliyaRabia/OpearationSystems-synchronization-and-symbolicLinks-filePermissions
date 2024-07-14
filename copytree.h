#ifndef COPYTREE_H
#define COPYTREE_H

#ifdef __cplusplus
extern "C" {
#endif

void copy_file(const char *src, const char *dest, int copy_symlinks, int copy_permissions);
void copy_directory(const char *src, const char *dest, int copy_symlinks, int copy_permissions);
void create_directories(const char *dir_path);

#ifdef __cplusplus
}
#endif

#endif // COPYTREE_H
