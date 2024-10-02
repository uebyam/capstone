#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "FreeRTOS.h"
#include "task.h"
#include "cybsp.h"
#include "cyhal.h"
#include "cy_pdl.h"
#pragma clang diagnostic pop

static inline uint32_t msToTicks(uint32_t ms) {
    uint64_t val = (CY_SYSCLK_WCO_FREQ*(uint64_t)ms/1000);
    val = (val>UINT32_MAX)?UINT32_MAX:val;
    return (uint32_t)val;
}


static inline uint32_t ticksToMs(uint32_t ticks) {
    return (ticks * 1000) / CY_SYSCLK_WCO_FREQ;
}


void vApplicationSleep( TickType_t xExpectedIdleTime ) {
    static cyhal_lptimer_t myTimer={0};
    unsigned long ulLowPowerTimeBeforeSleep, ulLowPowerTimeAfterSleep;

    if(myTimer.base == 0)
        cyhal_lptimer_init(&myTimer);

    Cy_SysTick_Disable();
    uint8_t interruptState = Cy_SysLib_EnterCriticalSection();

    eSleepModeStatus eSleepStatus = eTaskConfirmSleepModeStatus();
    cyhal_lptimer_reload(&myTimer);
    if( eSleepStatus != eAbortSleep ) {

        if( eSleepStatus != eNoTasksWaitingTimeout ) {
            cyhal_lptimer_set_delay    (&myTimer,msToTicks(xExpectedIdleTime));
            cyhal_lptimer_enable_event (&myTimer, CYHAL_LPTIMER_COMPARE_MATCH, 7, true);
        }

        ulLowPowerTimeBeforeSleep = cyhal_lptimer_read(&myTimer);

#if CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP
        cyhal_syspm_deepsleep();
#elif CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_SLEEP
        cyhal_syspm_sleep();
#else
        goto exitPoint;
#endif
        ulLowPowerTimeAfterSleep = cyhal_lptimer_read(&myTimer);
        vTaskStepTick( ticksToMs(ulLowPowerTimeAfterSleep - ulLowPowerTimeBeforeSleep));
    }

    cyhal_lptimer_enable_event (&myTimer, CYHAL_LPTIMER_COMPARE_MATCH, 4, false);

#if !(CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP || CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_SLEEP)
exitPoint:
#endif

    Cy_SysLib_ExitCriticalSection(interruptState);
    Cy_SysTick_Enable();
}
