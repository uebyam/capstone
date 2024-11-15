#include "cyhal.h"
#include "cycfg.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "string.h"
#include <time.h>

#define STRING_BUFFER_SIZE (80)

void init_rtc(bool ask);
void read_rtc(struct tm *date_time);
char convert_rtc_to_str(struct tm *date_time, char *date_string);
int convert_rtc_to_int(struct tm *date_time);
