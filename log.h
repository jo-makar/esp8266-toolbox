#ifndef LOG_H
#define LOG_H

#include <osapi.h>
#include <user_interface.h>

#include "config.h"
#include "missing-decls.h"

typedef struct {
    char buf[512];
    uint8_t lock;
} Log;
extern Log _log;

void log_init();

/* FIXME */
#define CRITICAL(a, b, ...) {}
#define ERROR(a, b, ...) {}
#define WARNING(a, b, ...) {}
#define INFO(a, b, ...) {}
#define DEBUG(a, b, ...) {}

//#define _PREFIX(level) { 
//    uint32_t t = system_get_time(); 
//    
//    os_printf("%u.%03u: " level ": %s:%d: ", 
//              t/1000000, (t%1000000)/1000, __FILE__, __LINE__); 
//    
//    if (LOG_BUF_ENABLED) { 
//        char first; 
//        int len; 
//        
//        log_buf_lock++; 
//        while (log_buf_lock > 1) os_delay_us(1000); 
//        
//        /* There are some quirks to the snprintf() implementation */ 
//        first = log_buf[0]; 
//        len = os_snprintf(log_buf, 1, "%u.%03u: " level ": %s:%d: ", 
//                          t/1000000, (t%1000000)/1000, __FILE__, __LINE__); 
//        log_buf[0] = first; 
//        
//        os_memmove(log_buf + len+1, log_buf, sizeof(log_buf)-(len+1)); 
//        os_snprintf(log_buf, len+1, "%u.%03u: " level ": %s:%d ", 
//                    t/1000000, (t%1000000)/1000, __FILE__, __LINE__); 
//        
//        log_buf_lock--; 
//    } 
//}
//
//#define CRITICAL_PREFIX _PREFIX("critical")
//#define ERROR_PREFIX _PREFIX("error")
//#define WARNING_PREFIX _PREFIX("warning")
//#define INFO_PREFIX _PREFIX("info")
//#define DEBUG_PREFIX _PREFIX("debug")
//
//#define CRITICAL(sys, fmt, ...) { 
//    if (sys##_LOG_LEVEL <= LEVEL_CRITICAL) { 
//        CRITICAL_PREFIX 
//        os_printf(fmt, ##__VA_ARGS__); 
//    } 
//    
//    while (1) os_delay_us(1000); 
//}
//
//#define ERROR(sys, fmt, ...) { 
//    if (sys##_LOG_LEVEL <= LEVEL_ERROR) { 
//        ERROR_PREFIX 
//        os_printf(fmt, ##__VA_ARGS__); 
//    } 
//}
//
//#define WARNING(sys, fmt, ...) { 
//    if (sys##_LOG_LEVEL <= LEVEL_WARNING) { 
//        WARNING_PREFIX 
//        os_printf(fmt, ##__VA_ARGS__); 
//    } 
//}
//
//#define INFO(sys, fmt, ...) { 
//    if (sys##_LOG_LEVEL <= LEVEL_INFO) { 
//        INFO_PREFIX 
//        os_printf(fmt, ##__VA_ARGS__); 
//    } 
//}
//
//#define DEBUG(sys, fmt, ...) { 
//    if (sys##_LOG_LEVEL <= LEVEL_DEBUG) { 
//        DEBUG_PREFIX 
//        os_printf(fmt, ##__VA_ARGS__); 
//    } 
//}
//
//void log_init();
//
//#define LOG_CRITICAL(sys, fmt, ...) { 
//    int i; 
//    
//    if (sys##_LOG_LEVEL <= LEVEL_CRITICAL) {
//log_prefix("critical", __FILE__, __LINE__, log_);    
//}
//    
//    os_printf(fmt, ##__VA_ARGS__); 
//    
//    for (i=0; ; i++) { 
//        if (i>0 && i%10==0) 
//            system_soft_wdt_feed(); 
//        os_delay_us(1000); 
//    } 
//}
//
//void log_prefix(const char *level, const char *file, int line, uint8_t sys);

#endif
