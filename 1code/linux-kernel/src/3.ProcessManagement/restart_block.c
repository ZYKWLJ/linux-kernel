#include "../../include/include.h"
// #include "restart_block.h"

Re_restart_block Re_restart_block_init(Re_restart_block restart_block)
{
    LOG_PRINT("start to initiation block......");
    Re_restart_block new_restart_block = (Re_restart_block)checked_malloc(sizeof(restart_block));
    LOG_PRINT("end to initiation block......");
    return new_restart_block;
}