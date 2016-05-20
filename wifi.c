#include "esp-missing-decls.h"
#include "mqtt/mqtt_init.h"
#include "string.h"

#include <osapi.h>
#include <user_interface.h>

void wifi_event_handler(System_Event_t *evt);

ICACHE_FLASH_ATTR void wifi_init() {
    uint8_t opmode;

    opmode = wifi_get_opmode();

    if (opmode == SOFTAP_MODE || opmode == STATIONAP_MODE) {
        struct softap_config config;

        /* Currently station+soft-AP mode is not used */
        if (!wifi_set_opmode(SOFTAP_MODE)) {
            os_printf("wifi_init: wifi_set_opmode failed\n");
            return;
        }

        if (!wifi_softap_get_config(&config)) {
            os_printf("wifi_init: wifi_softap_get_config failed\n");
            return;
        }

        {
            char ssid[sizeof(config.ssid) + 1];
            size_t len;

            len = strnlen((char *)config.ssid, sizeof(config.ssid));
            if (config.ssid_len != 0 && len > config.ssid_len)
                len = config.ssid_len;

            os_printf("  wifi_softap_get_config.ssid_len, len = %u, %u\n", config.ssid_len, len);

            os_strncpy(ssid, (char *)config.ssid, len);
            ssid[len] = 0;
            os_printf("           wifi_softap_get_config.ssid = %s\n", ssid);
        }

        os_printf("       wifi_softap_get_config.authmode = %u ", config.authmode);
        switch (config.authmode) {
            case 0:  os_printf("open\n");         break;
            case 1:  os_printf("WEP\n");          break;
            case 2:  os_printf("WPA_PSK\n");      break;
            case 3:  os_printf("WPA2_PSK\n");     break;
            case 4:  os_printf("WPA_WPA2_PSK\n"); break;
            default: os_printf("unknown\n");      break;
        }

        if (config.authmode != 0) {
            uint8_t len = strnlen((const char *)config.password, sizeof(config.password));
            os_printf("  wifi_softap_get_config.password, len = %s, %u\n", config.password, len);
        }

        os_printf("        wifi_softap_get_config.channel = %u\n", config.channel);
        os_printf("    wifi_softap_get_config.ssid_hidden = %u\n",    config.ssid_hidden);

        if (!config.ssid_hidden)
            os_printf("wifi_softap_get_config.beacon_interval = %u ms\n", config.beacon_interval);

        os_printf(" wifi_softap_get_config.max_connection = %u\n",    config.max_connection);

        os_printf("\n");
    }

    else if (opmode == STATION_MODE) {
        struct station_config config;

        if (!wifi_station_get_config(&config)) {
            os_printf("wifi_init: wifi_station_get_config failed\n");
            return;
        }

        {
            size_t i;

            os_printf("     wifi_station_get_config.ssid = %s\n", config.ssid);
            os_printf(" wifi_station_get_config.password = %s\n", config.password);
            os_printf("wifi_station_get_config.bssid_set = %u\n", config.bssid_set);

            if (config.bssid_set) {
                os_printf("    wifi_station_get_config.bssid = ");
                for (i=0; i<sizeof(config.bssid); i++)
                    os_printf("%02x", config.bssid[i]);
                os_printf("\n");
            }
        }
    }

    wifi_set_event_handler_cb(wifi_event_handler);
}


ICACHE_FLASH_ATTR void wifi_event_handler(System_Event_t *evt) {
    #define SSID_LEN_COPY(evtinfo) \
        len = strnlen((char *)evtinfo.ssid, sizeof(evtinfo.ssid)); \
        if (evtinfo.ssid_len != 0 && len > evtinfo.ssid_len) \
            len = evtinfo.ssid_len; \
        \
        os_strncpy(ssid, (char *)evtinfo.ssid, len); \
        ssid[len] = 0;

    switch (evt->event) {
        case EVENT_STAMODE_CONNECTED:
            {
                char ssid[sizeof(evt->event_info.connected.ssid) + 1];
                size_t len;

                SSID_LEN_COPY(evt->event_info.connected)

                os_printf("wifi_event_handler: connected ssid=%s bssid=" MACSTR " chan=%u\n",
                          ssid, MAC2STR(evt->event_info.connected.bssid), evt->event_info.connected.channel);
            }
            break;

        case EVENT_STAMODE_DISCONNECTED:
            {
                char ssid[sizeof(evt->event_info.disconnected.ssid) + 1];
                size_t len;

                SSID_LEN_COPY(evt->event_info.disconnected)

                os_printf("wifi_event_handler: disconnected ssid=%s bssid=" MACSTR " reason=%u/",
                          ssid, MAC2STR(evt->event_info.disconnected.bssid), evt->event_info.disconnected.reason);

                switch (evt->event_info.disconnected.reason) {
                    case REASON_UNSPECIFIED:              os_printf("UNSPECIFIED\n");              break;
                    case REASON_AUTH_EXPIRE:              os_printf("AUTH_EXPIRE\n");              break;
                    case REASON_AUTH_LEAVE:               os_printf("AUTH_LEAVE\n");               break;
                    case REASON_ASSOC_EXPIRE:             os_printf("ASSOC_EXPIRE\n");             break;
                    case REASON_ASSOC_TOOMANY:            os_printf("ASSOC_TOOMANY\n");            break;
                    case REASON_NOT_AUTHED:               os_printf("NOT_AUTHED\n");               break;
                    case REASON_NOT_ASSOCED:              os_printf("NOT_ASSOCED\n");              break;
                    case REASON_ASSOC_LEAVE:              os_printf("ASSOC_LEAVE\n");              break;
                    case REASON_ASSOC_NOT_AUTHED:         os_printf("ASSOC_NOT_AUTHED\n");         break;
                    case REASON_DISASSOC_PWRCAP_BAD:      os_printf("DISASSOC_PWRCAP_BAD\n");      break;
                    case REASON_DISASSOC_SUPCHAN_BAD:     os_printf("DISASSOC_SUPCHAN_BAD\n");     break;
                    case REASON_IE_INVALID:               os_printf("IE_INVALID\n");               break;
                    case REASON_MIC_FAILURE:              os_printf("MIC_FAILURE\n");              break;
                    case REASON_4WAY_HANDSHAKE_TIMEOUT:   os_printf("4WAY_HANDSHAKE_TIMEOUT\n");   break;
                    case REASON_GROUP_KEY_UPDATE_TIMEOUT: os_printf("GROUP_KEY_UPDATE_TIMEOUT\n"); break;
                    case REASON_IE_IN_4WAY_DIFFERS:       os_printf("IE_IN_4WAY_DIFFERS\n");       break;
                    case REASON_GROUP_CIPHER_INVALID:     os_printf("GROUP_CIPHER_INVALID\n");     break;
                    case REASON_PAIRWISE_CIPHER_INVALID:  os_printf("PAIRWISE_CIPHER_INVALID\n");  break;
                    case REASON_AKMP_INVALID:             os_printf("AKMP_INVALID\n");             break;
                    case REASON_UNSUPP_RSN_IE_VERSION:    os_printf("UNSUPP_RSN_IE_VERSION\n");    break;
                    case REASON_INVALID_RSN_IE_CAP:       os_printf("INVALID_RSN_IE_CAP\n");       break;
                    case REASON_802_1X_AUTH_FAILED:       os_printf("802_1X_AUTH_FAILED\n");       break;
                    case REASON_CIPHER_SUITE_REJECTED:    os_printf("CIPHER_SUITE_REJECTED\n");    break;
                    case REASON_BEACON_TIMEOUT:           os_printf("BEACON_TIMEOUT\n");           break;
                    case REASON_NO_AP_FOUND:              os_printf("NO_AP_FOUND\n");              break;
                    case REASON_AUTH_FAIL:                os_printf("AUTH_FAIL\n");                break;
                    case REASON_ASSOC_FAIL:               os_printf("ASSOC_FAIL\n");               break;
                    case REASON_HANDSHAKE_TIMEOUT:        os_printf("HANDSHAKE_TIMEOUT\n");        break;
                    default:                              os_printf("unknown\n");                  break;
                }
            }

        case EVENT_STAMODE_AUTHMODE_CHANGE:
            os_printf("wifi_event_handler: auth mode change old=%u/", evt->event_info.auth_change.old_mode);

            switch (evt->event_info.auth_change.old_mode) {
                case 0:  os_printf("open");         break;
                case 1:  os_printf("WEP");          break;
                case 2:  os_printf("WPA_PSK");      break;
                case 3:  os_printf("WPA2_PSK");     break;
                case 4:  os_printf("WPA_WPA2_PSK"); break;
                default: os_printf("unknown");      break;
            }

            os_printf(" new=%u/", evt->event_info.auth_change.new_mode);

            switch (evt->event_info.auth_change.new_mode) {
                case 0:  os_printf("open");         break;
                case 1:  os_printf("WEP");          break;
                case 2:  os_printf("WPA_PSK");      break;
                case 3:  os_printf("WPA2_PSK");     break;
                case 4:  os_printf("WPA_WPA2_PSK"); break;
                default: os_printf("unknown");      break;
            }

            os_printf("\n");

            break;

        case EVENT_STAMODE_GOT_IP:
            os_printf("wifi_event_handler: got ip ip=" IPSTR " mask=" IPSTR " gw=" IPSTR "\n",
                      IP2STR(&evt->event_info.got_ip.ip),
                      IP2STR(&evt->event_info.got_ip.mask),
                      IP2STR(&evt->event_info.got_ip.gw));

            mqtt_connect();

            break;

        case EVENT_STAMODE_DHCP_TIMEOUT:
            os_printf("wifi_event_handler: dhcp timeout\n");
            break;

        case EVENT_SOFTAPMODE_STACONNECTED:
            os_printf("wifi_event_handler: station connected mac=" MACSTR " aid=%u\n",
                      MAC2STR(evt->event_info.sta_connected.mac), evt->event_info.sta_connected.aid);
            break;

        case EVENT_SOFTAPMODE_STADISCONNECTED:
            os_printf("wifi_event_handler: station disconnected mac=" MACSTR " aid=%u\n",
                      MAC2STR(evt->event_info.sta_disconnected.mac), evt->event_info.sta_disconnected.aid);
            break;

        case EVENT_SOFTAPMODE_PROBEREQRECVED:
            /*
            os_printf("wifi_event_handler: probe request received mac=" MACSTR " rssi=%d\n",
                      MAC2STR(evt->event_info.ap_probereqrecved.mac), evt->event_info.ap_probereqrecved.rssi);
            */
            break;

        default:
            os_printf("wifi_event_handler: unknown event event=%u\n", evt->event);
            break;
    }
}
