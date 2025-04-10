#include <termios.h>
#include <unistd.h>
#include <stdbool.h>

#ifndef INC_SW_UART_H_
#define INC_SW_UART_H_

#define  DEV_SZ            32

typedef struct ValidRateStr {
    int num;
    int baud;
} ValidRate;

typedef struct UartDeviceStr {
    char filename[DEV_SZ];
    int rate;
    int num_rate;

    int fd;
    struct termios *tty;
} UartDevice;

int uart_start(UartDevice *dev, bool canonical);
void uart_stop(UartDevice *dev);
int uart_writen(UartDevice *dev, const char *buf, size_t buf_len);
int uart_writes(UartDevice *dev, const char *str);
int uart_reads(UartDevice *dev, char *buf, size_t buf_len);
int uart_rate_is_valid(int num);


#endif // INC_SW_UART_H_