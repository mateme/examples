#include "process.h"
#include "recipes.h"
#include "pid.h"
#include "stm32f10x.h"
#include "hdr_exti.h"
#include "hdr_rtc.h"
#include "timer.h"
#include "sensors.h"
#include "printf.h"
#include "buzzer.h"
#include "backupreg.h"
#include "sysled.h"
#include "rpm.h"
#include "Serialcomm.h"

#define STIRRER_FAST    2
#define STIRRER_SLOW    1
#define STIRRER_DISABLE 0

typedef enum _SysState SysState;

SystemRecipe   Recipe;
SystemTimes    Process;
SystemSettings Settings;

SysState     SystemState = SYS_FIRST_RUN;

xTaskHandle xProcessTaskHandle;

unsigned short CurrentTemperature = 250;
unsigned short SetTemperature = 400;

void Heater(int percent)
{
    SetTimer4PWM(percent);

    if(SerialCommDevice_Heater_bb)
    {
        unsigned char data[4];

        data[0] = SERIALCOMM_DEVICE_HEATER;
        data[1] = percent;
        data[2] = 0;
        data[3] = 0;

        SerialCommunicationSendCommand(data);
    }
}

void Stirrer(int state)
{
    switch(state)
    {
    case STIRRER_SLOW:
        SetTimer3PWMCH2(Settings.StirrerLowLevel);
        break;

    case STIRRER_FAST:
        SetTimer3PWMCH2(Settings.StirrerHighLevel);
        break;

    default:
        SetTimer3PWMCH2(0);
        break;
    }

    if(SerialCommDevice_Stirrer_bb)
    {
        unsigned char data[4];

        data[0] = SERIALCOMM_DEVICE_STIRRER;
        data[1] = state;
        data[2] = 0;
        data[3] = 0;

        SerialCommunicationSendCommand(data);
    }
}

static void MashingProcedure(void)
{
    static unsigned short BoilingStartTime = 0;

    //Save current state in backup registers
    BKP_SYSTEM_STATE       = SystemState;
    BKP_CURRENT_PHASE_TIME = Process.CurrentPhaseTime;
    BKP_TOTAL_PHASE_TIME   = Process.TotalPhaseTime;
    BKP_CURRENT_TIME       = Process.CurrentTime;
    BKP_PHASE              = Process.Phase;
    BKP_SET_TEMPERATURE    = SetTemperature;

    SaveRTCCounterToBackup();

    switch(SystemState)
    {
    case SYS_DISABLE:
        Heater(0);
        Stirrer(STIRRER_DISABLE);

        Process.CurrentPhaseTime = 0;
        Process.CurrentTime      = 0;
        Process.AdditionalTime   = 0;
        Process.Phase            = 0;
        Process.TotalPhaseTime   = 0;
        Process.RPM              = 0;
        Process.Power            = 0;
		Process.Stirrer          = 0;
        BoilingStartTime         = 0;

        BuzzerSound(BUZ_NONE);
        SystemLED(SYSLED_OFF);
        vTaskSuspend(xProcessTaskHandle);
        break;

    case SYS_FIRST_RUN:
        SystemState = SYS_DISABLE;

        Process.CurrentPhaseTime = 0;
        Process.CurrentTime      = 0;
        Process.AdditionalTime   = 0;
        Process.Phase            = 0;
        Process.TotalPhaseTime   = 0;
        Process.RPM              = 0;
        Process.Power            = 0;
		Process.Stirrer          = 0;
        BoilingStartTime         = 0;

        break;

    case SYS_PREHEATING:
        ++Process.CurrentPhaseTime;
        ++Process.CurrentTime;

        CurrentTemperature = GetProcessTemperature();

        Process.Power = 100;

        //Enable stirrer time to time
        if((Process.CurrentPhaseTime%60) >= 55)
            Stirrer(STIRRER_SLOW);
        else
            Stirrer(STIRRER_DISABLE);

        if(CurrentTemperature >= (Recipe.Phases[Process.Phase].Temperature-Settings.PreheatingDeltaTemperature))
        {
            SystemState = SYS_MASH_IN;
            SetTemperature = Recipe.Phases[Process.Phase].Temperature;
            Stirrer(STIRRER_SLOW);
            BuzzerSound(BUZ_INFO);
            SystemLED(SYSLED_USER_REQ);
        }

        GUIRefreshRequest();
        break;

    case SYS_MASH_IN:
        ++Process.CurrentPhaseTime;
        ++Process.CurrentTime;

        CurrentTemperature = GetProcessTemperature();
        Process.Power = PIDRegulator(SetTemperature, CurrentTemperature);

        if(CurrentTemperature < (SetTemperature-Settings.MashinDeltaTemperature))
        {
            ++Process.Phase;

            Process.CurrentPhaseTime = 0;
            Process.TotalPhaseTime = Recipe.Phases[Process.Phase].Time;

            SystemState = SYS_MASHING;
            BuzzerSound(BUZ_NONE);
            SystemLED(SYSLED_OFF);
        }

        GUIRefreshRequest();
        break;

    case SYS_MASHING:
        ++Process.CurrentPhaseTime;
        ++Process.CurrentTime;

        CurrentTemperature = GetProcessTemperature();
        Process.RPM = GetRPMValue();

        //If current phase has time equal to zero it's mean that phase time depends on how fast mash reach final temperature
        if(Recipe.Phases[Process.Phase].Time != 0)
        {
            //Quite bit different situation in temperature if this value is zero it's mean that driver create temperature ramp
            if(Recipe.Phases[Process.Phase].Temperature != 0)
            {
                SetTemperature = Recipe.Phases[Process.Phase].Temperature;
                Stirrer(STIRRER_SLOW);
            }
            else
            {
                SetTemperature = (unsigned short)((int)(Recipe.Phases[Process.Phase+1].Temperature - Recipe.Phases[Process.Phase-1].Temperature)*(int)Process.CurrentPhaseTime/(int)Recipe.Phases[Process.Phase].Time);
                Stirrer(STIRRER_FAST);
            }

            if(Process.CurrentPhaseTime >= (Recipe.Phases[Process.Phase].Time+Process.AdditionalTime))
            {
                ++Process.Phase;
                Process.AdditionalTime   = 0;

                if(Process.Phase < Recipe.NumOfPhases)
                {
                    Process.CurrentPhaseTime = 0;
                    Process.TotalPhaseTime = Recipe.Phases[Process.Phase].Time;
                    SetTemperature = Recipe.Phases[Process.Phase].Temperature;
                }
                else
                {
                    SystemState = SYS_WORT_OUT;
                    break;
                }
            }

            //Phase ending heating booster
            if((Process.CurrentPhaseTime >= (Recipe.Phases[Process.Phase].Time-Settings.PhaseEndBoosterTime)) && Process.Phase < (Recipe.NumOfPhases-1))
                Process.Power = 100;
            else
                Process.Power = PIDRegulator(SetTemperature, CurrentTemperature);
        }
        else
        {
            Process.Power = 100;
            Stirrer(STIRRER_FAST);

            if(CurrentTemperature >= (Recipe.Phases[Process.Phase].Temperature-Settings.PhaseEndDeltaTemperature))
            {
                ++Process.Phase;
                Process.CurrentPhaseTime = 0;
                Process.TotalPhaseTime = Recipe.Phases[Process.Phase].Time;
                SetTemperature = Recipe.Phases[Process.Phase].Temperature;
            }
        }

        GUIRefreshRequest();
        BKP_MAGIC_KEY = 0xBABE;
        break;

    case SYS_WORT_OUT:
        Process.Power = 0;
        Stirrer(STIRRER_SLOW);
        GUIRefreshRequest();
        BKP_MAGIC_KEY = 0x0000;
    break;

    case SYS_PREHEATING_BOILING:
        ++Process.CurrentTime;

        CurrentTemperature = GetProcessTemperature();

        if(CurrentTemperature >= Settings.BoilingStartTemperature)
            ++BoilingStartTime;
        else
            BoilingStartTime = 0;

        if(BoilingStartTime > 30)
        {
            SystemState = SYS_BOILING;
            Process.CurrentTime      = 0;
            Process.CurrentPhaseTime = 0;
            Process.Phase            = 0;
        }

        Process.Power = 100;
        GUIRefreshRequest();
        BKP_MAGIC_KEY = 0xBABE;
        break;

    case SYS_BOILING:
        ++Process.CurrentPhaseTime;
        ++Process.CurrentTime;

        CurrentTemperature = GetProcessTemperature();

        if(Process.CurrentTime > Recipe.BoilingTime)
        {
            SystemState = SYS_COMPLETE;
            break;
        }

        //Manage hops portions
        if((Recipe.BoilingTime-Process.CurrentTime) <= ((unsigned short)Recipe.BoilingPhasesTime[Process.Phase]*60))
        {
            ++Process.Phase;
            Process.CurrentPhaseTime = 0;

            //TODO:Add here functions to manage hop dispenser
            if(SerialCommDevice_HopDispenser_bb)
            {

            }
            else
            {
                GUIShowHopRequest(&Recipe, Process.Phase);
            }
        }

        if(CurrentTemperature > Settings.BoilingTemperatureTriggerPoint)
            Process.Power = Settings.BoilingPowerTriggerPoint;
        else
            Process.Power = 100;

        GUIRefreshRequest();
        BKP_MAGIC_KEY = 0xBABE;
        break;

    case SYS_COMPLETE:
        Process.Power = 0;
        Stirrer(STIRRER_DISABLE);
        BKP_MAGIC_KEY = 0x0000;
        SystemState = SYS_DISABLE;
        break;

    case SYS_THERMOMETER:
		++Process.CurrentTime;
        CurrentTemperature = GetProcessTemperature();
        Process.RPM = GetRPMValue();
		Stirrer(Process.Stirrer);

        GUIRefreshRequest();
        BKP_MAGIC_KEY = 0x0000;
        break;

    case SYS_SELFTUNING:
		++Process.CurrentTime;

        CurrentTemperature = GetProcessTemperature();
        Process.RPM  = GetRPMValue();
        Process.Power = 100;
		Stirrer(STIRRER_FAST);

        GUIRefreshRequest();
        BKP_MAGIC_KEY = 0x0000;
    break;
	}
}

static void MashingWatchdog(void)
{
    unsigned char powermeas, hlimit, llimit;

    switch(SystemState)
    {
    case SYS_MASHING:
        if(Recipe.Phases[Process.Phase].Time != 0)
        {
            if(CurrentTemperature <= (SetTemperature-Settings.TemperatureAlarmError))
            {
                BuzzerSound(BUZ_FATAL_ERROR);
                Process.Power = 100;
                Stirrer(STIRRER_SLOW);
                SystemLED(SYSLED_FATALERROR);
            }
            else if(CurrentTemperature <= (SetTemperature-Settings.TemperatureAlarmWarning))
            {
                BuzzerSound(BUZ_WARNING);
                SystemLED(SYSLED_WARNING);
            }
            else if(CurrentTemperature >= (SetTemperature+Settings.TemperatureAlarmError))
            {
                BuzzerSound(BUZ_FATAL_ERROR);
                Process.Power = 0;
                Stirrer(STIRRER_FAST);
                SystemLED(SYSLED_FATALERROR);
            }
            else if(CurrentTemperature >= (SetTemperature+Settings.TemperatureAlarmWarning))
            {
                BuzzerSound(BUZ_WARNING);
                SystemLED(SYSLED_WARNING);
            }
            else
            {
                BuzzerSound(BUZ_NONE);
                SystemLED(SYSLED_OFF);
            }
        }

        if(Process.RPM < 5)
        {
            BuzzerSound(BUZ_FATAL_ERROR);
            SystemLED(SYSLED_FATALERROR);
        }

        //TODO: Check this feature
        /*powermeas = GetTimer2CH1PWMDuty();

        hlimit = Process.Power+10;
        if(hlimit > 100)
            hlimit = 100;

        if(Process.Power > 10)
            llimit = Process.Power-10;
        else
            llimit = 0;

        if(powermeas > llimit && powermeas < hlimit)
        {
            BuzzerSound(BUZ_FATAL_ERROR);
            SystemLED(SYSLED_FATALERROR);
        }*/

        break;

    default:

        break;
    }
}

void RunProcess(int process)
{
    SysState state;

    state = process;

    if(state == SYS_PREHEATING && SystemState == SYS_MASHING)
        return;

    if(SystemState == SYS_DISABLE)
    {
        SystemState = state;
        vTaskResume(xProcessTaskHandle);
    }
    else
    {
        SystemState = state;
    }
}

int GetProcessState(void)
{
    return SystemState;
}

void LoadLastRecipe(void)
{
    GetRecipe(&Recipe, BKP_RECIPE_ADDRESS);
}

void vProcessTask(void * pvArg)
{
    pvArg = pvArg;

    SystemSettingsLoad(&Settings);
    LoadSensorsSettings(Settings.SensorsSettings);

    SensorsInit();
    PIDRegulatorInit(Settings.PIDSettings[0], Settings.PIDSettings[1], Settings.PIDSettings[2]);

    //Check that previous process was interrupted
    if(BKP_MAGIC_KEY == 0xBABE)
    {
        unsigned int time;

        SystemState              = BKP_SYSTEM_STATE;
        Process.CurrentPhaseTime = BKP_CURRENT_PHASE_TIME;
        Process.TotalPhaseTime   = BKP_TOTAL_PHASE_TIME;
        Process.CurrentTime      = BKP_CURRENT_TIME;
        Process.Phase            = BKP_PHASE;
        SetTemperature           = BKP_SET_TEMPERATURE;

        time = LoadRTCCounterFromBackup();

        if((time+(unsigned int)Process.CurrentPhaseTime < RECIPE_MIN(1000)) && (time+(unsigned int)Process.CurrentTime < RECIPE_MIN(1000)))
        {
            Process.CurrentPhaseTime += time;
            Process.CurrentTime      += time;
        }

        //TODO: Check and resolve possibility if time under power off is higher than rest time in current phase

        //Wait for other tasks
        vTaskDelay(500);
    }

    for(;;)
    {
        if(xSemaphoreTake(xProcessSemaphore, PROCESS_SEMPHR_WAITTIME) == pdTRUE)
        {
            MashingProcedure();
            MashingWatchdog();

            Heater(Process.Power);
        }
    }
}

void RTC_IRQHandler(void)
{
    portBASE_TYPE xTaskWoken = pdFALSE;

    if(RTC_CRL_SECF_bb)
    {
        RTC_CRL_SECF_bb = 0;

        xSemaphoreGiveFromISR(xProcessSemaphore, &xTaskWoken);

        if(xTaskWoken == pdTRUE)
            vPortYieldFromISR();
    }
}

void EXTI4_IRQHandler(void)
{
    if(EXTI_PR_PR4_bb)
    {

        EXTI_PR_PR4_bb  = 1;
    }
}
