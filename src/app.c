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
#include <signal.h>

#include "app.h"
#include "sw_uart.h"
#define  WEEK_SZ            8
#define  BUFF_SZ            2048

typedef struct queueItemStr {
    const char *msg;
    struct queueItemStr *next;
} queueItem;


UartDevice  gDev                = {};
FILE       *gLogHandle          = NULL;
int         gCurWeek            = -1;
char        gLogName[PATH_SZ]   = "";
char        buff[BUFF_SZ + 1]   = "";
queueItem  *gQueue              = NULL;
int         gSys                = 0;

// Long command line options
const struct option longOptions[] = {
    {"device",          required_argument,  0,  'd'},
    {"baud",            required_argument,  0,  'b'},
    {"log",             required_argument,  0,  'l'}
};

void on_terminate (int sig) {
    uart_stop (&gDev);
    if (gLogHandle)
        fclose(gLogHandle);
    printf("Exit by signal: %d\n", sig);
    exit (-2);
}

void queue_add (const char *msg) {
    queueItem *n = gQueue;
    queueItem *pq = (queueItem *)calloc (1, sizeof(queueItem));
    pq->msg = msg;
    if (gQueue) {
        while (n->next) { n = n->next; }
        n->next = pq;
    } else {
        gQueue = pq;
    }
}

const char * queue_get () {
    queueItem *n = gQueue;
    const char *ret = NULL;
    if (gQueue) {
        ret = n->msg;
        gQueue = n->next;
        free (n);
    }
    return ret;
}

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

void analyze (const char *txt) {
    // selfLog (txt);
    if(strstr(txt, "emak login:")) {
        queue_add ("defigo\r\n");
        gSys = 1;
    }

    if(strstr(txt, "orangepi3b login:")) {
        queue_add ("joy\r\n");
        gSys = 2;
    }

    if(strstr(txt, "Password:")) {
        if (gSys == 2) {
            queue_add ("joipaw\r\n");
        } else {
            queue_add ("defigo\r\n");
        }
        gSys = 0;
    }

    if(strstr(txt, "defigo@emak:~$")) {
        if (gSys == 0) {
            queue_add ("tail -f logs/access.log\r\n");
            gSys = 100;
        }
    }


    if(strstr(txt, "joy@orangepi3b:~$")) {
        if (gSys == 0) {
            queue_add ("ls -la\r\n");
            gSys = 100;
        }
    }
}

int main (int argc, char **argv) {
    size_t rd, ln;
    char week[WEEK_SZ] = "";
    const char *next;
    time_t t = time(NULL);
    struct tm* ptm = localtime(&t);
    char bytes[64];
    char text[17];
    char byte[3];

    signal (SIGTERM, on_terminate);
    signal (SIGKILL, on_terminate);
    signal (SIGSEGV, on_terminate);
    signal (SIGSTOP, on_terminate);
    signal (SIGINT, on_terminate);

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
    if (uart_start(&gDev, false) < 0) {
        selfLog ("Init error");
        return -1;
    }

    selfLog ("Start loop");
    while (1)  {
        memset (buff, 0, BUFF_SZ + 1);
        rd = uart_reads(&gDev, buff, BUFF_SZ);
        if (rd > 0) {
            selfLog (">> %d bytes:", rd);
            analyze (buff);
            next = buff;
            while (rd > 0) {
                memset(bytes, 0, 64);
                memset(text, 0, 17);
                for (int i = 0; i < 16; i++) {
                    char c = rd ? next[i] : 0;
                    sprintf (byte, "%02X", c);

                    if (i && ((i%4) == 0))
                        strcat(bytes, " ");

                    strcat(bytes, byte);
                    text[i] = c >= ' ' ? c : '.';

                    if (rd) rd--;
                }
                strcat(bytes, "  ");
                strcat(bytes, text);

                selfLog (bytes);
                if (rd) next += 16;
            }
            // for(int i = 0; i < rd; i++) {
            //     if (buff[i] == '\r' || buff[i] == '\n' || buff[i] == 0) {
            //         if (buff[i] == '\r')
            //             ln++;
            //         buff[i] = 0;
            //         selfLog ("=== clear");
            //         if (ln)
            //             analyze (next);
            //         ln = 0;
            //         next = buff + i + 1;
            //     } else {
            //         ln++;
            //     }
            // }
            // if (ln) {
            //     selfLog ("only %d byte", rd);
            //     analyze (next);
            // }

            while ((next = queue_get())) {
                uart_writes (&gDev, next);
            }
        }
    }

   return EXIT_SUCCESS;
}