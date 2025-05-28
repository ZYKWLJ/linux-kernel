#include "../../include/include.h"
// #include "mm_segment.h"

M_mm_segment_t M_mm_segment_t_init(M_mm_segment_t mm_segment_t)
{
    LOG_PRINT("start to initiation mm_segment......");
    M_mm_segment_t new_mm_segment_t = (M_mm_segment_t)checked_malloc(sizeof(*mm_segment_t));
    LOG_PRINT("end to initiation mm_segment......");

    return new_mm_segment_t;
}