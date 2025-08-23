#ifndef _STDDEF_H
#define _STDDEF_H

/*
 * 定义ptrdiff_t类型：用于表示两个指针相减的结果类型
 * 条件编译确保该类型只被定义一次
 */
#ifndef _PTRDIFF_T      // 检查是否已定义_PTRDIFF_T宏（防止重复定义）
#define _PTRDIFF_T      // 定义宏标记ptrdiff_t已被定义
typedef long ptrdiff_t; // 定义ptrdiff_t为long类型，用于存储指针差值
#endif

/*
 * 定义size_t类型：用于表示对象大小（如sizeof运算符的返回类型）
 * 条件编译确保该类型只被定义一次
 */
#ifndef _SIZE_T               // 检查是否已定义_SIZE_T宏
#define _SIZE_T               // 定义宏标记size_t已被定义
typedef unsigned long size_t; // 定义size_t为无符号长整型，用于表示大小/长度
#endif

/*
 * 定义空指针常量NULL
 * 先取消可能存在的NULL定义，再重新定义为(void *)0
 */
#undef NULL              // 取消之前可能的NULL定义（确保一致性）
#define NULL ((void *)0) // 标准空指针定义：将0强制转换为void*类型

/*
 * 计算结构体成员相对于结构体起始地址的偏移量
 * TYPE：结构体类型
 * MEMBER：结构体中的成员名
 * 原理：将0地址强制转换为TYPE*指针，获取该指针的MEMBER成员地址，
 *       由于起始地址为0，成员地址即为该成员相对于结构体起始位置的偏移量
 */
#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)

#endif // 结束头文件保护宏