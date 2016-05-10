#ifndef HTTPD_UTILS
#define HTTPD_UTILS

int append(char **dest, size_t *destlen, const char *src);

/* These macros use the conn variable from the current scope */

#define APPEND(dest, destlen, src) \
    if (append(&dest, &destlen, src)) { \
        os_printf("APPEND: insufficient buffer length at %s:%u\n", __FILE__, __LINE__); \
        if (espconn_disconnect(conn)) \
            os_printf("APPEND: espconn_disconnect failed\n"); \
        return; \
    }

#define APRINTF(destbuf, destlen, line, fmt, ...) \
    { \
        if (ets_snprintf(line, sizeof(line), fmt, ##__VA_ARGS__) >= (int)sizeof(line)) { \
            os_printf("APRINTF: insufficient buffer length at %s:%u\n", __FILE__, __LINE__); \
            if (espconn_disconnect(conn)) \
                os_printf("APRINTF: espconn_disconnect failed\n"); \
        } \
        \
        APPEND(destbuf, destlen, line) \
    }

int querystring(const char **buf, size_t *len, const char **key, size_t *keylen,
                                               const char **val, size_t *vallen);

#endif
