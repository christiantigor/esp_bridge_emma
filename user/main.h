#ifndef __USER_MAIN_H__
#define __USER_MAIN_H__

#include <limits.h>
#include <stdlib.h>
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#include "eagle_soc.h"
#include "xtensa/hal.h"
#include "xtensa/xtruntime.h"
#include "driver/uart.h"
#include "mem.h"
#include "espconn.h"

#define TASK_COUNT					1
#define DEBUG_UART					0
#define FUNC_ATTR_NORMAL			ICACHE_FLASH_ATTR
#define ADJUST_ALLOC_BLOCK(n)		(((n) + 3) & ~3)

// bits in the 'flags' member of NetConn_t below
#define CONNECTED					0x0004
#define CLIENT_MODE					0x0008
#define BUFFERED_DATA				0x0010

#define AWAITING_CONNECT			0x0100
#define AWAITING_DISCONNECT			0x0200
#define AWAITING_TRANSMISSION		0x0400
#define OPERATION_TIMEOUT			0x0800

#define CONN_ALLOCATED				0x8000

#define PROTO_TCP					0
#define PROTO_UDP					1
#define PROTO_MAX					1

#define DEFAULT_TIMEOUT				3000		// in milliseconds

#define DEF_SERVER_TIMEOUT			(8 * 60 * 60)

// error codes
#define ERR_OK						0
#define ERR_MEM						-1
#define ERR_TIMEOUT					-3
#define ERR_CLSD					-10
#define ERR_CONN					-11
#define ERR_ARG						-12
#define ERR_ISCONN					-15
#define ERR_ERR						-30

// callback identifiers
#define NET_CONNECT					0
#define NET_DISCONNECT				1
#define NET_RX_DATA					2
#define NET_TX_DATA					3
#define NET_TX_FINISH				4
#define NET_RECONNECT				5

typedef union
{
	uint32_t w;
	uint8_t b[4];
} IPAddress_t;

typedef void (*NetCallback_t)(uint8_t type, int16_t hand, uint8_t *data, uint16_t cnt);

typedef struct _netBuf
{
	struct _netBuf *next;			// pointer to the next fragment
	uint16_t len;					// length of this fragment
	uint8_t data[1];				// actually 'zb_len' bytes in length
} NetBuf_t;

typedef struct
{
	struct espconn *conn;
	uint16_t flags;					// control flags
	uint16_t timeout;				// operation timeout in milliseconds
	NetCallback_t callback;			// callback for connect, data received, etc.
	os_timer_t timer;				// operation timer
	uint16_t tick;					// timeout tick accumulator
	int16_t chan;					// the channel
	NetBuf_t *txList;				// blocks of data waiting to be sent
} NetConn_t;

void mainTask(os_event_t *events);

uint32_t wifiGetIP(void);
int16_t netOpen(void);
int16_t netClose(int16_t chan);
int16_t netSetCallback(int16_t chan, NetCallback_t cb);
void netCallback(uint8_t cbtype, int16_t handle, uint8_t *data, uint16_t cnt);
int16_t netListen(int16_t chan, uint32_t port, uint8_t proto);
int16_t netSendStr(int16_t chan, const uint8_t *str);
int16_t netSendData(int16_t chan, const uint8_t *data, uint16_t len);
int16_t netSend(NetConn_t *ncp, const uint8_t *data, uint16_t cnt);
void dspData(const uint8_t *data, uint16_t cnt);
void dspIP(const char *caption, uint32_t ip);

void *memAlloc(size_t size);
void *memZalloc(size_t size);
void memFree(void *p);
void dbgInit(uint8_t chan, uint32_t baud);
void dbgChar(uint8_t val);
void dbgByteDec(uint8_t val);
void dbgWordDec(uint16_t val);
void dbgInt(int16_t val);
void dbgStr(const char *str);
void dbgStrN(const char *str, uint16_t len);
void dbgCRLF(void);

#endif	// __USER_MAIN_H__
