# esp8266-toolbox
Motley of servers, clients and drivers for the ESP8266

# Status
- [ ] HTTP server framework
  - How to practically provide a server while in station-only mode?
  - State-machine design (ie push/pull data with callbacks handled by a task)
- [ ] Over-The-Air updates
  - [ ] RSA signed firmware 
  - [ ] Push updates using HTTP/S server
  - [ ] Pull updates using HTTP/S client (or by holding GPIO0 low x seconds?)
- [ ] SMTP client framework
  - [ ] PLAIN, CRAM-MD5 authentication
  - [ ] SSL support (via BearSSL or similar)
- [x] Logging framework inspired by syslog
  - [ ] Store entries in a circular buffer (to be sent by SMTP/HTTP)
  - [ ] Use SNTP for timestamps if available
- [ ] Environment monitoring
  - [ ] Ambient light
    - [ ] TLS2561 light sensing
    - [ ] Add interrupt support
  - [ ] Motion, vibration
  - [ ] Temperature, humidity
- [x] Custom board design
- [ ] Battery circuit monitoring
- [ ] Sleep mode for power conservation
- [ ] Self-diagnosis and healing
  - [ ] Monitor heap usage and stack pointer
- [ ] MQTT client framework

# Quick start
- Install the toolchain and SDK: https://github.com/pfalcon/esp-open-sdk
- Modify the Makefile variables in config.mk if necessary
- Modify the WiFi, SMTP, etc constants in config.h for your installation
- Run `make` to build the application binaries
- Power the circuit and connect via a USB-UART bridge
- Put the ESP8266 in flash mode by holding GPIO0 low during boot
- Run `make flash` to flash the application over USB/UART0

# Hardware
This software was intended for use with the common eight-pin ESP-12, though it
will work with any ESP8266 module with only minor modifications.

# SMTP client framework
A simple SMTP client for sending mails and texts is available in smtp/.

Currently only supporting non-SSL/TLS SMTP providers (eg SMTP2GO).

Internally designed using a clean state-machine (ie asynchronously push/pull
data) to conceal the callbacks used by the SDK and simplify porting to other
systems.

However the user interface is purposefully made simple (ie just `launch()` and
`status()`) because the SDK has no mechanism to "release" the processor to run
other tasks/timers, or to put it differently the scheduler only triggers when
nothing else is executing.  Compare this to a typical RTOS where a `delay()` or
`sleep()` function generally triggers rescheduling.

The following shows the user code compared to a simplified view of what happens
internally.

```c
smtp_send_launch("world@internet", "Hello world", NULL);
/* ... */

/* If needed and later after the function calling launch() has returned */
if ((state = smtp_send_status()) == STMP_STATE_ERROR)
    /* Handle error */
else if (state == SMTP_STATE_RESOLVE)
    /* Still resolving SMTP server host */
/* ... */
```

```c
struct {
    int8_t state;
    /* ... */
    os_timer_t timer;
} smtp_state;

ICACHE_FLASH_ATTR void smtp_send_launch(const char *to, const char *subj,
                                        const char *body) {
    /* Validate and buffer input */

    smtp_state.state = SMTP_STATE_START;
    smtp_handler(NULL);
}

ICACHE_FLASH_ATTR smtp_handler(void *arg) {
    if (smtp_state.state == SMTP_STATE_ERROR) {
        /* Handle error */
        return;
    }

    else if (smtp_state.state == SMTP_STATE_START) {
        /* Start process */
        smtp_state.state = SMTP_STATE_RESOLVE;
    }

    else if (smtp_state.state == SMTP_STATE_RESOLVE) {
        /* Launch SMTP server hostname resolution */
    }

    else if (smtp_state.state == SMTP_STATE_CONNECT) {
        /* Connect to the SMTP server */
    }

    /* ... */

    /* Schedule an execution of itself in three seconds */
    os_timer_disarm(&smtp_state.timer);
    os_timer_setfn(&smtp_state.timer, smtp_handler, NULL);
    os_timer_arm(&smtp_state.timer, 1000*3, false);
}
```

# Logging framework
A logging framework akin to Linux's syslog(3) is available via log.h.

An example use case from user_init.c:

`log_info("main", "version %s built on %s", VERSION, BUILD_DATETIME);`

Will produce a log entry similar to the following:

`00:00:15.506: info: user_init.c:13: version 1.0.0 built on Aug 10 2017 06:52:39`

Which is comprised of the system time (hours:minutes:seconds.milliseconds), log
level, file path and line number and finally entry proper.

The log entries are sent to UART0 (with the default baudrate and params).

Run `make uart0` to execute a host application that will read the output
produced from UART0 including the generated log entries. The host application
is provided to support USB-UART bridges that can use non-standard baudrates only
via ioctl() calls, eg the CP2104.

# Custom board design
A simple, two-layer breakout board was designed to simplify development, the
schematic and gerbers are located in board/.

The headers provided are intended to support attaching external boards for
sensors or actuators which are to be interfaced by multiplexing the available
pins (eg TX, RX, GPIO0 and GPIO2).

# License
This software is freely available for non-commerical use, commerical use requires
an expressed agreement from the author. The preceding clause must be included in
all derivative software developed with commerical rights retained by the author
of this software.

The author takes no responsibility for the consequences of the use of this
software.

Use of the software in any form indicates acknowledgement of the licensing terms.

Copyright (c) 2017 J. O. Makar <jomakar@gmail.com>
