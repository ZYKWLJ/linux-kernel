#ifndef _STRING_H_
#define _STRING_H_
/*
 * 条件编译预处理指令：防止头文件被重复包含
 * 当第一次包含该头文件时，_STRING_H_未定义，会执行#define _STRING_H_
 * 后续再次包含时，由于_STIRNG_H_已定义，会跳过整个文件内容
 */

#ifndef NULL
#define NULL ((void *)0)
/*
 * 定义NULL宏：表示空指针
 * 如果当前环境中未定义NULL，则将其定义为(void *)0
 * 空指针在C语言中用于表示指针不指向任何有效的内存地址
 */
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
/*
 * 定义size_t类型：无符号整数类型，用于表示内存大小和对象尺寸
 * 在标准C中，size_t通常用于sizeof运算符的返回值、内存分配函数的参数等
 * 这里定义为unsigned int，实际实现可能因系统而异
 */
#endif

extern char *strerror(int errno);
/*
 * 声明strerror函数：获取错误信息字符串
 * 参数：错误码(errno)
 * 返回值：指向对应错误信息字符串的指针
 */

/*
 * 以下注释说明该头文件的实现特点：
 * 1. 所有字符串函数都定义为内联(inline)函数，需要使用gcc编译器
 * 2. 假设ds=es=data space(数据段)，这在正常情况下是成立的
 * 3. 大多数字符串函数都经过了高度手工优化，特别是strtok、strstr、str[c]spn
 * 4. 这些函数可能不易理解，但能正常工作
 * 5. 所有操作都在寄存器中完成，使函数快速且简洁
 * 6. 全程使用字符串指令，使得代码"稍微"有些不清晰 :-)
 *
 * 版权信息：(C) 1991 Linus Torvalds(林纳斯·托瓦兹，Linux内核创始人)
 */

// 声明字符串复制函数：将src指向的字符串复制到dest
extern inline char *strcpy(char *dest, const char *src);
// 声明字符串拼接函数：将src指向的字符串追加到dest指向的字符串末尾
extern inline char *strcat(char *dest, const char *src);
// 声明字符串比较函数：比较cs和ct指向的字符串
extern inline int strcmp(const char *cs, const char *ct);
// 声明字符串前缀匹配函数：计算cs中连续匹配ct中字符的长度
extern inline int strspn(const char *cs, const char *ct);
// 声明字符串前缀不匹配函数：计算cs中连续不匹配ct中字符的长度
extern inline int strcspn(const char *cs, const char *ct);
// 声明字符串字符查找函数：查找cs中第一个出现在ct中的字符
extern inline char *strpbrk(const char *cs, const char *ct);
// 声明子字符串查找函数：在cs中查找ct子字符串第一次出现的位置
extern inline char *strstr(const char *cs, const char *ct);
// 声明字符串长度计算函数：计算字符串s的长度(不包含结束符'\0')
extern inline int strlen(const char *s);

extern char *___strtok;
/*
 * 声明strtok函数使用的静态变量：用于保存字符串分割的状态
 * 在多线程环境下可能存在问题，因为静态变量会被所有线程共享
 */

// 声明字符串分割函数：将字符串s按ct中的分隔符进行分割
extern inline char *strtok(char *s, const char *ct);

/*
 * falcon<zhangjinw@gmail.com>的修改说明：
 * 原始实现的返回值是static inline，这导致其他文件中的函数无法调用这些函数
 * 此处修改为extern inline，解决了跨文件调用的问题
 */

// 声明内存复制函数：将src指向的n个字节复制到dest
extern inline void *memcpy(void *dest, const void *src, int n);
// 声明内存移动函数：将src指向的n个字节移动到dest(可处理重叠内存块)
extern inline void *memmove(void *dest, const void *src, int n);
// 声明内存字符查找函数：在cs指向的count个字节中查找字符c
extern inline void *memchr(const void *cs, char c, int count);

#endif // 结束#ifndef _STRING_H_的条件编译块
