#include "esp-missing-decls.h"
#include "string.h"

#include <osapi.h>
#include <user_interface.h>

void wifi_event_handler(System_Event_t *evt);

void wifi_init() {
    struct softap_config config;
    uint8_t opmode;

    /* Set the wifi mode to soft-AP */
    if ((opmode = wifi_get_opmode()) != 0x02) {
        if (!wifi_set_opmode(0x02)) {
            os_printf("wifi_init: wifi_set_opmode failed\n");
            return;
        }
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

    /* wifi_get_ip_info cannot be used within user_init */
    /* wifi_get_macaddr however can be used here */

    wifi_set_event_handler_cb(wifi_event_handler);
}

void wifi_event_handler(System_Event_t *evt) {
    switch (evt->event) {
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
