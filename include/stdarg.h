#ifndef _STDARG_H
#define _STDARG_H

typedef char *va_list; /* 指向可变参数列表的指针 */

/* 定义 va_start 宏，用于初始化 va_list 指针 AP，指向参数列表中的最后一个固定参数 LASTARG。
   该宏计算 LASTARG 之后的偏移量，确保 AP 指向第一个可变参数的起始位置。
   偏移量计算方法：(LASTARG 地址 + 固定参数大小 + 3) & ~3，确保对齐到 4 字节边界。 */

/* Amount of space required in an argument list for an arg of type TYPE.
   TYPE may alternatively be an expression whose type is used.  */

// 计算 “某种类型（或表达式的类型）的参数在函数参数列表中实际占用的内存空间”（对齐到4字节边界）
// 公式 (size + align - 1) / align * align 是计算 “向上取整到对齐值倍数” 的通用写法。

#define __va_rounded_size(TYPE) \
    (((sizeof(TYPE) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))

#ifndef __sparc__

/* 初始化 va_list 指针 AP，使其指向第一个可变参数的位置,即...之后的第一个可变参数。*/
// AP：va_list 类型的指针（用于后续访问可变参数）。
// LASTARG：函数中最后一个固定参数（即 ... 前的参数，如 printf 中的 fmt），
// 此处加上这个参数的大小后，后面的栈空间就是存放的可变参数，如此进行初始化。

#define va_start(AP, LASTARG) \
    (AP = ((char *)&(LASTARG) + __va_rounded_size(LASTARG)))
#else
// 特殊处理：__sparc__ 架构（SPARC 处理器）需要先调用 __builtin_saveregs() 保存寄存器，再初始化指针（与架构的调用约定相关）。
#define va_start(AP, LASTARG) \
    (__builtin_saveregs(),    \
     AP = ((char *)&(LASTARG) + __va_rounded_size(LASTARG)))
#endif

void va_end(va_list); /* Defined in gnulib */

// 结束可变参数的访问，释放相关资源（此处简化实现为空宏）。
#define va_end(AP)

// 从可变参数列表中获取下一个类型为 TYPE 的参数，并更新 AP 指向后续参数。
#define va_arg(AP, TYPE)            \
    (AP += __va_rounded_size(TYPE), \
     *((TYPE *)(AP - __va_rounded_size(TYPE))))

#endif /* _STDARG_H */
