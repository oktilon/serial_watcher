#define _GNU_SOURCE
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <getopt.h>

#include "app.h"
#include "sw_uart.h"
#define  WEEK_SZ            8
#define  BUFF_SZ            2048


UartDevice  gDev                = {};
FILE       *gLogHandle          = NULL;
int         gCurWeek            = -1;
char        gLogName[PATH_SZ]   = "";
char        buff[BUFF_SZ + 1]   = "";

// Long command line options
const struct option longOptions[] = {
    {"device",          required_argument,  0,  'd'},
    {"baud",            required_argument,  0,  'b'},
    {"log",             required_argument,  0,  'l'}
};

const char * selfLogTimestamp () {
    // Static buffer
    static char buf[60] = {0};
    // Variables
    int r;
    size_t sz;
    char msec[30] = {0};
    struct timeval tv;
    struct tm t = {0};

    // Create timestamp with ms
    gettimeofday (&tv, NULL);
    localtime_r (&tv.tv_sec, &t);
    r = snprintf (msec, 30, "%ld", tv.tv_usec / 1000);
    for (; r < 3; r++) strcat (msec, "0");

    sz = strftime (buf, sizeof (buf), "%F %T", &t); // %F => %Y-%m-%d,  %T => %H:%M:%S
    snprintf (buf + sz, 60, ".%s", msec);

    return buf;
}


/**
 * @brief Logging main body
 *
 * @param fmt log format
 * @param argp log other arguments
 */
void selfLog (const char* fmt, ...) {
    // Variables
    int r;
    char *msg = NULL;
    const char *tms = selfLogTimestamp ();

    // Format log message
    va_list arglist;
    va_start (arglist, fmt);
    r = vasprintf (&msg, fmt, arglist);
    va_end (arglist);

    // Output log
    printf ("%s: %s\n", tms, msg);
    if (gLogHandle) {
        fprintf (gLogHandle, "%s: %s\n", tms, msg);
        fflush(gLogHandle);
    }


    // Free formatted log buffer
    if (r && msg)
        free (msg);
}

/**
 * @brief Parse cmdline arguments
 *
 * @param argc args count
 * @param argv args list
 */
void app_parse_arguments (int argc, char **argv) {
    int i;

    while ((i = getopt_long (argc, argv, "d:l:b:", longOptions, NULL)) != -1) {
        switch (i) {
            case 'd': // device
                snprintf(gDev.filename, PATH_SZ, "%s", optarg);
                break;

            case 'l': // log
                snprintf(gLogName, PATH_SZ, "%s", optarg);
                break;

            case 'b': // baud
                gDev.num_rate = atoi(optarg);
                gDev.rate = uart_rate_is_valid (gDev.num_rate);
                break;

            default:
                break;
        }
    }
}

int main (int argc, char **argv) {
    size_t rd;
    char week[WEEK_SZ] = "";
    time_t t = time(NULL);
    struct tm* ptm = localtime(&t);

    strftime(week, WEEK_SZ, "%V", ptm);
    strftime(gLogName, PATH_SZ, LOG_FILENAME, ptm);
    gCurWeek = atoi(week);
    // UART
    snprintf(gDev.filename, PATH_SZ, "%s", "/dev/ttyAMA3");
    gDev.rate = B115200;

    app_parse_arguments (argc, argv);

    if (!gDev.rate) {
        selfLog ("Unsupported rate %d", gDev.num_rate);
        return -EINVAL;
    }

    gLogHandle = fopen (gLogName, "a");
    if (!gLogHandle) {
        selfLog ("Openlog error(%d): %m", errno);
        return -EBADF;
    }

    selfLog ("Watcher v.%s started [%s, %d, %s]", VERSION_STR, gDev.filename, gDev.rate, gLogName);

    selfLog ("Init uart");
    if (uart_start(&gDev, 1) < 0) {
        selfLog ("Init error");
        return -1;
    }

    selfLog ("Start loop");
    while (1)  {
        memset (buff, 0, BUFF_SZ + 1);
        rd = uart_reads(&gDev, buff, BUFF_SZ);
        selfLog ("Got %d bytes", rd);
        if (rd > 0) {
            selfLog (buff);
        }
        sleep(1);
    }

   return EXIT_SUCCESS;
}