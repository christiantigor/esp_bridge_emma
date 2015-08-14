#include "sconfig.h"

sc_type SC_Type = 0;
struct station_config *sta_conf;

void ICACHE_FLASH_ATTR
smartconfig_done(sc_status status, void *pdata)
{
    switch(status) {
        case SC_STATUS_WAIT:
            os_printf("SC_STATUS_WAIT\n");
            break;
        case SC_STATUS_FIND_CHANNEL:
            os_printf("SC_STATUS_FIND_CHANNEL\n");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            os_printf("SC_STATUS_GETTING_SSID_PSWD\n");
            break;
        case SC_STATUS_LINK:
            os_printf("SC_STATUS_LINK\n");
			sta_conf = pdata;
	        wifi_station_set_config(sta_conf);
	        
			wifi_station_disconnect();
	        wifi_station_connect();
            
			break;
        case SC_STATUS_LINK_OVER:
            os_printf("SC_STATUS_LINK_OVER\n");
            if (SC_Type == SC_TYPE_ESPTOUCH) {
                uint8 phone_ip[4] = {0};

                os_memcpy(phone_ip, (uint8*)pdata, 4);
                os_printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
            }
            smartconfig_stop();
			os_printf("MODE=C OK\n");
			os_printf("[{\"wifiSSID\":\"(%s)\",\"wifiPASS\":\"(%s)\"}]\n", sta_conf->ssid, sta_conf->password);	
			wifi_station_set_auto_connect(TRUE);
            break;
    }	
}

void SCONFIG_Init(void) {
	SC_Type = SC_TYPE_ESPTOUCH;
    smartconfig_start(SC_Type, smartconfig_done);	
}