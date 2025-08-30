/*
 * 字符分类与转换头文件（ctype.h）
 * 定义字符类型判断宏（如判断字母、数字、控制字符）和字符大小写转换宏
 * 遵循C语言标准库中ctype.h的核心功能，适配Linux 0.11内核环境
 */
#ifndef _CTYPE_H // 防止头文件重复包含：若未定义_CTYPE_H，则进入定义
#define _CTYPE_H

/*
 * 字符类型标志位定义（用于_ctype数组存储字符属性）
 * 每个字符的属性用8位二进制表示，不同位对应不同类型
 */
#define _U 0x01  /* upper：表示字符为大写字母（A-Z） */
#define _L 0x02  /* lower：表示字符为小写字母（a-z） */
#define _D 0x04  /* digit：表示字符为数字（0-9） */
#define _C 0x08  /* cntrl：表示字符为控制字符（如ASCII 0-31和127） */
#define _P 0x10  /* punct：表示字符为标点符号（如!、,、;等） */
#define _S 0x20  /* white space：表示字符为空白字符（空格、换行符、制表符等） */
#define _X 0x40  /* hex digit：表示字符为十六进制数字（0-9、A-F、a-f） */
#define _SP 0x80 /* hard space：表示字符为硬空格（即ASCII 0x20，普通空格） */

/*
 * 外部变量声明：字符属性数组和临时变量
 * _ctype[]：存储每个ASCII字符的属性（索引为字符ASCII值，值为上述标志位的组合）
 * _ctmp：临时变量，用于大小写转换宏（避免多次计算）
 */
extern unsigned char _ctype[];
extern char _ctmp;

/*
 * 字符类型判断宏：通过_ctype数组查询字符属性，按位与判断是否属于目标类型
 * 注：(_ctype + 1)[c] 等价于 _ctype[c + 1]，因_ctype数组通常从索引1开始存储ASCII 0的属性（索引0留空）
 */
#define isalnum(c) ((_ctype + 1)[c] & (_U | _L | _D))            // 判断是否为字母或数字（A-Z、a-z、0-9）
#define isalpha(c) ((_ctype + 1)[c] & (_U | _L))                 // 判断是否为字母（A-Z、a-z）
#define iscntrl(c) ((_ctype + 1)[c] & (_C))                      // 判断是否为控制字符（ASCII 0-31、127）
#define isdigit(c) ((_ctype + 1)[c] & (_D))                      // 判断是否为数字（0-9）
#define isgraph(c) ((_ctype + 1)[c] & (_P | _U | _L | _D))       // 判断是否为可打印非空白字符（标点、字母、数字）
#define islower(c) ((_ctype + 1)[c] & (_L))                      // 判断是否为小写字母（a-z）
#define isprint(c) ((_ctype + 1)[c] & (_P | _U | _L | _D | _SP)) // 判断是否为可打印字符（含空格）
#define ispunct(c) ((_ctype + 1)[c] & (_P))                      // 判断是否为标点符号
#define isspace(c) ((_ctype + 1)[c] & (_S))                      // 判断是否为空白字符（空格、\t、\n、\r等）
#define isupper(c) ((_ctype + 1)[c] & (_U))                      // 判断是否为大写字母（A-Z）
#define isxdigit(c) ((_ctype + 1)[c] & (_D | _X))                // 判断是否为十六进制数字（0-9、A-F、a-f）

/*
 * ASCII字符判断与转换宏
 * isascii：判断字符是否为ASCII字符（ASCII值0-127）
 * toascii：将字符转换为ASCII字符（保留低7位，清除高位）
 */
#define isascii(c) (((unsigned)c) <= 0x7f) // 无符号类型转换避免负数判断错误（如扩展ASCII的高字节）
#define toascii(c) (((unsigned)c) & 0x7f)  // 按位与0x7f（二进制01111111），清除高位，保留ASCII部分

/*
 * 字符大小写转换宏
 * tolower：将大写字母转为小写（若为大写则减'A'-'a'的差值，即32；否则保持原字符）
 * toupper：将小写字母转为大写（若为小写则减'a'-'A'的差值，即-32；否则保持原字符）
 * 注：使用_ctmp临时变量存储输入c，避免多次计算c（如c为表达式时减少开销）
 */
#define tolower(c) (_ctmp = c, isupper(_ctmp) ? _ctmp - ('A' - 'a') : _ctmp)
#define toupper(c) (_ctmp = c, islower(_ctmp) ? _ctmp - ('a' - 'A') : _ctmp)

#endif // 结束_CTYPE_H的定义