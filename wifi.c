#include <sys/param.h>
#include <osapi.h>
#include <string.h>
#include <user_interface.h>

#include "config.h"
#include "log.h"
#include "missing-decls.h"

static void wifi_evt_cb(System_Event_t *evt);

ICACHE_FLASH_ATTR void wifi_init() {
    struct softap_config apconf;
    char ssid[sizeof(apconf.ssid)];
    struct station_config staconf;

    if (wifi_get_opmode() != STATIONAP_MODE) {
        if (!wifi_set_opmode(STATIONAP_MODE))
            CRITICAL(MAIN, "wifi_set_opmode() failed\n")
    }

    if (!wifi_softap_get_config(&apconf))
        CRITICAL(MAIN, "wifi_softap_get_config() failed\n")

    /* assert sizeof(SSID_PREFIX) + 10 < MIN(size(ssid), size(apconf.ssid)) */
    os_snprintf(ssid, sizeof(ssid),
                "%s-%08x", SSID_PREFIX, system_get_chip_id());
    if (os_strncmp(ssid, (char *)apconf.ssid, os_strlen(ssid)) != 0) {
        os_strncpy((char *)apconf.ssid, ssid, os_strlen(ssid)+1);
        os_strncpy((char *)apconf.password, SSID_PASS, os_strlen(SSID_PASS)+1);
        apconf.ssid_len = 0;

        /* The underlying hardware only supports one channel,
         * which will be purposefully overriden when connecting as a station.
         * Ref: ESP8266 Non-OS SDK API Reference (ver 2.1.2),
         *      A.4 ESP8266 SoftAP and Station Channel Configuration
         */
        apconf.channel = 1;

        apconf.authmode = AUTH_WPA_WPA2_PSK;
        apconf.ssid_hidden = 0;
        apconf.max_connection = MAX_CONN;
        apconf.beacon_interval = 100;

        if (!wifi_softap_set_config(&apconf))
            CRITICAL(MAIN, "wifi_softap_set_config() failed\n")
    }

    wifi_set_event_handler_cb(wifi_evt_cb);

    if (!wifi_station_get_config(&staconf)) {
        ERROR(MAIN, "wifi_station_get_config() failed\n")
        return;
    }

    INFO(MAIN, "station: ssid=%s\n", staconf.ssid)
    /* INFO(MAIN, "station: pass=%s\n", staconf.password) */
    if (staconf.bssid_set)
        INFO(MAIN, "station: bssid=" MACSTR "\n",
                   staconf.password, MAC2STR(staconf.bssid))
}

ICACHE_FLASH_ATTR static void wifi_evt_cb(System_Event_t *evt) {
    switch (evt->event) {
        case EVENT_STAMODE_CONNECTED: {
            Event_StaMode_Connected_t *info = &evt->event_info.connected;

            char ssid[sizeof(info->ssid) + 1];
            size_t len = MIN(strnlen((char *)info->ssid, sizeof(info->ssid)),
                             info->ssid_len);

            /* %.*s is not supported unfortunately */
            os_strncpy(ssid, (char *)info->ssid, len);
            ssid[len] = 0;

            DEBUG(MAIN, "station: connected ssid=%s channel=%d\n",
                        ssid, info->channel)
            break;
        }

        case EVENT_STAMODE_DISCONNECTED: {
            Event_StaMode_Disconnected_t *info = &evt->event_info.disconnected;

            char ssid[sizeof(info->ssid) + 1];
            size_t len = MIN(strnlen((char *)info->ssid, sizeof(info->ssid)),
                             info->ssid_len);

            /* %.*s is not supported unfortunately */
            os_strncpy(ssid, (char *)info->ssid, len);
            ssid[len] = 0;

            DEBUG(MAIN, "station: disconnected ssid=%s reason=0x%02x\n",
                        ssid, info->reason)
            break;
        }

        case EVENT_STAMODE_AUTHMODE_CHANGE: {
            Event_StaMode_AuthMode_Change_t *info = &evt->event_info.auth_change;
            DEBUG(MAIN, "station: authmode_change old=0x%02x new=0x%02x\n",
                  info->old_mode, info->new_mode);
            break;
        }

        case EVENT_STAMODE_GOT_IP: {
            Event_StaMode_Got_IP_t *info = &evt->event_info.got_ip;
            DEBUG(MAIN, "station: got_ip ip=" IPSTR
                        " mask=" IPSTR " gw=" IPSTR "\n",
                  IP2STR(&info->ip), IP2STR(&info->mask), IP2STR(&info->gw))
            break;
        }
        
        case EVENT_SOFTAPMODE_STACONNECTED: {
            Event_SoftAPMode_StaConnected_t *info =
                &evt->event_info.sta_connected;
            DEBUG(MAIN, "softap: staconnected mac=" MACSTR " aid=%02x\n",
                  MAC2STR(info->mac), info->aid)
            break;
        }

        case EVENT_SOFTAPMODE_STADISCONNECTED: {
            Event_SoftAPMode_StaDisconnected_t *info =
                &evt->event_info.sta_disconnected;
            DEBUG(MAIN, "softap: stadisconnected mac=" MACSTR " aid=%02x\n",
                  MAC2STR(info->mac), info->aid)
            break;
        }

        case EVENT_SOFTAPMODE_PROBEREQRECVED: {
            #if 0
                Event_SoftAPMode_ProbeReqRecved_t *info =
                    &evt->event_info.ap_probereqrecved;
                DEBUG(MAIN, "softap: probereqrecved mac=" MACSTR " rssi=%d\n",
                      MAC2STR(info->mac), info->rssi)
            #endif
            break;
        }

        default:
            DEBUG(MAIN, "unknown event (0x%04x)\n", evt->event)
            break;
    }
}
