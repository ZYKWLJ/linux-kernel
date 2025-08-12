#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#ifdef _WIN32
    #include <windows.h>
    #define PATH_SEPARATOR '\\'
    #define IS_DIR_SEPARATOR(c) ((c) == '\\' || (c) == '/')
#else
    #define PATH_SEPARATOR '/'
    #define IS_DIR_SEPARATOR(c) ((c) == '/')
    #include <libgen.h>  // For Linux path handling
#endif

// Recursively searches through directories
void search_files(const char *dir_path, const char *target, const char *base_path, int search_content);

// Searches for target string within a specific file
void search_in_file(const char *file_path, const char *target, const char *relative_path);

// Checks if a word exists as a whole word in a line
int is_word_match(const char *line, const char *word);

// Gets relative path from base directory
void get_relative_path(const char *full_path, const char *base_path, char *relative_path, size_t max_len);

int main(int argc, char *argv[]) {
    int search_content = 0;       // Flag for content search (1) vs filename search (0)
    const char *search_str = NULL; // String to search for
    const char *search_dir = NULL; // Directory to search in

    // Set default directory based on operating system
#ifdef _WIN32
    search_dir = "D:\\1code\\Linux-0.11";  // Default Windows directory
#else
    search_dir = "/D/1code/Linux-0.11";    // Default Linux directory
#endif

    // Parse command line arguments
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [-d] <search_string> [directory]\n", argv[0]);
        fprintf(stderr, "  -d: Search file contents instead of filenames\n");
        return 1;
    }

    int i = 1;
    // Check for content search flag
    if (i < argc && strcmp(argv[i], "-d") == 0) {
        search_content = 1;
        i++;
    }

    // Validate search string exists
    if (i >= argc) {
        fprintf(stderr, "Error: Missing search string\n");
        return 1;
    }

    search_str = argv[i];
    i++;

    // Use specified directory if provided
    if (i < argc) {
        search_dir = argv[i];
    }

    // Verify directory exists
    struct stat dir_stat;
    if (stat(search_dir, &dir_stat) == -1 || !S_ISDIR(dir_stat.st_mode)) {
        fprintf(stderr, "Error: Directory does not exist or cannot be accessed: %s\n", search_dir);
        return 1;
    }

    // Start file search
    printf("Searching for %s in %s...\n", search_str, search_dir);
    search_files(search_dir, search_str, search_dir, search_content);
    
    return 0;
}

void search_files(const char *dir_path, const char *target, const char *base_path, int search_content) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char path[1024];       // Full path to current entry
    char relative_path[1024]; // Path relative to base directory

    // Attempt to open directory
    if (!(dir = opendir(dir_path))) {
        perror("Failed to open directory");
        return;
    }

    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Skip current (.) and parent (..) directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct full path to entry
        snprintf(path, sizeof(path), "%s%c%s", dir_path, PATH_SEPARATOR, entry->d_name);
        
        // Get file/directory information
        if (stat(path, &statbuf) == -1) {
            perror("Failed to get file information");
            continue;
        }

        // If directory, search recursively
        if (S_ISDIR(statbuf.st_mode)) {
            search_files(path, target, base_path, search_content);
        } else {
            // Calculate relative path from base directory
            get_relative_path(path, base_path, relative_path, sizeof(relative_path));

            // Search in file content if flag set, otherwise search filename
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
    if (!file) {
        // On Linux, ignore files without read permission
#ifndef _WIN32
        if (access(file_path, R_OK) != 0)
            return;
#endif
        perror("Failed to open file");
        return;
    }

    char line[4096]; // Buffer for reading lines
    int line_num = 0; // Current line number

    // Read file line by line
    while (fgets(line, sizeof(line), file)) {
        line_num++;
        // Check if target word exists in current line
        if (is_word_match(line, target)) {
            printf("%s:%d: %s", relative_path, line_num, line);
        }
    }

    fclose(file);
}

int is_word_match(const char *line, const char *word) {
    const char *p = line;
    size_t word_len = strlen(word);

    while (*p) {
        // Skip non-alphanumeric/underscore characters
        while (*p && !isalnum((unsigned char)*p) && *p != '_')
            p++;

        if (!*p) break; // End of line reached

        const char *start = p;
        // Find end of current word
        while (*p && (isalnum((unsigned char)*p) || *p == '_'))
            p++;

        // Check if current word matches target
        size_t len = p - start;
        if (len == word_len && strncmp(start, word, len) == 0)
            return 1; // Match found
    }

    return 0; // No match found
}

// Implementation of relative path calculation
void get_relative_path(const char *full_path, const char *base_path, char *relative_path, size_t max_len) {
    size_t base_len = strlen(base_path);
    
    // Check if full_path starts with base_path
    if (strncmp(full_path, base_path, base_len) == 0) {
        // Skip past the base_path part
        const char *ptr = full_path + base_len;
        
        // Skip directory separators
        while (IS_DIR_SEPARATOR(*ptr)) {
            ptr++;
        }
        
        // Copy the relative path
        strncpy(relative_path, ptr, max_len - 1);
        relative_path[max_len - 1] = '\0';
    } else {
        // If paths don't match, use full path
        strncpy(relative_path, full_path, max_len - 1);
        relative_path[max_len - 1] = '\0';
    }
}
