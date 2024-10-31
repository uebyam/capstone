#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "lp.h"

#include "cyhal.h"
#include "FreeRTOS.h"
#include "cyabs_rtos.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

char insomniac = 0;

#define pdTICKS_TO_MS(xTicks)    ( ( ( TickType_t ) ( xTicks ) * 1000u ) / configTICK_RATE_HZ )

uint32_t cyabs_rtos_get_deepsleep_latency(void);

void vApplicationSleep(TickType_t xExpectedIdleTime)
{
    #if (defined(CY_CFG_PWR_MODE_DEEPSLEEP) && \
    (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)) || \
    (defined(CY_CFG_PWR_MODE_DEEPSLEEP_RAM) && \
    (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP_RAM))
    #define DEEPSLEEP_ENABLE
    #endif
    static cyhal_lptimer_t timer;
    uint32_t               actual_sleep_ms = 0;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if (NULL == cyabs_rtos_get_lptimer())
    {
        result = cyhal_lptimer_init(&timer);
        if (result == CY_RSLT_SUCCESS)
        {
            cyabs_rtos_set_lptimer(&timer);
        }
        else
        {
            CY_ASSERT(false);
        }
    }

    if (NULL != cyabs_rtos_get_lptimer())
    {
        /* Disable interrupts so that nothing can change the status of the RTOS while
         * we try to go to sleep or deep-sleep.
         */
        uint32_t         status       = cyhal_system_critical_section_enter();
        eSleepModeStatus sleep_status = eTaskConfirmSleepModeStatus();

        if (sleep_status != eAbortSleep)
        {
            // By default, the device will deep-sleep in the idle task unless if the device
            // configurator overrides the behaviour to sleep in the System->Power->RTOS->System
            // Idle Power Mode setting.
            #if defined (CY_CFG_PWR_SYS_IDLE_MODE)
            uint32_t sleep_ms = pdTICKS_TO_MS(xExpectedIdleTime);
            #if defined DEEPSLEEP_ENABLE
            bool deep_sleep = true;
            // If the system needs to operate in active mode the tickless mode should not be used in
            // FreeRTOS
            CY_ASSERT(CY_CFG_PWR_SYS_IDLE_MODE != CY_CFG_PWR_MODE_ACTIVE);
            deep_sleep =
                #if defined(CY_CFG_PWR_MODE_DEEPSLEEP_RAM)
                (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP_RAM) ||
                #endif
                (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP);
            if (deep_sleep)
            {
                // Adjust the deep-sleep time by the sleep/wake latency if set.
                #if defined(CY_CFG_PWR_DEEPSLEEP_LATENCY) || \
                defined(CY_CFG_PWR_DEEPSLEEP_RAM_LATENCY)
                uint32_t deep_sleep_latency = cyabs_rtos_get_deepsleep_latency();
                if (sleep_ms > deep_sleep_latency)
                {
                    result = cyhal_syspm_tickless_deepsleep(cyabs_rtos_get_lptimer(),
                                                            (sleep_ms - deep_sleep_latency),
                                                            &actual_sleep_ms);
                }
                else
                {
                    result = CY_RTOS_TIMEOUT;
                }
                #else \
                // defined(CY_CFG_PWR_DEEPSLEEP_LATENCY) ||
                // defined(CY_CFG_PWR_DEEPSLEEP_RAM_LATENCY)
                result = cyhal_syspm_tickless_deepsleep(_lptimer, sleep_ms, &actual_sleep_ms);
                #endif \
                // defined(CY_CFG_PWR_DEEPSLEEP_LATENCY) ||
                // defined(CY_CFG_PWR_DEEPSLEEP_RAM_LATENCY)
                //maintain compatibility with older HAL versions that didn't define this error
                #ifdef CYHAL_SYSPM_RSLT_DEEPSLEEP_LOCKED
                //Deepsleep has been locked, continuing into normal sleep
                if (result == CYHAL_SYSPM_RSLT_DEEPSLEEP_LOCKED)
                {
                    deep_sleep = false;
                }
                #endif
            }
            if (!deep_sleep)
            {
            #endif // if defined DEEPSLEEP_ENABLE
            uint32_t sleep_latency =
                #if defined (CY_CFG_PWR_SLEEP_LATENCY)
                CY_CFG_PWR_SLEEP_LATENCY +
                #endif
                0;
            if (sleep_ms > sleep_latency)
            {
                result = cyhal_syspm_tickless_sleep(cyabs_rtos_get_lptimer(), (sleep_ms - sleep_latency),
                                                    &actual_sleep_ms);
            }
            else
            {
                result = CY_RTOS_TIMEOUT;
            }
            #if defined DEEPSLEEP_ENABLE
        }
            #endif
            #else // if defined (CY_CFG_PWR_SYS_IDLE_MODE)
            CY_UNUSED_PARAMETER(xExpectedIdleTime);
            #endif // if defined (CY_CFG_PWR_SYS_IDLE_MODE)
            if (result == CY_RSLT_SUCCESS)
            {
                // If you hit this assert, the latency time (CY_CFG_PWR_DEEPSLEEP_LATENCY) should
                // be increased. This can be set though the Device Configurator, or by manually
                // defining the variable in cybsp.h for the TARGET platform.
                CY_ASSERT(actual_sleep_ms <= pdTICKS_TO_MS(xExpectedIdleTime));
                vTaskStepTick(convert_ms_to_ticks(actual_sleep_ms));
            }
        }

        cyhal_system_critical_section_exit(status);
    }
}
