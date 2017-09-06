#include "config.h"
#include "log.h"

#include <osapi.h>
#include <user_interface.h>

void wifi_event_cb(System_Event_t *evt);

ICACHE_FLASH_ATTR void wifi_init() {
    struct station_config conf;

    if (wifi_get_opmode() != STATION_MODE) {
        if (!wifi_set_opmode(STATION_MODE))
            log_critical("main", "wifi_set_opmode() failed");
    }

    if (!wifi_station_get_config(&conf))
        log_critical("main", "wifi_station_get_config() failed");

    if (os_strcmp(WIFI_SSID, (char *)conf.ssid) != 0) {
        os_strncpy((char *)conf.ssid, WIFI_SSID, os_strlen(WIFI_SSID)+1);
        os_strncpy((char *)conf.password, WIFI_PASS, os_strlen(WIFI_PASS)+1);

        if (!wifi_station_set_config(&conf))
            log_critical("main", "wifi_station_set_config() failed");
    }

    wifi_set_event_handler_cb(wifi_event_cb);
}

ICACHE_FLASH_ATTR void wifi_event_cb(System_Event_t *evt) {
    switch (evt->event) {
        case EVENT_STAMODE_CONNECTED: {
            Event_StaMode_Connected_t *info = &evt->event_info.connected;
            log_info("main", "station: connected ssid=%s channel=%d",
                             info->ssid, info->channel);
            break;
        }

        case EVENT_STAMODE_DISCONNECTED: {
            Event_StaMode_Disconnected_t *info = &evt->event_info.disconnected;
            log_info("main", "station: disconnected ssid=%s reason=0x%02x",
                             info->ssid, info->reason);
            break;
        }

        case EVENT_STAMODE_AUTHMODE_CHANGE: {
            Event_StaMode_AuthMode_Change_t *info = &evt->event_info.auth_change;
            log_info("main", "station: authmode_change old=0x%02x new=0x%02x",
                             info->old_mode, info->new_mode);
            break;
        }

        case EVENT_STAMODE_GOT_IP: {
            Event_StaMode_Got_IP_t *info = &evt->event_info.got_ip;
            log_info("main", "station: got_ip ip=" IPSTR " mask=" IPSTR " gw=" IPSTR,
                             IP2STR(&info->ip), IP2STR(&info->mask), IP2STR(&info->gw));
            break;
        }
        
        case EVENT_SOFTAPMODE_STACONNECTED: {
            Event_SoftAPMode_StaConnected_t *info = &evt->event_info.sta_connected;
            log_info("main", "softap: staconnected mac=" MACSTR " aid=%02x",
                             MAC2STR(info->mac), info->aid);

            break;
        }

        case EVENT_SOFTAPMODE_STADISCONNECTED: {
            Event_SoftAPMode_StaDisconnected_t *info = &evt->event_info.sta_disconnected;
            log_info("main", "softap: stadisconnected mac=" MACSTR " aid=%02x",
                             MAC2STR(info->mac), info->aid);
            break;
        }

        case EVENT_SOFTAPMODE_PROBEREQRECVED: {
            #if 0
                Event_SoftAPMode_ProbeReqRecved_t *info = &evt->event_info.ap_probereqrecved;
                log_info("main", "softap: probereqrecved mac=" MACSTR " rssi=%d",
                                 MAC2STR(info->mac), info->rssi);
            #endif
            break;
        }

        default:
            log_info("main", "unknown event (0x%04x)", evt->event);
            break;
    }
}
