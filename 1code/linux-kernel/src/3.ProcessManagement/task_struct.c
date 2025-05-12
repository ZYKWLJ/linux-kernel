#include "../../include/include.h"

T_task_struct T_task_struct_init(T_task_struct task_struct)
{
    LOG_PRINT("start initiation task info......\n");
    T_task_struct new_task_struct = (T_task_struct)checked_malloc(sizeof(*task_struct));
    LOG_PRINT("end initiation task info......\n");
    return new_task_struct;
}