/*
 * 以下是Linux内核中系统调用函数的外部声明
 * 这些函数是内核提供的核心服务接口，用户态程序通过系统调用指令触发
 */

// 系统初始化相关
extern int sys_setup(); // 系统启动初始化（早期内核使用）

// 进程管理相关
extern int sys_exit();      // 进程退出
extern int sys_fork();      // 创建子进程（复制当前进程）
extern int sys_waitpid();   // 等待子进程状态变化
extern int sys_execve();    // 执行新程序（替换进程镜像）
extern int sys_getpid();    // 获取当前进程ID
extern int sys_setuid();    // 设置进程用户ID
extern int sys_getuid();    // 获取进程用户ID
extern int sys_ptrace();    // 进程跟踪（调试用）
extern int sys_alarm();     // 设置闹钟信号
extern int sys_nice();      // 调整进程优先级
extern int sys_kill();      // 向进程发送信号
extern int sys_getppid();   // 获取父进程ID
extern int sys_getpgrp();   // 获取进程组ID
extern int sys_setsid();    // 创建新会话并设置进程组
extern int sys_setpgid();   // 设置进程组ID
extern int sys_signal();    // 设置信号处理函数
extern int sys_sigaction(); // 更复杂的信号处理设置
extern int sys_sgetmask();  // 获取信号掩码
extern int sys_ssetmask();  // 设置信号掩码
extern int sys_setreuid();  // 设置真实/有效用户ID
extern int sys_setregid();  // 设置真实/有效组ID
extern int sys_getgid();    // 获取进程组ID
extern int sys_setgid();    // 设置进程组ID
extern int sys_geteuid();   // 获取有效用户ID
extern int sys_getegid();   // 获取有效组ID
extern int sys_iam();       // 设置用户身份（自定义系统调用）
extern int sys_whoami();    // 获取当前用户身份（自定义系统调用）

// 文件系统相关
extern int sys_read();   // 从文件描述符读取数据
extern int sys_write();  // 向文件描述符写入数据
extern int sys_open();   // 打开文件
extern int sys_close();  // 关闭文件描述符
extern int sys_creat();  // 创建文件
extern int sys_link();   // 创建硬链接
extern int sys_unlink(); // 删除文件或链接
extern int sys_chdir();  // 改变当前工作目录
extern int sys_mknod();  // 创建特殊文件（设备文件等）
extern int sys_chmod();  // 修改文件权限
extern int sys_chown();  // 修改文件所有者
extern int sys_stat();   // 获取文件状态信息
extern int sys_lseek();  // 调整文件读写偏移量
extern int sys_fstat();  // 获取已打开文件的状态信息
extern int sys_utime();  // 修改文件访问和修改时间
extern int sys_access(); // 检查文件访问权限
extern int sys_rename(); // 重命名文件或目录
extern int sys_mkdir();  // 创建目录
extern int sys_rmdir();  // 删除目录
extern int sys_dup();    // 复制文件描述符
extern int sys_dup2();   // 复制文件描述符到指定编号
extern int sys_pipe();   // 创建管道
extern int sys_ioctl();  // 设备控制操作
extern int sys_fcntl();  // 文件描述符控制
extern int sys_lock();   // 文件锁定
extern int sys_chroot(); // 改变根目录
extern int sys_umask();  // 设置文件创建掩码
extern int sys_ustat();  // 获取文件系统统计信息

// 内存管理相关
extern int sys_break(); // 调整进程数据段大小（早期接口）
extern int sys_brk();   // 调整程序数据段结束地址
extern int sys_mpx();   // 内存保护扩展（兼容性接口）
extern int sys_phys();  // 物理内存访问（底层接口）

// 时间相关
extern int sys_time();  // 获取当前时间
extern int sys_stime(); // 设置系统时间
extern int sys_ftime(); // 获取精细时间
extern int sys_times(); // 获取进程时间统计
extern int sys_alarm(); // 设置闹钟

// 系统管理相关
extern int sys_mount();  // 挂载文件系统
extern int sys_umount(); // 卸载文件系统
extern int sys_acct();   // 进程记账（审计）
extern int sys_sync();   // 同步文件系统缓存到磁盘
extern int sys_ulimit(); // 获取/设置进程资源限制
extern int sys_uname();  // 获取系统标识信息
extern int sys_prof();   // 程序性能分析

// 终端相关
extern int sys_stty();  // 设置终端属性
extern int sys_gtty();  // 获取终端属性
extern int sys_pause(); // 暂停进程直到信号到来

/*
 * 系统调用表：存储系统调用函数指针的数组
 * 数组索引对应系统调用号，内核通过此表查找并执行对应的系统调用函数
 * 用户态程序通过系统调用号（如0、1、2...）指定需要调用的服务
 */
fn_ptr sys_call_table[] = {
    sys_setup, sys_exit, sys_fork, sys_read,
    sys_write, sys_open, sys_close, sys_waitpid, sys_creat, sys_link,
    sys_unlink, sys_execve, sys_chdir, sys_time, sys_mknod, sys_chmod,
    sys_chown, sys_break, sys_stat, sys_lseek, sys_getpid, sys_mount,
    sys_umount, sys_setuid, sys_getuid, sys_stime, sys_ptrace, sys_alarm,
    sys_fstat, sys_pause, sys_utime, sys_stty, sys_gtty, sys_access,
    sys_nice, sys_ftime, sys_sync, sys_kill, sys_rename, sys_mkdir,
    sys_rmdir, sys_dup, sys_pipe, sys_times, sys_prof, sys_brk, sys_setgid,
    sys_getgid, sys_signal, sys_geteuid, sys_getegid, sys_acct, sys_phys,
    sys_lock, sys_ioctl, sys_fcntl, sys_mpx, sys_setpgid, sys_ulimit,
    sys_uname, sys_umask, sys_chroot, sys_ustat, sys_dup2, sys_getppid,
    sys_getpgrp, sys_setsid, sys_sigaction, sys_sgetmask, sys_ssetmask,
    sys_setreuid, sys_setregid, sys_iam, sys_whoami};