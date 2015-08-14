/* BUILDING AND FLASHING GUIDE
* #clean
* mingw32-make clean
* #make
* mingw32-make SDK_BASE="c:/Espressif/ESP8266_SDK_112" FLAVOR="release" all
* #flash
* mingw32-make ESPPORT="COM1" flash
*/


/* USAGE GUIDE
This firmware put ESP8266 to 3 mode:
[1] Smart config
    MODE command: MODE=C
	Success indicated by "MODE=C_OK\n", followed by SSID name and password
		[{"wifiSSID":"(Belkin_G_Plus_MIMO)","wifiPASS":"()"}]

[2] Server for TCP
    MODE command: MODE=S
	Success indicated by "MODE=S_OK\n"
	Application will have to wait until it get "MODE=S Config", followed by APN and proxy name, port, auth
		[{"gprsAPN":"(Telkomsel)","proxyServer":"(192.168.128.5)","proxyPort":"(3128)","proxyAuth":"(Og==)"}]
	
[3] Bridge SLIP for MQTT and REST]
    MODE command: MODE=B
	Success indicated by "MODE=B_OK\n"

*/

#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "cmd.h"
#include "mode.h" // mode selector - set esp to mode SMARTCONFIG, WEBSERVER, OR SLIP MQTT REST
#include "server.h" // tcp webserver
#include "sconfig.h" // smartconfig
#include "driver/uart.h"

void user_rf_pre_init(void)
{
}

void ICACHE_FLASH_ATTR
bridge_init(void)
{
	MODE_Init();
}

void ICACHE_FLASH_ATTR
user_init(void)
{
	uart_init(BIT_RATE_19200, BIT_RATE_19200);
	os_printf("SDK version:%s\n", system_get_sdk_version());
	wifi_set_opmode(STATION_MODE);
	system_init_done_cb(bridge_init);
}