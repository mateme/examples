#ifndef PLAYER_H_INCLUDED
    #define PLAYER_H_INCLUDED

    #include "FreeRTOS.h"
    #include "task.h"

    #define PLAYER_TASK_STACK_SIZE      ( 5*configMINIMAL_STACK_SIZE )
    #define PLAYER_TASK_PRIORITY        2//( tskIDLE_PRIORITY + 1 )

    xTaskHandle xPlayerTaskHandle;
    void vPlayerTask(void *pvArg);
#endif
