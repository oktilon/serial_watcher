/*
 * uart.c
 *
 * @date 2019/08/09
 * @author Cosmin Tanislav
 * @author Cristian Fatu
 */

 #include <string.h>
 #include <stdbool.h>
 #include <stdlib.h>
 #include <stdio.h>
 #include <fcntl.h>
 #include <termios.h>
 #include <errno.h>
 #include <limits.h>

 #include "sw_uart.h"
 #include "app.h"

/*
 * Start the UART device.
 *
 * @param dev points to the UART device to be started, must have filename and rate populated
 * @param canonical whether to define some compatibility flags for a canonical interface
 *
 * @return - 0 if the starting procedure succeeded
 *         - negative if the starting procedure failed
 */
int uart_start(UartDevice* dev, bool canonical) {
    struct termios *tty;
    int fd;
    int rc;

    selfLog ("Open port [%s]", dev->filename);
    fd = open(dev->filename, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        selfLog("Failed to open UART device");
        return -EBADF;
    }

    selfLog ("Malloc TTY");
    tty = malloc(sizeof(*tty));
    if (!tty) {
        selfLog("Failed to allocate UART TTY instance");
        return -ENOMEM;
    }

    selfLog ("Memset TTY");
    memset(tty, 0, sizeof(*tty));

    /*
    * Set baud-rate.
    */
    tty->c_cflag |= dev->rate;

    /* Ignore framing and parity errors in input. */
    tty->c_iflag |=  IGNPAR;

    /* Use 8-bit characters. This too may affect standard streams,
    * but any sane C library can deal with 8-bit characters. */
    tty->c_cflag |=  CS8;

    /* Enable receiver. */
    tty->c_cflag |=  CREAD;

    if (canonical) {
        /* Enable canonical mode.
        * This is the most important bit, as it enables line buffering etc. */
        tty->c_lflag |= ICANON;
    } else {
        /* To maintain best compatibility with normal behaviour of terminals,
        * we set TIME=0 and MAX=1 in noncanonical mode. This means that
        * read() will block until at least one byte is available. */
        tty->c_cc[VTIME] = 0;
        tty->c_cc[VMIN] = 1;
    }

    /*
    * Flush port.
    */
    selfLog ("Flush port");
    tcflush(fd, TCIFLUSH);

    /*
    * Apply attributes.
    */
    selfLog ("Set port attr");
    rc = tcsetattr(fd, TCSANOW, tty);
    if (rc) {
        printf("%s: failed to set attributes\r\n", __func__);
        return rc;
    }

    dev->fd = fd;
    dev->tty = tty;

    return 0;
}

/*
 * Read a string from the UART device.
 *
 * @param dev points to the UART device to be read from
 * @param buf points to the start of buffer to be read into
 * @param buf_len length of the buffer to be read
 *
 * @return - number of bytes read if the read procedure succeeded
 *         - negative if the read procedure failed
 */
int uart_reads(UartDevice* dev, char *buf, size_t buf_len) {
    int rc;

    rc = read(dev->fd, buf, buf_len - 1);
    if (rc < 0) {
        printf("%s: failed to read uart data\r\n", __func__);
        return rc;
    }

    buf[rc] = '\0';
    return rc;
}

/*
 * Write data to the UART device.
 *
 * @param dev points to the UART device to be written to
 * @param buf points to the start of buffer to be written from
 * @param buf_len length of the buffer to be written
 *
 * @return - number of bytes written if the write procedure succeeded
 *         - negative if the write procedure failed
 */
int uart_writen(UartDevice* dev, char *buf, size_t buf_len) {
    return write(dev->fd, buf, buf_len);
}

/*
 * Write a string to the UART device.
 *
 * @param dev points to the UART device to be written to
 * @param string points to the start of buffer to be written from
 *
 * @return - number of bytes written if the write procedure succeeded
 *         - negative if the write procedure failed
 */
int uart_writes(UartDevice* dev, char *string) {
    size_t len = strlen(string);
    return uart_writen(dev, string, len);
}

/*
 * Stop the UART device.
 *
 * @param dev points to the UART device to be stopped
 */
void uart_stop(UartDevice* dev) {
    free(dev->tty);
}

ValidRate validRates[] = {
    { 50, B50 },
    { 75, B75 },
    { 110, B110 },
    { 134, B134 },
    { 150, B150 },
    { 200, B200 },
    { 300, B300 },
    { 600, B600 },
    { 1200, B1200 },
    { 1800, B1800 },
    { 2400, B2400 },
    { 4800, B4800 },
    { 9600, B9600 },
    { 19200, B19200 },
    { 38400, B38400 },
    { 57600, B57600 },
    { 115200, B115200 },
    { 230400, B230400 },
    { 460800, B460800 },
    { 500000, B500000 },
    { 576000, B576000 },
    { 921600, B921600 },
    { 1000000, B1000000 },
    { 1152000, B1152000 },
    { 1500000, B1500000 },
    { 2000000, B2000000 },
    { 2500000, B2500000 },
    { 3000000, B3000000 },
    { 3500000, B3500000 },
    { 4000000, B4000000 },
    { 0, 0 }
};

int uart_rate_is_valid(int num) {
    int ix = 0;

    while (validRates[ix].num) {
        if (validRates[ix].num == num) {
            return validRates[ix].baud;
        }
        ix++;
    }
    return 0;
}