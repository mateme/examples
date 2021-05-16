#include "wconf.h"

#include "extmemmap.h"
#include "m25pe80.h"
#include "printf.h"
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"

#define WCONFDEBUG( ... )		//printf_(__VA_ARGS__)

extern xSemaphoreHandle xRadioBufferSemaphore;

void WOEIDRead(char *woeid)
{
    char buffer[11];

    if(xSemaphoreTake(xRadioBufferSemaphore, WEATHERCONF_SEMAPHORE_WAITTIME) == pdTRUE)
    {
        M25PE80ReadData(EXT_MEM_WEATHER_BASE, buffer, 11);
        xSemaphoreGive(xRadioBufferSemaphore);

        //Check user name and copy it
        if(buffer[0] != 0xFF)
            strncpy(woeid, buffer, 10);
        else
            strncpy(woeid, DEFAULT_WOEID, 10);

        WCONFDEBUG("Read data: %s\r\n", woeid);
    }
}

void WOEIDWrite(char *woeid)
{
    char buffer[12];
    strncpy(buffer, woeid, 10);

    if(xSemaphoreTake(xRadioBufferSemaphore, WEATHERCONF_SEMAPHORE_WAITTIME) == pdTRUE)
    {
        M25PE80WriteEnable();
        M25PE80PageErase(EXT_MEM_WEATHER_BASE);
        while(M25PE80ReadStatus() & MEM_STATUS_WIP){};
        M25PE80WriteEnable();
        M25PE80PageWrite(EXT_MEM_WEATHER_BASE, buffer, 11);
        while(M25PE80ReadStatus() & MEM_STATUS_WIP){};
        M25PE80WriteDisable();

        WCONFDEBUG("%x\r\nWrite data buffer: %s\r\n", EXT_MEM_WEATHER_BASE, buffer);

        M25PE80ReadData(EXT_MEM_WEATHER_BASE, buffer, 11);
         WCONFDEBUG("Write data bufferB: %s\r\n", buffer);

        xSemaphoreGive(xRadioBufferSemaphore);
    }
}

