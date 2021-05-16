#ifndef PROCESS_H_INCLUDED
	#define PROCESS_H_INCLUDED

	#include "FreeRTOS.h"
	#include "task.h"
	#include "queue.h"
	#include "recipes.h"
	#include "settings.h"
	#include "semphr.h"

	#include "hdr_bitband.h"

	#define PROCESS_TASK_STACK_SIZE	    ( 1*configMINIMAL_STACK_SIZE)
	#define PROCESS_TASK_PRIORITY		( tskIDLE_PRIORITY + 2)

	#define PROCESS_SEMPHR_WAITTIME     1500

    enum _SysState {SYS_DISABLE, SYS_FIRST_RUN, SYS_PREHEATING, SYS_MASH_IN, SYS_MASHING, SYS_WORT_OUT, SYS_COMPLETE, SYS_PREHEATING_BOILING, SYS_BOILING, SYS_THERMOMETER,  SYS_SELFTUNING};

    typedef struct{
        unsigned short CurrentPhaseTime;
        unsigned short TotalPhaseTime;
        unsigned short CurrentTime;
        unsigned short AdditionalTime;
        unsigned char Phase;
        unsigned short RPM;
        unsigned char Power;
		unsigned char Stirrer;
    }SystemTimes;

    SystemRecipe   Recipe;
    SystemTimes    Process;
    SystemSettings Settings;

    unsigned short CurrentTemperature;
    unsigned short SetTemperature;

    xSemaphoreHandle xProcessSemaphore;

    //Provide your own implementation in GUI section
    void GUIRefreshRequest(void);
    void GUIShowHopRequest(SystemRecipe *processrecipe, int phase);
    void GUIShowMaltRequest(SystemRecipe *processrecipe, int phase);

    xTaskHandle xProcessTaskHandle;
	void vProcessTask(void *pvArg);
	void RunProcess(int process);
    int GetProcessState(void);
    void LoadLastRecipe(void);
#endif
