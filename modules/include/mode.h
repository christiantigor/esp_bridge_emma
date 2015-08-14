#ifndef __USER_MODE_H__
#define __USER_MODE_H__

#define MODE_TASK_QUEUE_SIZE 1
#define MODE_TASK_PRIO		1

#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"

void ICACHE_FLASH_ATTR MODE_Init();
void ICACHE_FLASH_ATTR MODE_Input(uint8_t data);
static void ICACHE_FLASH_ATTR MODE_Task(os_event_t *events);

#endif // __USER_MODE_H__