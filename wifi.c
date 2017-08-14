#include "log/log.h"
#include "smtp/smtp.h"
#include "missing-decls.h"

#include <ets_sys.h>
#include <osapi.h>
#include <user_interface.h>

static void wifi_evt_cb(System_Event_t *evt);

#define COPY_SSID(dest, src, srclen) { \
    /* dest expected to be sizeof((struct softap_config *)->ssid) + 1 */ \
    size_t maxlen = 32; /* sizeof((struct softap_config).ssid); */ \
    size_t len    = strnlen((char *)(src), maxlen); \
    \
    if (0 < srclen && srclen < len) \
        len = srclen; \
    \
    /* %.*s is not supported unfortunately */ \
    os_strncpy(dest, (char *)(src), len); \
    (dest)[len] = 0; \
}

ICACHE_FLASH_ATTR void wifi_init() {
    struct softap_config apconf;
    char ssid[sizeof(apconf.ssid) + 1];
    struct station_config staconf;

    if (wifi_get_opmode() != STATIONAP_MODE) {
        if (!wifi_set_opmode(STATIONAP_MODE))
            CRITICAL(MAIN, "wifi_set_opmode() failed")
    }

    if (!wifi_softap_get_config(&apconf))
        CRITICAL(MAIN, "wifi_softap_get_config() failed")

    os_snprintf(ssid, sizeof(ssid)-1, "%s-%08x",
                WIFI_SSID_PREFIX, system_get_chip_id());
    if (os_strncmp(ssid, (char *)apconf.ssid, os_strlen(ssid)) != 0) {
        os_strncpy((char *)apconf.ssid, ssid, os_strlen(ssid)+1);
        apconf.ssid_len = 0;

        os_strncpy((char *)apconf.password, WIFI_PASSWORD,
                   os_strlen(WIFI_PASSWORD)+1);

        /* The underlying hardware only supports one channel,
         * which will be purposefully overriden when connecting as a station.
         * Ref: ESP8266 Non-OS SDK API Reference (ver 2.1.2),
         *      A.4 ESP8266 SoftAP and Station Channel Configuration
         */
        apconf.channel = 1;

        apconf.authmode = AUTH_WPA_WPA2_PSK;
        apconf.ssid_hidden = 0;
        apconf.max_connection = 4;
        apconf.beacon_interval = 100;

        if (!wifi_softap_set_config(&apconf))
            CRITICAL(MAIN, "wifi_softap_set_config() failed")
    }

    wifi_set_event_handler_cb(wifi_evt_cb);

    if (!wifi_station_get_config(&staconf)) {
        ERROR(MAIN, "wifi_station_get_config() failed")
        return;
    }

    COPY_SSID(ssid, staconf.ssid, 0)
    DEBUG(MAIN, "station: ssid=%s", ssid)
    DEBUG(MAIN, "station: pass=%s", staconf.password);
    if (staconf.bssid_set)
        DEBUG(MAIN, "station: bssid=" MACSTR, MAC2STR(staconf.bssid))
}

ICACHE_FLASH_ATTR static void wifi_evt_cb(System_Event_t *evt) {
    switch (evt->event) {
        case EVENT_STAMODE_CONNECTED: {
            Event_StaMode_Connected_t *info = &evt->event_info.connected;
            char ssid[sizeof(info->ssid) + 1];

            COPY_SSID(ssid, info->ssid, info->ssid_len)
            INFO(MAIN, "station: connected ssid=%s channel=%d", ssid, info->channel)

            break;
        }

        case EVENT_STAMODE_DISCONNECTED: {
            Event_StaMode_Disconnected_t *info = &evt->event_info.disconnected;
            char ssid[sizeof(info->ssid) + 1];

            COPY_SSID(ssid, info->ssid, info->ssid_len)
            INFO(MAIN, "station: disconnected ssid=%s reason=0x%02x", ssid, info->reason)

            break;
        }

        case EVENT_STAMODE_AUTHMODE_CHANGE: {
            Event_StaMode_AuthMode_Change_t *info = &evt->event_info.auth_change;

            INFO(MAIN, "station: authmode_change old=0x%02x new=0x%02x", info->old_mode, info->new_mode)

            break;
        }

        case EVENT_STAMODE_GOT_IP: {
            Event_StaMode_Got_IP_t *info = &evt->event_info.got_ip;
            char subj[64];

            INFO(MAIN, "station: got_ip ip=" IPSTR " mask=" IPSTR " gw=" IPSTR,
                       IP2STR(&info->ip), IP2STR(&info->mask), IP2STR(&info->gw))

            os_snprintf(subj, sizeof(subj), "esp8266-%08x online", system_get_chip_id());
            smtp_send(smtp_server.from, smtp_server.to, subj, "");

            break;
        }
        
        case EVENT_SOFTAPMODE_STACONNECTED: {
            Event_SoftAPMode_StaConnected_t *info = &evt->event_info.sta_connected;

            INFO(MAIN, "softap: staconnected mac=" MACSTR " aid=%02x", MAC2STR(info->mac), info->aid);

            break;
        }

        case EVENT_SOFTAPMODE_STADISCONNECTED: {
            Event_SoftAPMode_StaDisconnected_t *info = &evt->event_info.sta_disconnected;

            INFO(MAIN, "softap: stadisconnected mac=" MACSTR " aid=%02x", MAC2STR(info->mac), info->aid);

            break;
        }

        case EVENT_SOFTAPMODE_PROBEREQRECVED: {
            #if 0
                Event_SoftAPMode_ProbeReqRecved_t *info = &evt->event_info.ap_probereqrecved;

                INFO(MAIN, "softap: probereqrecved mac=" MACSTR " rssi=%d", MAC2STR(info->mac), info->rssi);
            #endif
            break;
        }

        default:
            INFO(MAIN, "unknown event (0x%04x)", evt->event);
            break;
    }
}

