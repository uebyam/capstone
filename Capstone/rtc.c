/******************************************************************************
 * Include header files
 ******************************************************************************/
#include "cy_result.h"
#include "cy_syslib.h"
#include "cyhal.h"
#include "cycfg.h"
#include "cybsp.h"
#include "cy_pdl.h"
#include "cy_retarget_io.h"
#include "cyhal_gpio.h"
#include "cyhal_system.h"
#include "cyhal_uart.h"
#include "string.h"
#include "ansi.h"
#include <time.h>

/*******************************************************************************
* Macros
*******************************************************************************/

#define UART_TIMEOUT_MS (10u)      /* in milliseconds */
#define INPUT_TIMEOUT_MS (120000u) /* in milliseconds */

#define STRING_BUFFER_SIZE (80)

/* Available commands */
#define RTC_CMD_SET_DATE_TIME ('1')
#define RTC_CMD_CONFIG_DST ('2')

#define RTC_CMD_ENABLE_DST ('1')
#define RTC_CMD_DISABLE_DST ('2')
#define RTC_CMD_QUIT_CONFIG_DST ('3')

#define FIXED_DST_FORMAT ('1')
#define RELATIVE_DST_FORMAT ('2')

/* Macro used for checking validity of user input */
#define MIN_SPACE_KEY_COUNT (5)

/* Structure tm stores years since 1900 */
#define TM_YEAR_BASE (1900u)

/* Maximum value of seconds and minutes */
#define MAX_SEC_OR_MIN (60u)

/* Maximum value of hours definition */
#define MAX_HOURS_24H (23UL)

/* Month per year definition */
#define MONTHS_PER_YEAR (12U)

/* Days per week definition */
#define DAYS_PER_WEEK (7u)

/* Days in month */
#define DAYS_IN_JANUARY (31U)   /* Number of days in January */
#define DAYS_IN_FEBRUARY (28U)  /* Number of days in February */
#define DAYS_IN_MARCH (31U)     /* Number of days in March */
#define DAYS_IN_APRIL (30U)     /* Number of days in April */
#define DAYS_IN_MAY (31U)       /* Number of days in May */
#define DAYS_IN_JUNE (30U)      /* Number of days in June */
#define DAYS_IN_JULY (31U)      /* Number of days in July */
#define DAYS_IN_AUGUST (31U)    /* Number of days in August */
#define DAYS_IN_SEPTEMBER (30U) /* Number of days in September */
#define DAYS_IN_OCTOBER (31U)   /* Number of days in October */
#define DAYS_IN_NOVEMBER (30U)  /* Number of days in November */
#define DAYS_IN_DECEMBER (31U)  /* Number of days in December */

/* Flags to indicate the if the entered time is valid */
#define DST_DISABLED_FLAG (0)
#define DST_VALID_START_TIME_FLAG (1)
#define DST_VALID_END_TIME_FLAG (2)
#define DST_ENABLED_FLAG (3)

/* Macro to validate seconds parameter */
#define IS_SEC_VALID(sec) ((sec) <= MAX_SEC_OR_MIN)

/* Macro to validate minutes parameters */
#define IS_MIN_VALID(min) ((min) <= MAX_SEC_OR_MIN)

/* Macro to validate hour parameter */
#define IS_HOUR_VALID(hour) ((hour) <= MAX_HOURS_24H)

/* Macro to validate month parameter */
#define IS_MONTH_VALID(month) (((month) > 0U) && ((month) <= MONTHS_PER_YEAR))

/* Macro to validate the year value */
#define IS_YEAR_VALID(year) ((year) > 0U)

/* Checks whether the year passed through the parameter is leap or not */
#define IS_LEAP_YEAR(year) \
(((0U == (year % 4UL)) && (0U != (year % 100UL))) || (0U == (year % 400UL)))

/*******************************************************************************
* Global Variables
*******************************************************************************/
cyhal_rtc_t rtc_obj;
uint32_t dst_data_flag = 0;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/

static void set_time(uint32_t timeout_ms);
static bool validate_date_time(int sec, int min, int hour,
                                int mday, int month, int year);
static int get_day_of_week(int day, int month, int year);
static void set_dst_feature(uint32_t timeout_ms);
static cy_rslt_t fetch_time_data(char *buffer,
                             uint32_t timeout_ms, uint32_t *space_count);
static int get_week_of_month(int day, int month, int year);

/*******************************************************************************
* Function Name: handle_error
********************************************************************************
* Summary:
* User defined error handling function
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void handle_error(void)
{
    LOG_ERR("RTC Error");
    /* Disable all interrupts. */
    __disable_irq();

    CY_ASSERT(0);
}

// initialise rtc
void init_rtc(void) {
    cy_rslt_t rslt = cyhal_rtc_init(&rtc_obj);
    if (CY_RSLT_SUCCESS != rslt)
    {
        handle_error();
    }
    
    LOG_INFO("input in HH MM SS DD MM YY\n");
    set_time(INPUT_TIMEOUT_MS);
}

void read_rtc(struct tm *date_time){
    cy_rslt_t rslt;
    rslt = cyhal_rtc_read(&rtc_obj, date_time);
    if (rslt != CY_RSLT_SUCCESS) {
        handle_error();
    }
}

int convert_rtc_to_int(struct tm *date_time) {
    return mktime(date_time);
}

void convert_rtc_to_str(struct tm *date_time, char *date_string) {
    strftime(date_string, STRING_BUFFER_SIZE, "%c", date_time); 
    printf("%s\r\n", date_string);
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*   This function:
*  - Initializes the device and board peripherals
*  - Initializes RTC
*  - The loop checks for the user command and process the commands
*
* Parameters :
*  void
*
* Return:
*  int
*
*******************************************************************************/
void rtc(void)
{
    char date_string[STRING_BUFFER_SIZE];
    struct tm date_time;

    init_rtc();
    
    // to read and print
    for(;;) {
        read_rtc(&date_time);
        convert_rtc_to_str(&date_time, date_string);
        Cy_SysLib_Delay(1000);
    }
}

/*******************************************************************************
* Function Name: set_dst_feature
********************************************************************************
* Summary:
*  This functions takes the user input ,sets the dst start/end date and time,
*  and then enables the DST feature.
*
* Parameter:
*  uint32_t timeout_ms : Maximum allowed time (in milliseconds) for the
*  function
*
* Return:
*  void
*******************************************************************************/

static void set_dst_feature(uint32_t timeout_ms)
{
    cy_rslt_t rslt;
    uint8_t dst_cmd;
    char dst_start_buffer[STRING_BUFFER_SIZE] = {0};
    char dst_end_buffer[STRING_BUFFER_SIZE] = {0};
    uint32_t space_count = 0;

    /* Variables used to store DST start and end time information */
    cyhal_rtc_dst_t dst_start_time, dst_end_time;

    /* Variables used to store date and time information */
    int mday = 0, month = 0, year = 0, sec = 0, min = 0, hour = 0;
    uint8_t fmt = 0;
    if (DST_ENABLED_FLAG == dst_data_flag)
    {
        if (cyhal_rtc_is_dst(&rtc_obj))
        {
            printf("\rCurrent DST Status :: Active\r\n\n");
        }
        else
        {
            printf("\rCurrent DST Status :: Inactive\r\n\n");
        }
    }
    else
    {
        printf("\rCurrent DST Status :: Disabled\r\n\n");
    }

    /* Display available commands */
    printf("Available DST commands \r\n");
    printf("1 : Enable DST feature\r\n");
    printf("2 : Disable DST feature\r\n");
    printf("3 : Quit DST Configuration\r\n\n");

    rslt = cyhal_uart_getc(&cy_retarget_io_uart_obj, &dst_cmd, timeout_ms);

    if (rslt != CY_RSLT_ERR_CSP_UART_GETC_TIMEOUT)
    {
        if (RTC_CMD_ENABLE_DST == dst_cmd)
        {
            /* Get DST start time information */
            printf("Enter DST format \r\n");
            printf("1 : Fixed DST format\r\n");
            printf("2 : Relative DST format\r\n\n");

            rslt = cyhal_uart_getc(&cy_retarget_io_uart_obj, &fmt, timeout_ms);
            if (rslt != CY_RSLT_ERR_CSP_UART_GETC_TIMEOUT)
            {
            printf("Enter DST start time in \"HH MM SS dd mm yyyy\" format\r\n");
                rslt = fetch_time_data(dst_start_buffer, timeout_ms,
                                                        &space_count);
                if (rslt != CY_RSLT_ERR_CSP_UART_GETC_TIMEOUT)
                {
                    if (space_count != MIN_SPACE_KEY_COUNT)
                    {
                        printf("\rInvalid values! Please enter "
                        "the values in specified format\r\n");
                    }
                    else
                    {
                        sscanf(dst_start_buffer, "%d %d %d %d %d %d",
                               &hour, &min, &sec,
                               &mday, &month, &year);

                    if((validate_date_time(sec, min, hour, mday, month, year))&&
                    ((fmt == FIXED_DST_FORMAT) || (fmt == RELATIVE_DST_FORMAT)))
                    {
                        dst_start_time.format =
                        (fmt == FIXED_DST_FORMAT) ? CYHAL_RTC_DST_FIXED :
                                                        CYHAL_RTC_DST_RELATIVE;
                        dst_start_time.hour = hour;
                        dst_start_time.month = month;
                        dst_start_time.dayOfWeek =
                        (fmt == FIXED_DST_FORMAT) ? 1 :
                                            get_day_of_week(mday, month, year);
                        dst_start_time.dayOfMonth =
                        (fmt == FIXED_DST_FORMAT) ? mday : 1;
                        dst_start_time.weekOfMonth =
                        (fmt == FIXED_DST_FORMAT) ? 1 :
                                        get_week_of_month(mday, month, year);
                        /* Update flag value to indicate that a
                            valid DST start time information has been received*/
                        dst_data_flag = DST_VALID_START_TIME_FLAG;
                    }
                    else
                    {
                        printf("\rInvalid values! Please enter the values"
                                   " in specified format\r\n");
                    }
                    }
                }
                else
                {
                    printf("\rTimeout \r\n");
                }

                if (DST_VALID_START_TIME_FLAG == dst_data_flag)
                {
                    /* Get DST end time information,
                    iff a valid DST start time information is received */
                    printf("Enter DST end time "
                    " in \"HH MM SS dd mm yyyy\" format\r\n");
                    rslt = fetch_time_data(dst_end_buffer, timeout_ms,
                                            &space_count);
                    if (rslt != CY_RSLT_ERR_CSP_UART_GETC_TIMEOUT)
                    {
                        if (space_count != MIN_SPACE_KEY_COUNT)
                        {
                            printf("\rInvalid values! Please"
                            "enter the values in specified format\r\n");
                        }
                        else
                        {
                            sscanf(dst_end_buffer, "%d %d %d %d %d %d",
                                   &hour, &min, &sec,
                                   &mday, &month, &year);

                            if((validate_date_time(sec,min,hour,mday,month,year))&&
                                ((fmt == FIXED_DST_FORMAT) ||
                                (fmt == RELATIVE_DST_FORMAT)))
                            {
                                dst_end_time.format = (fmt == FIXED_DST_FORMAT)?
                                 CYHAL_RTC_DST_FIXED : CYHAL_RTC_DST_RELATIVE;
                                dst_end_time.hour = hour;
                                dst_end_time.month = month;
                                dst_end_time.dayOfWeek =
                                (fmt == FIXED_DST_FORMAT) ? 1 :
                                get_day_of_week(mday, month, year);
                                dst_end_time.dayOfMonth =
                                (fmt == FIXED_DST_FORMAT) ? mday : 1;
                                dst_end_time.weekOfMonth =
                                (fmt == FIXED_DST_FORMAT) ? 1 :
                                get_week_of_month(mday, month, year);
                                /* Update flag value to indicate that a valid
                                 DST end time information has been recieved*/
                                dst_data_flag = DST_VALID_END_TIME_FLAG;
                            }
                            else
                            {
                                printf("\rInvalid values! Please enter the "
                                       " values in specified format\r\n");
                            }
                        }
                    }
                    else
                    {
                        printf("\rTimeout \r\n");
                    }
                }

                if (DST_VALID_END_TIME_FLAG == dst_data_flag)
                {
                    rslt = cyhal_rtc_set_dst(&rtc_obj, &dst_start_time,
                                                        &dst_end_time);
                    if (CY_RSLT_SUCCESS == rslt)
                    {
                        dst_data_flag = DST_ENABLED_FLAG;
                        printf("\rDST time updated\r\n\n");
                    }
                    else
                    {
                        handle_error();
                    }
                }
            }
            else
            {
                printf("\rTimeout \r\n");
            }
        }
        else if (RTC_CMD_DISABLE_DST == dst_cmd)
        {
            dst_end_time.format = CYHAL_RTC_DST_FIXED;
            dst_end_time.hour = 0;
            dst_end_time.month = 1;
            dst_end_time.dayOfWeek = 1;
            dst_end_time.dayOfMonth = 1;
            dst_end_time.weekOfMonth = 1;
            dst_start_time = dst_end_time;
            rslt = cyhal_rtc_set_dst(&rtc_obj, &dst_start_time, &dst_end_time);
            if (CY_RSLT_SUCCESS == rslt)
            {
                dst_data_flag = DST_DISABLED_FLAG;
                printf("\rDST feature disabled\r\n\n");
            }
            else
            {
                handle_error();
            }
        }
        else if (RTC_CMD_QUIT_CONFIG_DST == dst_cmd)
        {
            printf("\rExit from DST Configuration \r\n\n");
        }
    }
    else
    {
        printf("\rTimeout \r\n");
    }
}

/*******************************************************************************
* Function Name: set_time
********************************************************************************
* Summary:
*  This functions takes the user input and sets the new date and time.
*
* Parameter:
*  uint32_t timeout_ms : Maximum allowed time (in milliseconds) for the
*  function
*
* Return :
*  void
*******************************************************************************/
static void set_time(uint32_t timeout_ms)
{
    cy_rslt_t rslt;
    char buffer[STRING_BUFFER_SIZE];
    uint32_t space_count = 0;

    /* Variables used to store date and time information */
    int mday, month, year, sec, min, hour;
    struct tm new_time = {0};


    // printf("\rEnter time in \"HH MM SS dd mm yyyy\" format \r\n");
    rslt = fetch_time_data(buffer, timeout_ms, &space_count);
    
    if (rslt != CY_RSLT_ERR_CSP_UART_GETC_TIMEOUT)
    {
        if (space_count != MIN_SPACE_KEY_COUNT)
        {
            LOG_WARN("\rInvalid values! Please enter the"
                    "values in specified format\n");
        }
        else
        {
            sscanf(buffer, "%d %d %d %d %d %d",
                   &hour, &min, &sec,
                   &mday, &month, &year);

            if (validate_date_time(sec, min, hour, mday, month, year))
            {
                new_time.tm_sec = sec;
                new_time.tm_min = min;
                new_time.tm_hour = hour;
                new_time.tm_mday = mday;
                new_time.tm_mon = month - 1;
                new_time.tm_year = year - TM_YEAR_BASE;
                new_time.tm_wday = get_day_of_week(mday, month, year);

                rslt = cyhal_rtc_write(&rtc_obj, &new_time);
                if (CY_RSLT_SUCCESS == rslt)
                {
                    LOG_DEBUG("\rRTC time updated\r\n\n");
                }
                else
                {
                    LOG_ERR("Writing RTC time failed\n");
                    handle_error();
                }
            }
            else
            {
                LOG_WARN("\rInvalid values! Please enter the values in specified"
                       " format\r\n");
            }
        }
    }
    else
    {
        LOG_ERR("\rTimeout \r\n");
    }
}
/*******************************************************************************
* Function Name: fetch_time_data
********************************************************************************
* Summary:
*  Function fetches data entered by the user through UART and stores it in the
*  buffer which is passed through parameters. The function also counts number of
*  spaces in the recieved data and stores in the variable, whose address are
*  passsed as parameter.
*
* Parameter:
*  char* buffer        : Buffer to store the fetched data
*  uint32_t timeout_ms : Maximum allowed time (in milliseconds) for the function
*  uint32_t* space_count : The number of spaces present in the fetched data.
*
* Return:
*  Returns the status of the getc request
*
*******************************************************************************/
static cy_rslt_t fetch_time_data(char *buffer, uint32_t timeout_ms,
                                    uint32_t *space_count)
{
    cy_rslt_t rslt;
    uint32_t index = 0;
    uint8_t ch;
    *space_count = 0;
    
    while (index < STRING_BUFFER_SIZE)
    {
        if (timeout_ms <= UART_TIMEOUT_MS)
        {
            rslt = CY_RSLT_ERR_CSP_UART_GETC_TIMEOUT;
            break;
        }

        rslt = cyhal_uart_getc(&cy_retarget_io_uart_obj, &ch, 0);

        if (rslt != CY_RSLT_ERR_CSP_UART_GETC_TIMEOUT)
        {
            if (ch == '\n' || ch == '\r')
            {
                break;
            }
            else if (ch == ' ')
            {
                (*space_count)++;
            }

            buffer[index] = ch;
            rslt = cyhal_uart_putc(&cy_retarget_io_uart_obj, ch);
            index++;
        }

        timeout_ms -= UART_TIMEOUT_MS;
    }
    LOG_INFO_NOFMT("\n\r");
    return rslt;
}

/*******************************************************************************
* Function Name: get_day_of_week
********************************************************************************
* Summary:
*  Returns a day of the week for a year, month, and day of month that are passed
*  through parameters. Zeller's congruence is used to calculate the day of
*  the week. See https://en.wikipedia.org/wiki/Zeller%27s_congruence for more
*  details.
*
*  Note: In this algorithm January and February are counted as months 13 and 14
*  of the previous year.
*
* Parameter:
*  int day          : The day of the month, Valid range 1..31.
*  int month        : The month of the year
*  int year         : The year value. Valid range non-zero value.
*
* Return:
*  Returns a day of the week (0 = Sunday, 1 = Monday, ., 6 = Saturday)
*
*******************************************************************************/
static int get_day_of_week(int day, int month, int year)
{
    int ret;
    int k = 0;
    int j = 0;
    if (month < CY_RTC_MARCH)
    {
        month += CY_RTC_MONTHS_PER_YEAR;
        year--;
    }

    k = (year % 100);
    j = (year / 100);
    ret=(day+(13*(month+1)/5)+k+(k/4)+(j/4)+(5*j))%DAYS_PER_WEEK;
    ret = ((ret + 6) % DAYS_PER_WEEK);
    return ret;
}

/*******************************************************************************
* Function Name: get_week_of_month
********************************************************************************
* Summary:
*  Returns week number of the month for a year and a month that are passed
*  through parameters.
*
* Parameter:
*  int day          : The day of the month, Valid range 1..31.
*  int month        : The month of the year
*  int year         : The year value. Valid range non-zero value.
*
* Return:
*  Returns a week of the month (0 to 5)
*
*******************************************************************************/
static int get_week_of_month(int day, int month, int year)
{
    int count = 0, day_of_week = 0, weekend_day = 0;

    day_of_week = get_day_of_week(1, month, year);
    weekend_day = 7 - day_of_week;
    while (day > weekend_day)
    {
        count++;
        weekend_day += 7;
    }

    return count;
}

/*******************************************************************************
* Function Name: validate_date_time
********************************************************************************
* Summary:
*  This function validates date and time value.
*
* Parameters:
*  uint32_t sec     : The second valid range is [0-59].
*  uint32_t min     : The minute valid range is [0-59].
*  uint32_t hour    : The hour valid range is [0-23].
*  uint32_t date    : The date valid range is [1-31], if the month of February
*                     is selected as the Month parameter, then the valid range
*                     is [0-29].
*  uint32_t month   : The month valid range is [1-12].
*  uint32_t year    : The year valid range is [> 0].
*
* Return:
*  false - invalid ; true - valid
*
*******************************************************************************/
static bool validate_date_time(int sec, int min, int hour, int mday,
                                    int month, int year)
{
    static const uint8_t days_in_month_table[MONTHS_PER_YEAR] =
        {
            DAYS_IN_JANUARY,
            DAYS_IN_FEBRUARY,
            DAYS_IN_MARCH,
            DAYS_IN_APRIL,
            DAYS_IN_MAY,
            DAYS_IN_JUNE,
            DAYS_IN_JULY,
            DAYS_IN_AUGUST,
            DAYS_IN_SEPTEMBER,
            DAYS_IN_OCTOBER,
            DAYS_IN_NOVEMBER,
            DAYS_IN_DECEMBER,
        };

    uint8_t days_in_month;

    bool rslt = IS_SEC_VALID(sec) & IS_MIN_VALID(min) &
                IS_HOUR_VALID(hour) & IS_MONTH_VALID(month) &
                IS_YEAR_VALID(year);

    if (rslt)
    {
        days_in_month = days_in_month_table[month - 1];

        if (IS_LEAP_YEAR(year) && (month == 2))
        {
            days_in_month++;
        }

        rslt &= (mday > 0U) && (mday <= days_in_month);
    }

    return rslt;
}

/* [] END OF FILE */
