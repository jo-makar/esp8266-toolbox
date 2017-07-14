#include <osapi.h>
#include <sys/param.h>
#include <user_interface.h>

#include "missing-decls.h"
#include "string.h"

static void wifi_evt_cb(System_Event_t *evt);

void wifi_init() {
    #define SSID_PREFIX "esp"
    #define SSID_PASS "l0cK*It_Dwn"

    #define FAIL(s, ...) { \
        os_printf(s, ##__VA_ARGS__); \
        while (1) os_delay_us(1000); \
    }

    struct softap_config conf;
    char ssid[sizeof(conf.ssid)];
    bool update;

    if (wifi_get_opmode() != STATIONAP_MODE) {
        if (!wifi_set_opmode(STATIONAP_MODE))
            FAIL("wifi_set_opmode() failed\n")
    }

    update = false;
    if (!wifi_softap_get_config(&conf))
        FAIL("wifi_softap_get_config() failed\n")
    else {
        /* assert sizeof(SSID_PREFIX) + 10 < MIN(size(ssid), size(conf.ssid)) */
        os_snprintf(ssid, sizeof(ssid),
                    "%s-%08x", SSID_PREFIX, system_get_chip_id());
        if (os_strncmp(ssid, (char *)conf.ssid, os_strlen(ssid)) != 0)
            update = true;
    }

    if (update) {
        os_strncpy((char *)conf.ssid, ssid, os_strlen(ssid)+1);
        os_strncpy((char *)conf.password, SSID_PASS, os_strlen(SSID_PASS)+1);
        conf.ssid_len = 0;

        /* The underlying hardware only supports one channel,
         * which will be purposefully overriden when connecting as a station.
         * Ref: ESP8266 Non-OS SDK API Reference (ver 2.1.2),
         *      A.4 ESP8266 SoftAP and Station Channel Configuration
         */
        conf.channel = 1;

        conf.authmode = AUTH_WPA_WPA2_PSK;
        conf.ssid_hidden = 0;
        conf.max_connection = 4;
        conf.beacon_interval = 100;

        if (!wifi_softap_set_config(&conf))
            FAIL("wifi_softap_set_config() failed\n")
    }

    wifi_set_event_handler_cb(wifi_evt_cb);
}

static void wifi_evt_cb(System_Event_t *evt) {
    size_t len;

    switch (evt->event) {
        case EVENT_STAMODE_CONNECTED: {
            Event_StaMode_Connected_t *info = &evt->event_info.connected;
            len = MIN(strnlen((char *)info->ssid, sizeof(info->ssid)),
                      info->ssid_len);
            os_printf("wifi: station: connected ssid=%.*s channel=%d\n",
                      len, info->ssid, info->channel);
            /* TODO uint8 bssid[6]? */
            break;
        }

        case EVENT_STAMODE_DISCONNECTED: {
            Event_StaMode_Disconnected_t *info = &evt->event_info.disconnected;
            len = MIN(strnlen((char *)info->ssid, sizeof(info->ssid)),
                      info->ssid_len);
            os_printf("wifi: station: disconnected ssid=%.*s reason=0x%02x\n",
                      len, info->ssid, info->reason);
            /* TODO uint8 bssid[6]? */
            break;
        }

        case EVENT_STAMODE_AUTHMODE_CHANGE: {
            Event_StaMode_AuthMode_Change_t *info = &evt->event_info.auth_change;
            os_printf("wifi: station: authmode_change old=0x%02x new=0x%02x\n",
                      info->old_mode, info->new_mode);
            break;
        }

        case EVENT_STAMODE_GOT_IP: {
            Event_StaMode_Got_IP_t *info = &evt->event_info.got_ip;
            os_printf("wifi: station: got_ip "
                      "ip=" IPSTR " mask=" IPSTR " gw=" IPSTR "\n",
                      IP2STR(&info->ip), IP2STR(&info->mask), IP2STR(&info->gw));
            break;
        }
        
        case EVENT_SOFTAPMODE_STACONNECTED: {
            Event_SoftAPMode_StaConnected_t *info =
                &evt->event_info.sta_connected;
            os_printf("wifi: softap: staconnected mac=" MACSTR " aid=%02x\n",
                      MAC2STR(info->mac), info->aid);
            break;
        }

        case EVENT_SOFTAPMODE_STADISCONNECTED: {
            Event_SoftAPMode_StaDisconnected_t *info =
                &evt->event_info.sta_disconnected;
            os_printf("wifi: softap: stadisconnected mac=" MACSTR " aid=%02x\n",
                      MAC2STR(info->mac), info->aid);
            break;
        }

        case EVENT_SOFTAPMODE_PROBEREQRECVED: {
            if (0) {
                Event_SoftAPMode_ProbeReqRecved_t *info =
                    &evt->event_info.ap_probereqrecved;
                os_printf("wifi: softap: probereqrecved "
                          "mac=" MACSTR " rssi=%d\n",
                          MAC2STR(info->mac), info->rssi);
            }
            break;
        }

        default:
            os_printf("wifi: unknown event (0x%04x)\n", evt->event);
            break;
    }
}
