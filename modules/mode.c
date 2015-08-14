#include "mode.h"
#include "ringbuf.h"
#include "smartconfig.h"

os_event_t    	modeRecvQueue[MODE_TASK_QUEUE_SIZE];
RINGBUF 		rxdRb;
uint8_t			rxdBuf[256];
uint8_t			modeBuf[6];
uint8_t			idx;

void ICACHE_FLASH_ATTR
MODE_Init()
{
	os_printf("MODE: INIT\r\n");
	RINGBUF_Init(&rxdRb, rxdBuf, sizeof(rxdBuf));

	system_os_task(MODE_Task, MODE_TASK_PRIO, modeRecvQueue, MODE_TASK_QUEUE_SIZE);
	system_os_post(MODE_TASK_PRIO, 0, 0);
	idx = 0;
}

void ICACHE_FLASH_ATTR
MODE_Input(uint8_t data)
{
	RINGBUF_Put(&rxdRb, data);
	system_os_post(MODE_TASK_PRIO, 0, 0);
}

static void ICACHE_FLASH_ATTR MODE_Task(os_event_t *events)
{
	uint8_t c;
	smartconfig_stop();
	
	while(RINGBUF_Get(&rxdRb, &c) == 0){
		if (c=='M') { idx=0; }
		modeBuf[idx] = c;
#ifdef DEBUG
		os_printf("modeBuf[%d]: %c\n", idx, modeBuf[idx]);
#endif
		if (idx == 5) {
			idx = 0;
			if ((modeBuf[0]=='M') && (modeBuf[1]=='O') && (modeBuf[2]=='D') && (modeBuf[3]=='E') && (modeBuf[4]=='=')) {
				if (modeBuf[5] == 'S') { // SERVER MODE
					SERVER_Init();
				}
				if (modeBuf[5] == 'C') { // CONFIG MODE
					SCONFIG_Init();
				}
				if (modeBuf[5] == 'B') { // OPERATIONAL SLIP BRIDGE MODE
					CMD_Init();
					os_printf("MODE=B_OK\n"); 					
				}
			}
		}
		idx++;
	}
}