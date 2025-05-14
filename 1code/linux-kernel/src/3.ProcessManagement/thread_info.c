#include "../../include/include.h"
// #include "thread_info.h"
void print_thread_info(T_thread_info thread_info)
{
    printf("this is all thread info:\n");
    printf("task:%p\n", thread_info->task);
    printf("exec_domain:%p\n", thread_info->exec_domain);
    printf("restart_block:%p\n", thread_info->restart_block);
    printf("addr_limit:%p\n", thread_info->addr_limit);
    printf("preempt_count:%d\n", thread_info->preempt_count);
    printf("cpu:%d\n", thread_info->cpu);
    printf("flags:%d\n", thread_info->flags);
    printf("status:%d\n", thread_info->status);
    printf("sysenter_return:%d\n", thread_info->sysenter_return);
    printf("uaccess_err:%d\n", thread_info->uaccess_err);
    return;
}
// #include "thread_info.h"
T_thread_info T_thread_info_init(T_thread_info thread_info)
{
    int ramdom;
    srand(time(NULL));
    LOG_PRINT("start initiation thread info ......");
    T_thread_info new_thread_info = (T_thread_info)checked_malloc(sizeof(*thread_info));
    new_thread_info->task = T_task_struct_init(NULL);
    new_thread_info->exec_domain = E_exec_domain_init(NULL);
    new_thread_info->restart_block = Re_restart_block_init(NULL);
    new_thread_info->addr_limit = M_mm_segment_t_init(NULL);
    new_thread_info->preempt_count = (rand() % 10);
    new_thread_info->cpu = (rand() % 10) + 1;
    new_thread_info->flags = (rand() % 10) + 1;
    new_thread_info->status = (rand() % 10) + 1;
    new_thread_info->sysenter_return = (rand() % 10) + 1;
    new_thread_info->uaccess_err = (rand() % 10) + 1;
    LOG_PRINT("end initiation thread info ......");
    return new_thread_info;
}
void test_thread_info()
{
    T_thread_info thread_info = T_thread_info_init(NULL);
    print_thread_info(thread_info);
}

// #define TEST_THREAD_INFO
#ifdef TEST_THREAD_INFO

int main()
{
    test_thread_info();
}

#endif