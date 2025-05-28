#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
    #include <windows.h>
    #define PATH_SEPARATOR '\\'
#else
    #define PATH_SEPARATOR '/'
#endif

void search_files(const char *dir_path, const char *target, const char *base_path, int search_content);
void search_in_file(const char *file_path, const char *target, const char *relative_path);
int is_word_match(const char *line, const char *word);

int main(int argc, char *argv[]) {
    int search_content = 0;
    const char *search_str = NULL;
    const char *search_dir = "D:\\1code\\Linux-0.11";
    #ifndef _WIN32
        const char *search_dir = "/D/1code/Linux-0.11";
    #endif

    if (argc < 2) {
        fprintf(stderr, "用法: %s [-d] <搜索字符串>\n", argv[0]);
        return 1;
    }

    int i = 1;
    if (i < argc && strcmp(argv[i], "-d") == 0) {
        search_content = 1;
        i++;
    }

    if (i >= argc) {
        fprintf(stderr, "错误: 缺少搜索字符串\n");
        return 1;
    }

    search_str = argv[i];
    search_files(search_dir, search_str, search_dir, search_content);
    return 0;
}

void search_files(const char *dir_path, const char *target, const char *base_path, int search_content) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char path[1024];
    char relative_path[1024];

    if (!(dir = opendir(dir_path)))
        return;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(path, sizeof(path), "%s%c%s", dir_path, PATH_SEPARATOR, entry->d_name);
        if (stat(path, &statbuf) == -1)
            continue;

        if (S_ISDIR(statbuf.st_mode)) {
            search_files(path, target, base_path, search_content);
        } else {
            size_t base_len = strlen(base_path);
            if (strncmp(path, base_path, base_len) == 0) {
                strcpy(relative_path, &path[base_len]);
                if (relative_path[0] == PATH_SEPARATOR)
                    memmove(relative_path, relative_path + 1, strlen(relative_path));
            } else {
                strcpy(relative_path, path);
            }

            if (search_content) {
                search_in_file(path, target, relative_path);
            } else if (strstr(entry->d_name, target) != NULL) {
                printf("%s\n", relative_path);
            }
        }
    }
    closedir(dir);
}

void search_in_file(const char *file_path, const char *target, const char *relative_path) {
    FILE *file = fopen(file_path, "r");
    if (!file) return;

    char line[4096];
    int line_num = 0;

    while (fgets(line, sizeof(line), file)) {
        line_num++;
        if (is_word_match(line, target)) {
            printf("%s:%d\n", relative_path, line_num);
        }
    }

    fclose(file);
}

int is_word_match(const char *line, const char *word) {
    const char *p = line;
    size_t word_len = strlen(word);

    while (*p) {
        // 跳过前导非单词字符
        while (*p && !((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
            p++;

        if (!*p) break;

        const char *start = p;
        // 找到单词结尾
        while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
            p++;

        size_t len = p - start;
        if (len == word_len && strncmp(start, word, len) == 0)
            return 1;
    }

    return 0;
}    