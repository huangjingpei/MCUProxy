#include <sys/time.h>
#include "wallClock.h"

#include "sh_print.h"
#include "ioStream.h"

const unsigned long EPOCH = 2208988800UL; // delta between epoch time and ntp time
const double NTP_SCALE_FRAC = 4294967295.0; // maximum value of the ntp fractional part

static struct timeval initTime;
int saveStartTime()
{
    gettimeofday(&initTime, NULL);
    return 0;
}

void showUptime()
{
     struct timeval nowTime;
     uint32_t hour = 0, min = 0, sec = 0, day = 0;
     gettimeofday(&nowTime, NULL);
     sec = nowTime.tv_sec - initTime.tv_sec;
     min = sec / 60;
     sec %= 60;
     hour = min / 60;
     min %= 60;
     day = hour / 24;
     hour %= 24;
     shell_print("uptime: %d days, [%d%d:%d%d:%d%d] ",
                day, hour/10, hour%10, min/10, min%10, sec/10, sec%10);
}

void getWholeNTPNow(uint64_t *ntptime)
{
    if (!ntptime) {
        return;
    }

    uint32_t ntpsec, ntpusec;
    getApartNTPNow(&ntpsec, &ntpusec);

    *ntptime = (uint64_t)(ntpsec << 32 | ntpusec);
}

void getApartNTPNow(uint32_t *ntpsec, uint32_t *ntpusec)
{
    if (!ntpsec || !ntpusec) {
        return;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);

    *ntpsec = tv.tv_sec + EPOCH;
    *ntpusec = (tv.tv_usec * 1e-6) * NTP_SCALE_FRAC;
}

static uint32_t minusNTP(uint64_t ntp1, uint64_t ntp2)
{
    uint64_t diff = (ntp1 - ntp2) >> 16;
    if (diff < 0x0FFFFFFFFL) {
        return (uint32_t)diff;
    } else {
        return 0x0FFFFFFFFL;
    }
}

uint32_t minusNTPDelay(uint64_t ntptime)
{
    uint64_t _ntp;
    getWholeNTPNow(&_ntp);

    return minusNTP(_ntp, ntptime);
}
