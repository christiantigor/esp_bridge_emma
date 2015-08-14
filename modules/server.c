#include "server.h"

// uncomment one of the following lines to enable a server
#define HTTP_SERVER
//#define UDP_SERVER

#define DEBUG

#if defined(HTTP_SERVER) && defined(UDP_SERVER)
  #error the code supports only one server
#endif

#define UDP_PORT				8888
#define TCP_PORT				80

#define MAX_NET_CONN			20

os_event_t procTaskQueue[TASK_COUNT];
uint8_t dbgChan;
uint16_t loopCount;
uint16_t state;
uint8_t connected;
int16_t handle = -1;
int16_t stat;

#if defined(HTTP_SERVER)
// header information to send when requested content is not found
uint8_t notFoundHeader[] =
	"emmaModeSettings_OK\r\n"
	"Content-Length: 0\r\n"
	"\r\n";

// header information to send when there is no content available
uint8_t noContentHeader[] =
	"HTTP/1.1 204 No Content\r\n"
	"Content-Length: 0\r\n"
	"\r\n";

// header information for a successful request
uint8_t normalHeader[] =
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: text/html; charset=ISO-8859-1\r\n"
	"Cache-Control: no-cache\r\n"
	"Transfer-Encoding: chunked\r\n"
	"\r\n";

// webserver content
uint8_t myHTML_head[] =
	"<html><body>\r\n"
	"<title>Web Server</title>\r\n"
	"<center><h2><font color=""#0000FF"">ESP8266</font></h2>\r\n"
	"This is my esp8266 web server page.<br><br>\r\n";

// put other content here

// web page tail
uint8_t myHTML_tail[] =
	"</center></body></html>\r\n"
	"\r\n";
#endif

NetConn_t *netConn[MAX_NET_CONN];

static int16_t isRequest(const uint8_t *data, uint16_t cnt, const char *request);
static void sendDoneCB(void *arg);
static void udpReceiveCB(void *arg, char *data, uint16_t cnt);
static void tcpConnectCB(void *arg);
static void sendChunkSize(int16_t handle, uint16_t chunkLen);
static void dropConnection(NetConn_t *ncp);

void SERVER_Init(void)
{
	struct station_config config;

	state = 0;
	loopCount = 0;
	
	system_os_task(serverTask, 0, procTaskQueue, TASK_COUNT);
	system_os_post(0, 0, 0);
}

void ICACHE_FLASH_ATTR
serverTask(os_event_t *events)
{
	if (++loopCount >= 1000)
	{
		loopCount = 0;
		if (state < 10)
		{
			uint8_t wifiStat = wifi_station_get_connect_status();

			#ifdef DEBUG
			os_printf("state = %d", state);
			os_printf(", WiFi status is %d\n", wifiStat);
			#endif

			if (!connected & (wifiStat == 5))
			{
				uint32_t addr = wifiGetIP();
				os_printf("connected, IP = %d.%d.%d.%d\n", IP2STR(&addr));
				connected = 1;
				if (IP2STR(&addr))
				{
					#ifdef DEBUG
					os_printf("connected, IP = %d.%d.%d.%d\n", IP2STR(&addr));
					#endif
					
					handle = netOpen();
					if (handle >= 0)
					{
						stat = netSetCallback(handle, (NetCallback_t)netCallback);
#if defined(HTTP_SERVER)
						stat = netListen(handle, TCP_PORT, PROTO_TCP);
#endif
#if defined(UDP_SERVER)
						stat = netListen(handle, UDP_PORT, PROTO_UDP);
#endif	
						os_printf("Net.Listen() returns %d\n", stat);
					}
				}
			}
			if (state == 9) { os_printf("MODE=S_OK\n"); }
		}
		if (++state > 1000)
			state = 1000;
	}
    os_delay_us(1000);
	system_os_post(0, 0, 0);
}

void ICACHE_FLASH_ATTR
netCallback(uint8_t cbtype, int16_t handle, uint8_t *data, uint16_t cnt)
{
#ifdef DEBUG
	os_printf("chan %d\n", handle);
#endif
	
	switch(cbtype)
	{
	case NET_CONNECT:
#ifdef DEBUG
		os_printf("cb: connect\r\n");
#endif
		break;

	case NET_DISCONNECT:
#ifdef DEBUG
		os_printf("cb: disconnect\r\n");
#endif
		break;

	case NET_RX_DATA:
#ifdef DEBUG
		os_printf("cb: receive %d\n", cnt);
#endif
		dspData(data, cnt);

#if defined(UDP_SERVER)
		// send a reply to the client
		netSendStr(handle, "Goodbye, world!");
#endif
#if defined(HTTP_SERVER)
		// This is a simple validation of the HTTP request.  It should be more
		// fully validated but this is fine for a quick test.
		if (isRequest(data, cnt, "GET / ") || isRequest(data, cnt, "GET /index.html "))
		{
			uint16_t chunkLen;

			// send the content in segments
			netSendStr(handle, normalHeader);
			chunkLen = strlen(myHTML_head);
			sendChunkSize(handle, chunkLen - 2);
			netSendData(handle, myHTML_head, chunkLen);

// add other content here

			chunkLen = strlen(myHTML_tail);
			sendChunkSize(handle, chunkLen - 2);
			netSendData(handle, myHTML_tail, chunkLen);

			// send a chunk size of zero signalling the end of the content
			sendChunkSize(handle, 0);
		}
		else if (isRequest(data, cnt, "GET /favicon.ico "))
			netSendStr(handle, noContentHeader);
		else
			netSendStr(handle, notFoundHeader);
#endif
		break;

	case NET_TX_DATA:
		os_printf("cb: send\r\n");
#if defined(UDP_SERVER)
		// close the handle now that the UDP response has been sent
		netClose(handle);
#endif
		break;

	default:
		os_printf("cb: unknown (");
		dbgWordDec(cbtype);
		os_printf(")\r\n");
		break;
	}
}

int16_t ICACHE_FLASH_ATTR
netOpen(void)
{
	os_printf("netOpen\n");
	int idx;
	for (idx = 0; idx < MAX_NET_CONN; idx++)
	{
		if (netConn[idx] == NULL)
		{			
			// attempt to allocate a control structure
			NetConn_t *ncp = memZalloc(sizeof(*ncp));
			if (ncp == NULL){
				os_printf("ERR MEM\n");
				return(ERR_MEM);
			}
			// store the pointer to the control structure and initialize some elements
			netConn[idx] = ncp;
			ncp->chan = idx;
			ncp->timeout = DEFAULT_TIMEOUT;
			return(idx);
		}
	}
	return(ERR_ERR);	
}

int16_t ICACHE_FLASH_ATTR
netClose(int16_t chan)
{
	int16_t stat = ERR_OK;
	NetConn_t *ncp;

	if ((chan < 0) || (chan >= MAX_NET_CONN))
		return(ERR_ARG);
	if ((ncp = netConn[chan]) == NULL)
		return(ERR_CLSD);
	if (ncp->conn != NULL)
	{
		NetCallback_t cb = NULL;
		switch (ncp->conn->type)
		{
#if defined(HTTP_SERVER)
		case ESPCONN_TCP:
			if (ncp->flags & CONNECTED)
			{
				stat = espconn_disconnect(ncp->conn);
				cb = ncp->callback;
			}
			else
				stat = ERR_OK;
			break;
#endif

#if defined(UDP_SERVER)
		case ESPCONN_UDP:
			if (ncp->flags & CONNECTED)
				cb = ncp->callback;
			stat = ERR_OK;
			break;
#endif
		default:
			// this shouldn't happen
			break;
		}

		dropConnection(ncp);
		if (cb)
			(*cb)(NET_DISCONNECT, chan, NULL, 0);
	}

	// free the control block
	memFree(ncp);

	netConn[chan] = NULL;
	return(stat);
}

int16_t ICACHE_FLASH_ATTR
netSetCallback(int16_t chan, NetCallback_t cb)
{
	NetConn_t *ncp;

	if ((chan < 0) || (chan >= MAX_NET_CONN))
		return(ERR_ARG);
	if ((ncp = netConn[chan]) == NULL)
		return(ERR_CLSD);
	ncp->callback = cb;
	return(ERR_OK);
}

int16_t ICACHE_FLASH_ATTR
netListen(int16_t chan, uint32_t port, uint8_t proto)
{
	int16_t stat;
	IPAddress_t ip;
	NetConn_t *ncp;
	struct espconn *conn;
	esp_tcp *pcb;
	enum espconn_type type;

	// check for a valid channel
	if ((chan < 0) || (chan >= MAX_NET_CONN) || (proto > PROTO_MAX))
		return(ERR_ARG);

	// check that the channel is open but not yet connected
	if ((ncp = netConn[chan]) == NULL)
		return(ERR_CLSD);
	if (ncp->conn && (ncp->conn->type || ncp->conn->proto.tcp))
		return(ERR_ISCONN);

	// allocate the protocol control block
	type = ((proto & 0x01) ? ESPCONN_UDP : ESPCONN_TCP);
	if ((pcb = memZalloc((type == ESPCONN_TCP) ? sizeof(esp_tcp) : sizeof(esp_udp))) == NULL)
		return(ERR_MEM);
	if ((conn = memZalloc(sizeof(*conn))) == NULL)
	{
		memFree(pcb);
		return(ERR_MEM);
	}

	// prepare for a connection
	ncp->flags = CONN_ALLOCATED;
	ncp->conn = conn;
	conn->proto.tcp = pcb;
	conn->type = type;
	conn->state = ESPCONN_NONE;
	conn->reverse = ncp;

	// prepare the addresses and ports
	ip.w = wifiGetIP();
	memcpy(pcb->local_ip, &ip.b, 4);
	pcb->local_port = port;	
	ip.w = 0;
	memcpy(pcb->remote_ip, &ip.b, 4);
	pcb->remote_port = 0;

	switch (type)
	{
#if defined(HTTP_SERVER)
	case ESPCONN_TCP:
		// prepare a TCP server connection
		stat = espconn_regist_connectcb(ncp->conn, tcpConnectCB);
		stat = espconn_accept(ncp->conn);
		stat = espconn_regist_time(ncp->conn, DEF_SERVER_TIMEOUT, 0);
		break;
#endif

#if defined(UDP_SERVER)
	case ESPCONN_UDP:
		// prepare a UDP server connection
		stat = espconn_regist_recvcb(ncp->conn, udpReceiveCB);
		stat = espconn_regist_sentcb(ncp->conn, sendDoneCB);
		stat = espconn_create(ncp->conn);
		break;
#endif

	default:
		// this should never happen
		stat = ERR_ERR;
		break;
	}
	return(stat);
}

static NetConn_t * ICACHE_FLASH_ATTR
findListenConn(struct espconn *ecp)
{
	int chan;
	uint16_t port;

	if ((ecp == NULL) || (ecp->proto.tcp == NULL))
		return(NULL);

	port = ecp->proto.tcp->local_port;
	for (chan = 0; chan < MAX_NET_CONN; chan++)
	{
		NetConn_t *ncp;

		// skip if this channel isn't open
		if ((ncp = netConn[chan]) == NULL)
			continue;
		
		// skip if the channel is a client
		if (ncp->flags & CLIENT_MODE)
			continue;

		// skip if the channel isn't listening on the given port
		if ((ncp->conn == NULL) || (ncp->conn->proto.tcp->local_port != port))
			continue;

		// skip if the connection protocol doesn't match
		if (ncp->conn->type != ecp->type)
			continue;

		// found the listener channel
		return(ncp);
	}
	return(NULL);
}

static void ICACHE_FLASH_ATTR
dropConnection(NetConn_t *ncp)
{
	if (ncp != NULL)
	{
		NetBuf_t *nbp;

		// deallocate buffered transmit data
		while ((nbp = ncp->txList) != NULL)
		{
			// link to next, free the node
			ncp->txList = nbp->next;
			memFree(nbp);
		}

		// delete the connection if it was server-initiated, i.e. espconn_accept() or espconn_create()
		if ((ncp->flags & CONN_ALLOCATED) && !(ncp->flags & CLIENT_MODE))
			espconn_delete(ncp->conn);

		if (ncp->flags & CONN_ALLOCATED)
		{
			// deallocate the TCP/UDP protocol block
			if (ncp->conn->proto.tcp != NULL)
				memFree(ncp->conn->proto.tcp);

			// deallocate the connection
			memFree(ncp->conn);
		}

		// kill the conection
		ncp->conn = NULL;
		ncp->flags = 0;
	}
}

#if defined(UDP_SERVER)
static void ICACHE_FLASH_ATTR
udpReceiveCB(void *arg, char *data, uint16_t cnt)
{
	int hand;
	NetConn_t *ncpOld;
	NetConn_t *ncpNew;
	struct espconn *ecp = (struct espconn *)arg;

	if ((ecp != NULL) && ((ncpOld = findListenConn(ecp)) != NULL) &&
			((hand = netOpen()) >= 0) && ((ncpNew = netConn[hand]) != NULL)) 
	{
		// populate the new connection handle
		ecp->reverse = ncpNew;
		ncpNew->conn = ecp;
		ncpNew->callback = ncpOld->callback;
		ncpNew->flags = CONNECTED;

		if (ncpNew->callback != NULL)
			(*ncpNew->callback)(NET_RX_DATA, hand, (uint8_t *)data, cnt);
	}
}
#endif

#if defined(HTTP_SERVER)

static void ICACHE_FLASH_ATTR
receiveCB(void *arg, char *data, uint16_t cnt)
{
	os_printf("MODE=S_Config\n");
	os_printf("%s", data);
	
	NetConn_t *ncp;
	struct espconn *ecp = (struct espconn *)arg;
	
	if ((ecp != NULL) && ((ncp = ecp->reverse) != NULL))
	{
		if (ncp->callback != NULL)
			(*ncp->callback)(NET_RX_DATA, ncp->chan, (uint8_t *)data, cnt);
	}
}

static void ICACHE_FLASH_ATTR
disconnectCB(void *arg)
{
	NetConn_t *ncp;
	struct espconn *ecp = (struct espconn *)arg;

	if ((ecp != NULL) && ((ncp = ecp->reverse) != NULL))
	{
		// the connection was broken by the remote host, close our end
		int chan = ncp->chan;
		ncp->flags &= ~CONNECTED;
		NetCallback_t cb = ncp->callback;
		dropConnection(ncp);
		memFree(ncp);
		netConn[chan] = NULL;
		if (cb != NULL)
			(*cb)(NET_DISCONNECT, chan, NULL, 0);
	}
}

static void ICACHE_FLASH_ATTR
tcpConnectCB(void *arg)
{
	os_printf("tcpConnectCB\n");
	int hand;
	NetConn_t *ncpOld;
	NetConn_t *ncpNew;
	struct espconn *ecp = (struct espconn *)arg;

	if ((ecp == NULL) || ((ncpOld = findListenConn(ecp)) == NULL))
		return;
	if (((hand = netOpen()) < 0) || ((ncpNew = netConn[hand]) == NULL))
		return;

	// populate the new connection handle
	ecp->reverse = ncpNew;
	ncpNew->conn = ecp;
	ncpNew->callback = ncpOld->callback;
	ncpNew->flags |= CONNECTED;

	// set the callbacks
	espconn_regist_disconcb(ncpNew->conn, disconnectCB);
	espconn_regist_sentcb(ncpNew->conn, sendDoneCB);
	espconn_regist_recvcb(ncpNew->conn, receiveCB);
//	espconn_regist_reconcb(ncpNew->conn, reconnectCB);
//	espconn_regist_write_finish(ncpNew->conn, writeFinishCB);

	espconn_set_opt(ncpNew->conn, ESPCONN_NODELAY);
	espconn_set_opt(ncpNew->conn, ESPCONN_COPY);

	if (ncpNew->callback != NULL)
		(*ncpNew->callback)(NET_CONNECT, hand, NULL, 0);
}

static int16_t ICACHE_FLASH_ATTR
isRequest(const uint8_t *data, uint16_t cnt, const char *request)
{
	uint16_t reqLen;

	if ((request == NULL) || ((reqLen = strlen(request)) == 0) || (reqLen > cnt))
		return(0);
	return(memcmp(data, request, reqLen) == 0);
}

static void ICACHE_FLASH_ATTR
sendChunkSize(int16_t handle, uint16_t chunkLen)
{
	uint8_t buf[10];

	// compose the hexadecimal chunk length descriptor, adding the extra CRLF if zero
	os_sprintf(buf, "%x\r\n%s", chunkLen, chunkLen ? "" : "\r\n");
	netSendStr(handle, buf);
}

#endif

static void ICACHE_FLASH_ATTR
sendDoneCB(void *arg)
{
	NetConn_t *ncp;
	struct espconn *ecp = (struct espconn *)arg;

	if ((ecp != NULL) && ((ncp = ecp->reverse) != NULL))
	{
		NetBuf_t *nbp;
		if (((nbp = ncp->txList) != NULL) && !(ncp->flags & CLIENT_MODE))
		{
			// send the next block on the list
			espconn_sent(ncp->conn, nbp->data, nbp->len);

			// free the node
			ncp->txList = nbp->next;
			memFree(nbp);
		}
		else
			ncp->flags &= ~AWAITING_TRANSMISSION;

		if (ncp->callback != NULL)
			(*ncp->callback)(NET_TX_DATA, ncp->chan, NULL, 0);
	}
}

// append a data buffer to the end of a list
static void ICACHE_FLASH_ATTR
appendBuffer(NetBuf_t **nbpp, NetBuf_t *nbp)
{
	if ((nbpp != NULL) && (nbp != NULL))
	{
		// move to the final node
		while (*nbpp != NULL)
			nbpp = &(*nbpp)->next;

		// link the the new node
		*nbpp = nbp;
	}
}

int16_t ICACHE_FLASH_ATTR
netSend(NetConn_t *ncp, const uint8_t *data, uint16_t cnt)
{
	int stat = ERR_OK;

	if ((ncp == NULL) || (ncp->conn == NULL) || (data == NULL))
		return(ERR_ARG);
	if (cnt == 0)
		return(ERR_OK);

	if ((ncp->txList != NULL) || ((ncp->flags & AWAITING_TRANSMISSION) && !(ncp->flags & CLIENT_MODE)))
	{
		NetBuf_t *nbp;

		// create a data buffer
		if ((nbp = memZalloc(sizeof(*nbp) + cnt - 1)) == NULL)
			return(ERR_MEM);
		
		// populate the buffer
		nbp->len = cnt;
		memcpy(nbp->data, data, cnt);

		// add it to the list of buffers
		appendBuffer(&ncp->txList, nbp);
		return(ERR_OK);
	}

	// OK to send the data immediately
	ncp->flags |= AWAITING_TRANSMISSION;
	return(espconn_sent(ncp->conn, (uint8_t *)data, cnt));
}

int16_t ICACHE_FLASH_ATTR
netSendStr(int16_t chan, const uint8_t *str)
{
	if (str != NULL)
		return(netSendData(chan, str, strlen((char *)str)));
	return(ERR_OK);
}

int16_t ICACHE_FLASH_ATTR
netSendData(int16_t chan, const uint8_t *data, uint16_t cnt)
{
	NetConn_t *ncp;

	if ((chan < 0) || (chan >= MAX_NET_CONN) || (data == NULL))
		return(ERR_ARG);
	if ((ncp = netConn[chan]) == NULL)
		return(ERR_CLSD);
	if ((ncp->conn->type == ESPCONN_TCP) && !(ncp->flags & CONNECTED))
		return(ERR_CONN);
	return(netSend(ncp, data, cnt));
}

// Print Ip Address
void print_ip(char *desc, uint32 addr)
{
    os_printf("%s", desc);
    if(addr>0)
    {
		os_printf("%d.%d.%d.%d", IP2STR(&addr));
    }
    else
		os_printf("Not set");
    os_printf("\n\r");
}

uint32_t ICACHE_FLASH_ATTR
wifiGetIP(void)
{
	struct ip_info ipInfo;
	ipInfo.ip.addr=0;
    ipInfo.netmask.addr=255;
    ipInfo.gw.addr=0;
	wifi_get_ip_info(0, &ipInfo);
	
	print_ip("IP Address: ", ipInfo.ip.addr);
    print_ip("Netmask   : ", ipInfo.netmask.addr);
    print_ip("Gateway   : ", ipInfo.gw.addr);

	return(ipInfo.ip.addr);
}

void ICACHE_FLASH_ATTR
dbgInit(uint8_t chan, uint32_t baud)
{
	// set the debug channel
	if (chan > 1)
		chan = 1;
	dbgChan = chan;

	if (baud != 0)
	{
		uint32_t flags;
		uint32_t mask;
		if (dbgChan == 1)
			PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
		else
		{
			PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
			PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
		}
		uart_div_modify(dbgChan, UART_CLK_FREQ / baud);

		mask = (UART_STOP_BIT_NUM << UART_STOP_BIT_NUM_S) |
				(UART_BIT_NUM << UART_BIT_NUM_S) | UART_PARITY_EN | UART_PARITY;
		flags = READ_PERI_REG(UART_CONF0(dbgChan)) & ~mask;

		// default to N81 operation
		flags |= (NONE_BITS & (UART_PARITY_EN | UART_PARITY));
		flags |= (EIGHT_BITS & UART_BIT_NUM) << UART_BIT_NUM_S;
		flags |= (ONE_STOP_BIT & UART_STOP_BIT_NUM) << UART_STOP_BIT_NUM_S;
		WRITE_PERI_REG(UART_CONF0(dbgChan), flags);

		// clear rx and tx fifo, not ready
		SET_PERI_REG_MASK(UART_CONF0(dbgChan), UART_RXFIFO_RST | UART_TXFIFO_RST);
		CLEAR_PERI_REG_MASK(UART_CONF0(dbgChan), UART_RXFIFO_RST | UART_TXFIFO_RST);

		//clear all pending interrupts and disable
		WRITE_PERI_REG(UART_INT_CLR(dbgChan), 0xffff);
		CLEAR_PERI_REG_MASK(UART_INT_ENA(dbgChan), 0xff);
	}
}

void ICACHE_FLASH_ATTR
dbgChar(uint8_t c)
{
	while (1)
	{
		uint32_t fifoCnt = READ_PERI_REG(UART_STATUS(dbgChan)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);
		if (((fifoCnt >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT) < 126)
			break;
	}
	WRITE_PERI_REG(UART_FIFO(dbgChan), c);
}

void ICACHE_FLASH_ATTR
dbgCRLF(void)
{
	dbgChar(0x0d);
	dbgChar(0x0a);
}

void ICACHE_FLASH_ATTR
dbgByteDec(uint8_t val)
{
	dbgWordDec(val);
}

void ICACHE_FLASH_ATTR
dbgWordDec(uint16_t val)
{
	if (val >= 10)
		dbgWordDec(val / 10);
	dbgChar((uint8_t)(val % 10) + '0');
}

void ICACHE_FLASH_ATTR
dbgInt(int16_t val)
{
	if (val < 0)
	{
		dbgChar('-');
		val = -val;
	}
	dbgWordDec(val);
}

void ICACHE_FLASH_ATTR
dbgStr(const char *str)
{
	if (str != NULL)
	{
		char c;
		while ((c = *str++) != '\0')
			dbgChar(c);
	}
}

void ICACHE_FLASH_ATTR
dbgStrN(const char *str, uint16_t cnt)
{
	if (str != NULL)
	{
		while (cnt--)
			dbgChar(*str++);
	}
}

void * ICACHE_FLASH_ATTR
memAlloc(size_t size)
{
	return((size == 0) ? NULL : os_malloc(ADJUST_ALLOC_BLOCK(size)));
}

void * ICACHE_FLASH_ATTR
memZalloc(size_t size)
{
	return((size == 0) ? NULL : os_zalloc(ADJUST_ALLOC_BLOCK(size)));
}

void ICACHE_FLASH_ATTR
memFree(void *p)
{
	if (p != NULL)
	   	os_free(p);
}

void ICACHE_FLASH_ATTR
dspData(const uint8_t *data, uint16_t cnt)
{
	os_printf("++++++++++++++++++\r\n");
	while (cnt--)
	{
		uint8_t c;
		c = *data++;
		if (c == '\n')
			dbgCRLF();
		else if (c != '\r')
			os_printf("%c", c);
	}
	os_printf("------------------\r\n");
}

void ICACHE_FLASH_ATTR
dspIP(const char *caption, uint32_t ip)
{
	int16_t i;
	IPAddress_t addr;

	os_printf("%s", caption);
	addr.w = ip;
	for (i = 0; i < 4; i++)
	{
		dbgByteDec(addr.b[i]);
		if (i < 3)
			dbgChar('.');
	}
}
