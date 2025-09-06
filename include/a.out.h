#ifndef _A_OUT_H
#define _A_OUT_H

/*
 * 头文件保护宏：防止重复包含导致的编译错误（如结构体重定义、宏重复定义）
 * 首次包含时，_A_OUT_H未定义，执行后续内容；重复包含时，直接跳过
 */

#define __GNU_EXEC_MACROS__ // 启用GNU风格的可执行文件相关宏定义，标识该头文件遵循GNU扩展规范

/**
 * @brief 可执行文件/目标文件的头部结构（a.out格式核心元数据）
 * 存储文件的关键信息，供加载器（如内核）解析文件结构、分配内存、执行程序
 */
struct exec
{
    unsigned long a_magic; /* 魔数（文件类型标识）：通过N_MAGIC等宏解析，区分可执行文件/目标文件类型 */
    unsigned a_text;       /* 代码段（text segment）长度（字节）：存储程序指令的区域 */
    unsigned a_data;       /* 数据段（data segment）长度（字节）：存储已初始化全局/静态变量的区域 */
    unsigned a_bss;        /* BSS段长度（字节）：存储未初始化全局/静态变量的区域（加载时分配内存，文件中不占空间） */
    unsigned a_syms;       /* 符号表（symbol table）长度（字节）：存储变量名、函数名与地址映射的区域 */
    unsigned a_entry;      /* 程序入口地址：内核加载程序后，第一个执行的指令地址 */
    unsigned a_trsize;     /* 代码段重定位信息长度（字节）：修正代码段中地址引用的元数据（目标文件中有效） */
    unsigned a_drsize;     /* 数据段重定位信息长度（字节）：修正数据段中地址引用的元数据（目标文件中有效） */
};

/* 魔数解析宏：若未定义N_MAGIC，提供默认实现，从struct exec中提取魔数 */
#ifndef N_MAGIC
#define N_MAGIC(exec) ((exec).a_magic)
#endif

/* 可执行文件类型魔数定义（a.out格式的核心类型标识） */
#ifndef OMAGIC
#define OMAGIC 0407 /* 老式可执行文件/目标文件：代码段与数据段未分离，加载后不考虑页对齐 */
#define NMAGIC 0410 /* 纯可执行文件：代码段与数据段分离，加载时按段对齐（无共享页机制） */
#define ZMAGIC 0413 /* 按需分页可执行文件（现代常用）：支持内存分页加载，仅在需要时读取文件内容到内存 */
#endif              /* not OMAGIC */

/* 检查魔数合法性的宏：判断文件是否为支持的a.out格式（非OMAGIC/NMAGIC/ZMAGIC则为非法） */
#ifndef N_BADMAG
#define N_BADMAG(x) \
    (N_MAGIC(x) != OMAGIC && N_MAGIC(x) != NMAGIC && N_MAGIC(x) != ZMAGIC)
#endif

/* 与N_BADMAG功能相同的宏，用于兼容不同代码场景 */
#define _N_BADMAG(x) \
    (N_MAGIC(x) != OMAGIC && N_MAGIC(x) != NMAGIC && N_MAGIC(x) != ZMAGIC)

/* 计算ZMAGIC格式文件头部偏移：ZMAGIC文件头部位于第一个段（通常是代码段）之前，偏移为“段大小 - 头部结构体大小” */
#define _N_HDROFF(x) (SEGMENT_SIZE - sizeof(struct exec))

/* 计算代码段在文件中的偏移（不同格式文件的代码段起始位置不同） */
#ifndef N_TXTOFF
#define N_TXTOFF(x) \
    (N_MAGIC(x) == ZMAGIC ? _N_HDROFF((x)) + sizeof(struct exec) : sizeof(struct exec))
/* ZMAGIC：头部偏移 + 头部大小；其他格式：直接从文件起始位置（头部之后）开始 */
#endif

/* 计算数据段在文件中的偏移：代码段偏移 + 代码段长度 */
#ifndef N_DATOFF
#define N_DATOFF(x) (N_TXTOFF(x) + (x).a_text)
#endif

/* 计算代码段重定位信息在文件中的偏移：数据段偏移 + 数据段长度 */
#ifndef N_TRELOFF
#define N_TRELOFF(x) (N_DATOFF(x) + (x).a_data)
#endif

/* 计算数据段重定位信息在文件中的偏移：代码段重定位偏移 + 代码段重定位长度 */
#ifndef N_DRELOFF
#define N_DRELOFF(x) (N_TRELOFF(x) + (x).a_trsize)
#endif

/* 计算符号表在文件中的偏移：数据段重定位偏移 + 数据段重定位长度 */
#ifndef N_SYMOFF
#define N_SYMOFF(x) (N_DRELOFF(x) + (x).a_drsize)
#endif

/* 计算字符串表在文件中的偏移：符号表偏移 + 符号表长度（字符串表存储符号名的原始字符串） */
#ifndef N_STROFF
#define N_STROFF(x) (N_SYMOFF(x) + (x).a_syms)
#endif

/* 代码段加载到内存后的起始地址：默认0（早期系统中代码段从内存低地址开始，具体由硬件/内核决定） */
#ifndef N_TXTADDR
#define N_TXTADDR(x) 0
#endif

/* 段大小定义（不同架构/系统的段大小可能不同，此处提供常见架构的默认值） */
#if defined(vax) || defined(hp300) || defined(pyr)
#define SEGMENT_SIZE PAGE_SIZE // VAX、HP300等架构：段大小 = 页大小
#endif
#ifdef hp300
#define PAGE_SIZE 4096 // HP300架构：页大小4096字节（1页）
#endif
#ifdef sony
#define SEGMENT_SIZE 0x2000 // Sony架构：段大小8192字节
#endif                      /* Sony.  */
#ifdef is68k
#define SEGMENT_SIZE 0x20000 // 68000架构（部分）：段大小131072字节
#endif
#if defined(m68k) && defined(PORTAR)
#define PAGE_SIZE 0x400        // 68000架构（PORTAR）：页大小1024字节
#define SEGMENT_SIZE PAGE_SIZE // 段大小 = 页大小
#endif

/* 默认页大小与段大小（未匹配上述架构时使用，通用值） */
#define PAGE_SIZE 4096    // 标准页大小：4096字节（现代系统通用）
#define SEGMENT_SIZE 1024 // 默认段大小：1024字节（兼容早期系统）

/* 段对齐宏：将地址/长度向上对齐到SEGMENT_SIZE的整数倍（内存分配时确保段对齐） */
#define _N_SEGMENT_ROUND(x) (((x) + SEGMENT_SIZE - 1) & ~(SEGMENT_SIZE - 1))

/* 代码段在内存中的结束地址：代码段起始地址 + 代码段长度 */
#define _N_TXTENDADDR(x) (N_TXTADDR(x) + (x).a_text)

/* 数据段加载到内存后的起始地址（不同格式文件的对齐规则不同） */
#ifndef N_DATADDR
#define N_DATADDR(x)                           \
    (N_MAGIC(x) == OMAGIC ? (_N_TXTENDADDR(x)) \
                          : (_N_SEGMENT_ROUND(_N_TXTENDADDR(x))))
/* OMAGIC：数据段紧跟代码段（无对齐）；其他格式：数据段在代码段对齐后的地址开始 */
#endif

/* BSS段加载到内存后的起始地址：数据段起始地址 + 数据段长度（BSS段无文件内容，仅内存分配） */
#ifndef N_BSSADDR
#define N_BSSADDR(x) (N_DATADDR(x) + (x).a_data)
#endif

/* 符号表条目结构（未声明过则定义）：存储单个符号的元数据（如函数名、变量名及其地址属性） */
#ifndef N_NLIST_DECLARED
struct nlist
{
    union // 符号名相关字段（共用体：根据场景存储符号名指针、链表指针或字符串表偏移）
    {
        char *n_name;         // 符号名指针（内存中解析时使用，指向符号名字符串）
        struct nlist *n_next; // 符号链表指针（用于符号表遍历）
        long n_strx;          // 符号名在字符串表中的偏移（文件中存储时使用，节省空间）
    } n_un;
    unsigned char n_type;  // 符号类型（如N_TEXT表示代码段符号、N_DATA表示数据段符号）
    char n_other;          // 保留字段（暂未使用，通常为0）
    short n_desc;          // 符号描述（如重定位相关信息，部分场景使用）
    unsigned long n_value; // 符号的值（如函数地址、变量地址、常量值）
};
#endif

/* 符号类型宏定义（标识符号所属的内存区域或属性） */
#ifndef N_UNDF
#define N_UNDF 0 // 未定义符号（如外部引用的函数/变量，需链接时解析）
#endif
#ifndef N_ABS
#define N_ABS 2 // 绝对符号（值为常量，不依赖内存地址，如#define定义的常量）
#endif
#ifndef N_TEXT
#define N_TEXT 4 // 代码段符号（如函数、静态代码中的变量）
#endif
#ifndef N_DATA
#define N_DATA 6 // 数据段符号（如已初始化的全局/静态变量）
#endif
#ifndef N_BSS
#define N_BSS 8 // BSS段符号（如未初始化的全局/静态变量）
#endif
#ifndef N_COMM
#define N_COMM 18 // 公共符号（如未初始化的全局变量，链接时分配内存）
#endif
#ifndef N_FN
#define N_FN 15 // 函数名符号（用于调试信息，标识函数边界）
#endif

/* 符号属性宏定义（补充符号类型的属性） */
#ifndef N_EXT
#define N_EXT 1 // 外部符号（可被其他文件引用，如extern声明的变量/函数）
#endif
#ifndef N_TYPE
#define N_TYPE 036 // 符号类型掩码（用于从n_type中提取类型部分，屏蔽其他属性位）
#endif
#ifndef N_STAB
#define N_STAB 0340 // 调试符号掩码（用于标识调试相关的符号，如行号、变量作用域）
#endif

/* 间接符号类型：标识符号是对另一个符号的间接引用（链接时需用目标符号的值替换） */
#define N_INDR 0xa

/* 集合符号类型（用于定义符号集合，如初始化函数表、全局变量集合） */
#define N_SETA 0x14 /* 绝对集合元素符号（值为绝对地址，无内存依赖） */
#define N_SETT 0x16 /* 代码段集合元素符号（属于代码段的集合元素） */
#define N_SETD 0x18 /* 数据段集合元素符号（属于数据段的集合元素） */
#define N_SETB 0x1A /* BSS段集合元素符号（属于BSS段的集合元素） */
#define N_SETV 0x1C /* 集合向量符号（指向集合在数据段中的存储地址，链接器输出） */

/* 重定位信息结构（未声明过则定义）：修正目标文件中地址引用的元数据（链接/加载时使用） */
#ifndef N_RELOCATION_INFO_DECLARED
struct relocation_info
{
    int r_address; // 需重定位的地址（相对于所在段的偏移量，如代码段内的偏移）

    /* 位域：用32位存储多个属性（节省空间，按位划分功能） */
    unsigned int r_symbolnum : 24;
    // 符号索引：
    // - r_extern=1时：符号表中符号的索引（需用该符号的值修正地址）
    // - r_extern=0时：标识段类型（如N_TEXT/N_DATA，需用段地址修正）
    unsigned int r_pcrel : 1;  // PC相对标识：1=地址是PC相对偏移（需结合当前指令地址修正），0=绝对地址
    unsigned int r_length : 2; // 重定位字段长度：值为n表示字段占2^n字节（如0=1字节，1=2字节，2=4字节）
    unsigned int r_extern : 1; // 外部符号标识：1=引用外部符号，0=引用段地址
    unsigned int r_pad : 4;    // 填充位：未使用，保留为0（确保结构对齐）
};
#endif /* no N_RELOCATION_INFO_DECLARED.  */

#endif /* _A_OUT_H */ // 闭合头文件保护宏