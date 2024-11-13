#ifndef RTC_H
#define RTC_H

#include "cyhal.h"
#include "cycfg.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "string.h"
#include <time.h>
#include "eepromManager.h"

#define STRING_BUFFER_SIZE (80)

void init_rtc(bool ask);
void read_rtc(struct tm *date_time);
void convert_rtc_to_str(struct tm *date_time, char *date_string);
int convert_rtc_to_int(struct tm *date_time);

extern time_t global_staging_time[MAX_TIMESTAMP_COUNT];

#endif
