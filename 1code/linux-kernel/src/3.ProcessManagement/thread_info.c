#include "../../include/include.h"
// #include "thread_info.h"
T_thread_info T_thread_info_init(T_thread_info thread_info)
{
    LOG_PRINT("start initiation thread info ......\n");
    T_thread_info new_thread_info = (T_thread_info)checked_malloc(sizeof(*thread_info));
    new_thread_info->task = T_task_struct_init(NULL);
    LOG_PRINT("end initiation thread info ......\n");
    return new_thread_info;
}
void test_thread_info()
{
    T_thread_info thread_info = T_thread_info_init(NULL);
}

#define TEST_THREAD_INFO
#ifdef TEST_THREAD_INFO
int main()
{
    test_thread_info();
}

#endif