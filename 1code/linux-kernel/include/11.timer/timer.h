#ifndef TIMER_H_
#define TIMER_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#define TICK_INTERVAL_MS 10

void timer_interrupt_handler();

#endif /* TIMER_H_ */