#ifndef _ERRNO_H
#define _ERRNO_H
// 声明一个外部全局整数变量errno。
// 作用：errno是 C 语言中用于保存最近一次系统调用或库函数错误状态的变量，由系统自动设置。
extern int errno;
/**
* data descp: 命名规则：以E开头，后续字母为错误描述的缩写（如EPERM即 "Error PERMission"）。
* 这些错误码对应系统调用或库函数执行失败的具体原因，应用程序可通过errno获取并判断错误类型。
*/
#define ERROR		99    // 定义通用错误码ERROR，值为 99（可能用于未明确分类的错误）。
#define EPERM		 1    // Operation not permitted（操作不允许：权限不足）
#define ENOENT		 2    // No such file or directory（无此文件或目录）
#define ESRCH		 3    // No such process（无此进程）
#define EINTR		 4    // Interrupted system call（系统调用被中断）
#define EIO		 5    // I/O error（输入/输出错误）
#define ENXIO		 6    // No such device or address（无此设备或地址）
#define E2BIG		 7    // Argument list too long（参数列表过长）
#define ENOEXEC		8    // Exec format error（执行格式错误：文件不可执行）
#define EBADF		 9    // Bad file descriptor（无效的文件描述符）
#define ECHILD		10   // No child processes（无子进程）
#define EAGAIN		11   // Try again（资源暂时不可用，可重试）
#define ENOMEM		12   // Out of memory（内存不足）
#define EACCES		13   // Permission denied（权限被拒绝）
#define EFAULT		14   // Bad address（无效的内存地址）
#define ENOTBLK		15   // Block device required（需要块设备）
#define EBUSY		16   // Device or resource busy（设备或资源正忙）
#define EEXIST		17   // File exists（文件已存在）
#define EXDEV		18   // Cross-device link（跨设备链接）
#define ENODEV		19   // No such device（无此设备）
#define ENOTDIR		20   // Not a directory（不是目录）
#define EISDIR		21   // Is a directory（是目录，而非文件）
#define EINVAL		22   // Invalid argument（无效的参数）
#define ENFILE		23   // File table overflow（系统打开文件数超限）
#define EMFILE		24   // Too many open files（进程打开文件数超限）
#define ENOTTY		25   // Not a typewriter（非终端设备，不支持终端操作）
#define ETXTBSY		26   // Text file busy（文本文件正忙，如执行中的程序）
#define EFBIG		27   // File too large（文件过大）
#define ENOSPC		28   // No space left on device（设备存储空间不足）
#define ESPIPE		29   // Illegal seek（非法的seek操作，如管道）
#define EROFS		30   // Read-only file system（只读文件系统）
#define EMLINK		31   // Too many links（链接数过多）
#define EPIPE		32   // Broken pipe（管道已断开）
#define EDOM		33   // Math argument out of domain of func（数学函数参数域错误）
#define ERANGE		34   // Math result not representable（数学结果超出表示范围）
#define EDEADLK		35   // Resource deadlock would occur（可能发生资源死锁）
#define ENAMETOOLONG	36   // File name too long（文件名过长）
#define ENOLCK		37   // No locks available（无可用锁）
#define ENOSYS		38   // Function not implemented（函数未实现）
#define ENOTEMPTY	39   // Directory not empty（目录非空）

#endif