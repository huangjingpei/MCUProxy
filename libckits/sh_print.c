#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#include "sh_print.h"

static int s_fd_shell_log     = -1;
static char s_buf_shell_log[1024];
static pthread_mutex_t s_atomic;
static struct sockaddr_un s_addr_shell_log;


static int open_unix_socket()
{
    int     sockfd;

    sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);

    bzero(&s_addr_shell_log, sizeof(s_addr_shell_log));
    s_addr_shell_log.sun_family = AF_LOCAL;
    strcpy(s_addr_shell_log.sun_path, "/tmp/pntServer");

    return sockfd;
}


int shell_print(const char* format, ...)
{
    va_list vlist;
    int ret = -1;

    if (s_fd_shell_log < 0) {
        ret = pthread_mutex_init(&s_atomic, NULL);

        if (0 != ret) {
            printf("%s:%d sem_init() failed.\n",__FUNCTION__,__LINE__);
        }

        s_fd_shell_log = open_unix_socket();
        if(s_fd_shell_log <0) {
            printf("%s:%d open_unix_socket() failed.\n",__FUNCTION__,__LINE__);
            return -1;
        }

        //no block
        int bFlag = 0;
        bFlag= fcntl(s_fd_shell_log, F_GETFL, 0);
        if(fcntl(s_fd_shell_log, F_SETFL, O_NONBLOCK | bFlag) == -1) {
            printf("%s:%d set O_NONBLOCK failed.\n",__FUNCTION__,__LINE__);
            return -1;
        }
    }

    pthread_mutex_lock(&s_atomic);

    va_start(vlist, format);
    vsnprintf(s_buf_shell_log, 1024, format, vlist);
    va_end(vlist);

    if (s_fd_shell_log > 0) {
        ret = sendto(s_fd_shell_log, s_buf_shell_log, strlen(s_buf_shell_log),
            0, (struct sockaddr*) &s_addr_shell_log, sizeof(s_addr_shell_log) );
    }

    pthread_mutex_unlock(&s_atomic);

    return ret;
}

