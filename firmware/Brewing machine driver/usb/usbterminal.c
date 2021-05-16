#include "usbterminal.h"
#include "menu.h"

unsigned char *USBTerminalOutputBuffer      = NULL;
unsigned char *USBTerminalOutputBufferBegin = NULL;
unsigned int  *USBTerminalPTR                = NULL;
unsigned short USBTerminalBufferSize = 0;

void USBTerminalInit(unsigned char *buff, unsigned int *buffptr, int buffsize)
{
    USBTerminalOutputBuffer      = buff;
    USBTerminalOutputBufferBegin = buff;
    USBTerminalPTR               = buffptr;
    USBTerminalBufferSize        = buffsize;

    *USBTerminalPTR = 0;
}

void USBTerminalSendaDataToPC(char *buffer, int size)
{
    if(USBTerminalBufferSize != 0)
    {
        int i;

        for(i=0; i<size; ++i)
        {
            *USBTerminalOutputBuffer++ = *buffer++;
            *USBTerminalPTR += 1;

            if(*USBTerminalPTR >= USBTerminalBufferSize)
            {
                USBTerminalOutputBuffer = USBTerminalOutputBufferBegin;
                *USBTerminalPTR = 0;
            }
        }
    }
}

void USBTerminalGetDataFromPC(unsigned char *buffer, int size)
{
    int i;
    unsigned short key;
    portBASE_TYPE xTaskWoken = pdFALSE;

    static char CharHistory[2] = {0, 0};

    for(i=0; i< size; ++i)
    {
        unsigned char ch = *buffer++;

        if(ch == 0x1B)
        {
            CharHistory[0] = 0x1B;
        }
        else if(ch == 0x5B)
        {
            if(CharHistory[0] == 0x1B)
                CharHistory[1] = 0x5B;
            else
            {
                CharHistory[0] = 0;
                CharHistory[1] = 0;
            }
        }
        else
        {
            switch(ch)
            {
            case 0x42:
                if(CharHistory[0] == 0x1B && CharHistory[1] == 0x5B)
                {
                    key = TERMINAL_UP;
                    xQueueSendFromISR(KeyboardQueueHandler,(void*)&key, &xTaskWoken);
                }
                break;

            case 0x41:
                if(CharHistory[0] == 0x1B && CharHistory[1] == 0x5B)
                {
                    key = TERMINAL_DOWN;
                    xQueueSendFromISR(KeyboardQueueHandler,(void*)&key, &xTaskWoken);
                }
                break;

            case 0x0D:
                key = TERMINAL_ENTER;
                xQueueSendFromISR(KeyboardQueueHandler,(void*)&key, &xTaskWoken);
                break;

            case 0x7F:
                key = TERMINAL_ESC;
                xQueueSendFromISR(KeyboardQueueHandler,(void*)&key, &xTaskWoken);
                break;

            default:
                if(ch >= ' ' && ch <= '}')
                {
                    key = KEYBOARD_SOURCE_PC | ch;
                    xQueueSendFromISR(KeyboardQueueHandler,(void*)&key, &xTaskWoken);
                }
                break;
            }

            CharHistory[0] = 0;
            CharHistory[1] = 0;
        }
    }
}
