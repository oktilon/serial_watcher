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
#define  TIME_SZ            32
#define  BUFF_SZ            256
#define  PATH_SZ            1024
#define  NAME_SZ            PATH_SZ - TIME_SZ - 1
#define  TEXT_SZ            32768
#define  DIAG_SZ            128
#define  DIAG_TX            24

typedef struct queueItemStr {
    const char *msg;
    struct queueItemStr *next;
} queueItem;

typedef enum StateEnum {
    STATE_NONE,
    STATE_LOGIN,
    STATE_PASS,
    STATE_IP,
    STATE_COMMAND
} State;


UartDevice  gDev                = {};
FILE       *gLogHandle          = NULL;
int         gCurWeek            = -1;
char        gLogTime[TIME_SZ]   = "";
char        gLogName[NAME_SZ]   = "";
char        gBuff[BUFF_SZ]      = "";
char        gText[TEXT_SZ]      = "";
size_t      gLen                = 0;
queueItem  *gQueue              = NULL;
State       gState              = STATE_NONE;
bool        gDiagnostic         = false;

// Long command line options
const struct option longOptions[] = {
    {"device",          required_argument,  0,  'd'},
    {"baud",            required_argument,  0,  'b'},
    {"log",             required_argument,  0,  'l'}
    {"diagnostic",      no_argument,        0,  'x'}
};

void on_terminate (int sig) {
    uart_stop (&gDev);
    if (gLogHandle)
        fclose(gLogHandle);
    printf("\nExit by signal: %d\n", sig);
    exit (-EINTR);
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

bool selfLogCheckTime () {
    // Static buffer
    static char buf[TIME_SZ] = {0};
    // Variables
    time_t t;
    struct tm *pt;

    t = time (NULL);
    pt = localtime (&t);
    // Week-number %V
    // Day-number %j
    strftime (buf, TIME_SZ, "%V", pt);

    if (strcmp(buf, gLogTime) != 0) {
        strncpy(gLogTime, buf, TIME_SZ);
        return true;
    }

    return false;
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

bool selfLogOpen () {
    char path[PATH_SZ];

    if (gLogHandle) {
        fclose (gLogHandle);
        gLogHandle = NULL;
    }

    snprintf(path, PATH_SZ, "%s-%s", gLogName, gLogTime);
    gLogHandle = fopen(path, "a");

    return gLogHandle != NULL;
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

    if (selfLogCheckTime()) {
        selfLogOpen ();
    }

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
                snprintf(gDev.filename, DEV_SZ, "%s", optarg);
                break;

            case 'l': // log
                snprintf(gLogName, NAME_SZ, "%s", optarg);
                break;

            case 'b': // baud
                gDev.num_rate = atoi(optarg);
                gDev.rate = uart_rate_is_valid (gDev.num_rate);
                break;

            case 'x': // diagnostic
                gDiagnostic = true;
                break;

            default:
                break;
        }
    }
}

const char * analyze () {
    if(strstr(gText, STR_LOGIN)) {
        gState = STATE_LOGIN;
        return  USER "\n";
    }

    if(strstr(gText, "Password:")) {
        if (gState == STATE_LOGIN) {
            gState = STATE_PASS;
            return PASS "\n";
        }
    }

    if(strstr(gText, STR_PROMPT)) {
        switch (gState) {
            case STATE_PASS:
                gState = STATE_IP;
                return "ip a\n";

            case STATE_IP:
                gState = STATE_COMMAND;
                return COMMAND "\n";
        }
    }

    if(strstr(gText, STR_SUDO)) {
        return PASS "\n";
    }

    return NULL;
}

void dump () {
    selfLog(gText);
    memset (gText, 0, TEXT_SZ);
    gLen = 0;
}

int main (int argc, char **argv) {
    char c;
    size_t rd;
    const char *ret;
    char *next;
    char bytes[DIAG_SZ];
    char text[DIAG_TX];
    char byte[8];

    signal (SIGTERM, on_terminate);
    signal (SIGKILL, on_terminate);
    signal (SIGSEGV, on_terminate);
    signal (SIGSTOP, on_terminate);
    signal (SIGINT, on_terminate);

    memset (gText, 0, TEXT_SZ);


    snprintf(gLogName, NAME_SZ, "%s", LOG_FILENAME);
    // UART
    snprintf(gDev.filename, DEV_SZ, "%s", "/dev/ttyAMA3");
    gDev.rate = B115200;

    app_parse_arguments (argc, argv);

    if (!gDev.rate) {
        selfLog ("ERROR: Unsupported rate %d", gDev.num_rate);
        return -EINVAL;
    }

    selfLogCheckTime ();
    if(!selfLogOpen ()) {
        selfLog ("ERROR: Openlog error(%d): %m", errno);
        return -EBADF;
    }

    selfLog ("Watcher v.%s started [%s, %d, %s-%s]", VERSION_STR, gDev.filename, gDev.rate, gLogName, gLogTime);

    if (uart_start(&gDev, false) < 0) {
        selfLog ("ERROR: Init UART error");
        return -EACCES;
    }

    while (1)  {
        rd = uart_reads(&gDev, gBuff, BUFF_SZ);
        if (rd > 0) {
            if (gDiagnostic) {
                selfLog (">> %d bytes:", rd);
                strncpy (gText, gBuff, rd);
                gLen = 0;
                next = gBuff;
                while (rd > 0) {
                    memset(bytes, 0, DIAG_SZ);
                    memset(text, 0, DIAG_TX);
                    for (int i = 0; i < 16; i++) {
                        char c = rd ? next[i] : 0;
                        if (rd) {
                            sprintf (byte, " %02X", c);
                        } else {
                            sprintf (byte, "   ");
                        }

                        if (i && ((i%8) == 0))
                            strcat(bytes, " ");

                        strcat(bytes, byte);
                        text[i] = c >= ' ' ? c : (rd ? '.' : ' ');

                        if (rd) rd--;
                    }
                    strcat(bytes, "  |");
                    strcat(bytes, text);
                    strcat(bytes, "|");

                    selfLog (bytes);
                    if (rd) next += 16;
                }
            } else {
                for(int i = 0; i < rd; i++) {
                    c = gBuff[i];
                    switch (c) {
                        case '\r':
                            break;

                        case '\n':
                        case 0:
                            dump ();
                            break;

                        default:
                            gText[gLen++] = c;
                            break;
                    }

                    if (gLen + 2 >= TEXT_SZ) {
                        dump ();
                    }
                }
            }
            ret = analyze ();

            if (ret) {
                if (gLen) {
                    dump ();
                }
                uart_writes (&gDev, ret);
            }
        }
    }

   return EXIT_SUCCESS;
}