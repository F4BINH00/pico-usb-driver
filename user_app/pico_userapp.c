#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define DEV_DEFAULT_1 "/dev/pico_usb"
#define DEV_DEFAULT_2 "/dev/pico_usb0"

#define CMD_LED_ON   0x01
#define CMD_LED_OFF  0x02
#define CMD_BLINK    0x03
#define CMD_ECHO     0x10

static int open_dev(const char *path)
{
    int fd = open(path, O_RDWR);
    if (fd >= 0) return fd;

    // fallback automático
    if (strcmp(path, DEV_DEFAULT_1) == 0) {
        fd = open(DEV_DEFAULT_2, O_RDWR);
        if (fd >= 0) return fd;
    }

    fprintf(stderr, "open(%s): %s\n", path, strerror(errno));
    fprintf(stderr, "Dica: crie a regra udev (README) ou use %s.\n", DEV_DEFAULT_2);
    return -1;
}

static void usage(const char *p)
{
    fprintf(stderr,
        "Uso:\n"
        "  %s [device] led on|off\n"
        "  %s [device] blink <reps> <period_ms>\n"
        "  %s [device] echo <texto>\n\n"
        "device (opcional): caminho /dev/... (ex.: /dev/pico_usb0 ou /dev/pico_usb-191)\n",
        p, p, p);
}

static int do_write_read(int fd, const uint8_t *tx, size_t txlen, uint8_t *rx, size_t rxmax)
{
    ssize_t w = write(fd, tx, txlen);
    if (w < 0) {
        fprintf(stderr, "write: %s\n", strerror(errno));
        return -1;
    }

    ssize_t r = read(fd, rx, rxmax);
    if (r < 0) {
        fprintf(stderr, "read: %s\n", strerror(errno));
        return -1;
    }

    return (int)r;
}

int main(int argc, char **argv)
{
    const char *dev = DEV_DEFAULT_1;

    int argi = 1;
    if (argc >= 2 && strncmp(argv[1], "/dev/", 5) == 0) {
        dev = argv[1];
        argi = 2;
    }

    if (argc - argi < 1) {
        usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[argi++];

    int fd = open_dev(dev);
    if (fd < 0) return 1;

    uint8_t tx[512];
    uint8_t rx[512];

    memset(tx, 0, sizeof(tx));
    size_t txlen = 0;

    if (strcmp(cmd, "led") == 0) {
        if (argc - argi < 1) { usage(argv[0]); close(fd); return 1; }
        const char *state = argv[argi++];

        tx[0] = (strcmp(state, "on") == 0) ? CMD_LED_ON : CMD_LED_OFF;
        txlen = 1;

    } else if (strcmp(cmd, "blink") == 0) {
        if (argc - argi < 2) { usage(argv[0]); close(fd); return 1; }
        int reps = atoi(argv[argi++]);
        int period_ms = atoi(argv[argi++]);
        int period10 = period_ms / 10;
        if (period10 < 1) period10 = 1;
        if (period10 > 255) period10 = 255;
        if (reps < 1) reps = 1;
        if (reps > 255) reps = 255;

        tx[0] = CMD_BLINK;
        tx[1] = (uint8_t)reps;
        tx[2] = (uint8_t)period10;
        txlen = 3;

    } else if (strcmp(cmd, "echo") == 0) {
        if (argc - argi < 1) { usage(argv[0]); close(fd); return 1; }
        const char *msg = argv[argi++];

        size_t mlen = strlen(msg);
        if (mlen > sizeof(tx) - 1) mlen = sizeof(tx) - 1;

        tx[0] = CMD_ECHO;
        memcpy(&tx[1], msg, mlen);
        txlen = 1 + mlen;

    } else {
        usage(argv[0]);
        close(fd);
        return 1;
    }

    int r = do_write_read(fd, tx, txlen, rx, sizeof(rx));
    if (r >= 0) {
        printf("RX(%d): ", r);
        fwrite(rx, 1, (size_t)r, stdout);
        printf("\n");
    }

    close(fd);
    return (r >= 0) ? 0 : 1;
}

