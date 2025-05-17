/**
 * filename:find_file.c
 * description: 搜索linux0.11下的文件
 * author:EthanYankang
 * create time:2025/05/17 15:41:46
 */
#include <stdio.h>
#include <string.h>
#define PATH "/home/eyk/1code/Linux-0.11code/py/line.txt"
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("usage: find_file <filename>\nPlease enter the filename you want to search for(only one name).\n");
    }
    FILE *fp = fopen(PATH, "r");
    if (fp == NULL)
    {
        printf("Failed to open file.\n");
    }
    for (int i = 1; i < argc; i++)
    {
        /**
         * data descp: then argv[1] is the filename we want to search for
         */
        char *filename = argv[i];
        /**
         * data descp: the root directory of linux0.11
         */

        char line[256];
        while (fgets(line, sizeof(line), fp))
        {
            if (strstr(line, filename) != NULL)
            {
                printf("%s", line);
            }
        }
        /**
        * data descp: rewind the file pointer to the beginning of the file is necessary
        */
        rewind(fp);
    }

    fclose(fp);
    return 0;
}