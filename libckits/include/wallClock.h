#ifndef __WALLCLOCK_H__
#define __WALLCLOCK_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

int saveStartTime();
void showUptime();

void getWholeNTPNow(uint64_t *ntptime);
void getApartNTPNow(uint32_t *ntpsec, uint32_t *ntpusec);
uint32_t minusNTPDelay(uint64_t ntptime);

#ifdef __cplusplus
};
#endif

#endif

