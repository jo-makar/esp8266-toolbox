#include <asm/termios.h>

/* #include <sys/ioctl.h> */
int ioctl(int d, unsigned long request, ...);

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int loop = 1;

void sighandler(int sig) {
    /* assert sig == SIGINT */
    (void)sig;

    loop = 0;
}

int main(int argc, char *argv[]) {
    char path[256] = "/dev/ttyUSB0";
    unsigned baud = 74880;
    int fd;
    struct termios2 tio;
    char buf[1024];
    size_t i = 0;
    ssize_t rv;

    if (argc > 3) {
        fprintf(stderr, "usage: %s device baud\n", argv[0]);
        return 1;
    }
    if (argc >= 2) {
        strncpy(path, argv[1], sizeof(path));
        path[sizeof(path)-1] = 0;
    }
    if (argc == 3)
        baud = atoi(argv[2]);

    if ((fd = open(path, O_RDONLY)) == -1)
        perror("open");

    /*
     * TODO Assuming similar to ioctl(TCGETS) but for struct termios2:
     *     Equivalent to tcgetattr(fd, tio).
     *     Get the current serial port settings.
     *     Ref: http://man7.org/linux/man-pages/man2/ioctl_tty.2.html
     */

    if (ioctl(fd, TCGETS2, &tio) == -1) {
        perror("ioctl(TCGETS2)");
        close(fd);
        return 1;
    }

    /*
     * TODO Print out the various fields of the strct termios2
     * Ref: /usr/src/linux-headers-xxx/include/uapi/asm-generic/termbits.h
     */

    /* Control mode flag: Baud speed mask */
    tio.c_cflag &= ~CBAUD;
    /* Control mode flag: Baud: other */
    tio.c_cflag |= BOTHER;
    /* Input, output speed */
    tio.c_ispeed = tio.c_ospeed = baud;

    /*
     * TODO Assuming similar to ioctl(TCSETS) but for struct termios2:
     * Equivalent to tcsetattr(fd, TCSANOW, tio).
     * Set the current serial port settings.
     * Ref: http://man7.org/linux/man-pages/man2/ioctl_tty.2.html
     */

    if (ioctl(fd, TCSETS2, &tio) == -1) {
        perror("ioctl(TCSETS2)");
        close(fd);
        return 1;
    }

    if (signal(SIGINT, sighandler) == SIG_ERR) {
        perror("signal");
        close(fd);
        return 1;
    }

    while (loop) {
        if ((rv = read(fd, buf+i, 1)) == -1) {
            perror("read");
            close(fd);
            return 1;
        }
        else if (rv == 1) {
            if (++i > sizeof(buf)-1) {
                fprintf(stderr, "++i > sizeof(buf)-1, aborting\n");
                close(fd);
                return 1;
            }

            if (buf[i-1] == '\n') {
                buf[i] = 0;
                printf("%s", buf);
                i = 0;
            }
        }

        /*
         * 74880 baud gives about 1/74880 => 13.3 us/bit.
         * Then eight bits per byte gives a wait time of >80 us.
         */
        usleep(80);
    }

    close(fd);
    return 0;
}
