#include "../../include/include.h"

unsigned long long jiffies = 0;

void timer_interrupt_handler()
{

    jiffies++;
}

int main()
{
    /**
    * func descp: 实现了jiffies的定时中断！这里模拟的是100HZ，每秒100个时钟中断！jiffies增加100.
    */
    while (1)
    {
        timer_interrupt_handler();
        usleep(TICK_INTERVAL_MS * 1000); // 沉睡10ms
        if (jiffies % 100 == 0)
        {
            printf("jiffies: %llu\n", jiffies);
        }
    }
}