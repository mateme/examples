#ifndef USB_TERMINAL_H_INCLUDED
    #define USB_TERMINAL_H_INCLUDED

    void USBTerminalInit(unsigned char *buff, unsigned int *buffptr, int buffsize);
    void USBTerminalSendaDataToPC(char *buffer, int size);
    void USBTerminalGetDataFromPC(unsigned char *buffer, int size);
#endif

