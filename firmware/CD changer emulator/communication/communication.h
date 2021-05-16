#ifndef COMMUNICATION_H_INCLUDED
    #define COMMUNICATION_H_INCLUDED

    #include "FreeRTOS.h"
    #include "task.h"
    #include "queue.h"
    #include "semphr.h"

    #define SERIALCOMM_TASK_STACK_SIZE  ( 5*configMINIMAL_STACK_SIZE )
    #define SERIALCOMM_TASK_PRIORITY    2//( tskIDLE_PRIORITY + 2 )

    #define SERIAL_RX_PLAYER_QUEUE_LEN  4
    #define SERIAL_RX_CMD_QUEUE_LEN     4

    #define SERIAL_RX_QUEUE_TIMEOUT     50

    #define SYSTEM_BUTTON_LONG_PRESS_TIME 150//*10ms

    #define CMD_PLAYER_PLAY        1
    #define CMD_PLAYER_PAUSE       2
    #define CMD_PLAYER_NEXT        3
    #define CMD_PLAYER_PREV        4
    #define CMD_PLAYER_SEEK_NEXT   5
    #define CMD_PLAYER_SEEK_PREV   6
    #define CMD_PLAYER_CD1         10
    #define CMD_PLAYER_CD2         20
    #define CMD_PLAYER_CD3         30
    #define CMD_PLAYER_CD4         40
    #define CMD_PLAYER_CD5         50
    #define CMD_PLAYER_CD6         60

    #define CMD_PLAYER_BUTTON      100
    #define CMD_PLAYER_BUTTON_LONG 101

    #define LED_STATUS_PLAYER        0
    #define LED_STATUS_PLAYER_STREND 1
    #define LED_STATUS_BT            2
    #define LED_STATUS_BT_STREND     3
    #define LED_STATUS_BT_CALL       4
    #define LED_STATUS_BT_RING       5

    typedef struct
    {
        unsigned char command;
        unsigned char command_inv;
    }SerialCommand;

    typedef struct
    {
        unsigned char disc;
        unsigned char track;
        unsigned char time_min;
        unsigned char time_sec;
        unsigned char mode;
    }DisplayPacket;


    volatile char CDsInSystem[6];
    volatile DisplayPacket Display;

    xTaskHandle  xSerialCommTaskHandle;

    xSemaphoreHandle   xNewTrackLoaded;

    xQueueHandle SerialRXPlayerHandler;
    xQueueHandle SerialRXCommandHandler;

    void SetSystemLED(int state);
    void vSerialCommunicationTask(void * pvArg);
#endif

