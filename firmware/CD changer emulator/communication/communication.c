#include "communication.h"
#include "hdr_exti.h"
#include "usb_host.h"
#include "spi.h"
#include "hdr_tim.h"
#include "systemcfg.h"
#include "timers.h"

enum CommunicationStateRX
{
    COMM_RADIORX_IDLE,
    COMM_RADIORX_CDX1,
    COMM_RADIORX_CDX2,
} CommunicationStateRX = COMM_RADIORX_IDLE;

enum CommunicationStateTX
{
    COMM_RADIOTX_IDLE,
    COMM_RADIOTX_INITPLAY_ANNOUNCECD,
    COMM_RADIOTX_INITPLAY,
    COMM_RADIOTX_PLAY_LEADIN_ANNOUNCECD,
    COMM_RADIOTX_PLAY_LEADIN,
    COMM_RADIOTX_TRACK_LEADIN,
    COMM_RADIOTX_PLAY,
} CommunicationStateTX = COMM_RADIOTX_IDLE;

enum CommandStateRX
{
    CMD_RCV_IDLE,
    CMD_RCV_START_H,
    CMD_RCV_START_L,
    CMD_RCV_DATABITS_MEASURE_H,
    CMD_RCV_DATABITS_MEASURE_L,
    CMD_RCV_NOISE_RCV_BLOCK,
} CommandReceiverStatus = CMD_RCV_IDLE;

unsigned char ReceivedCommandBits = 0;

int ChangeCDLp;
unsigned char player_data;
int ACKData = 0;
static int ACK = 0;

volatile unsigned short LEDRedPattern   = 0;
volatile unsigned short LEDGreenPattern = 0;

TimerHandle_t xTXTimer;

//TODO: Podczas odbierania danych vwcdcpic ustawia ta zmienna na -4 i kazdy poczatkowy i ostatni bajt odpowiedzi zwieksza go o jeden, wartosci ujemne powoduje zmiane tych dwoch bajtów

/*
switch on in cd mode/radio to cd (play)
0x53 0x2C 0xE4 0x1B
0x53 0x2C 0x14 0xEB

switch off in cd mode/cd to radio (pause)
0x53 0x2C 0x10 0xEF
0x53 0x2C 0x14 0xEB

next
0x53 0x2C 0xF8 0x7

prev
0x53 0x2C 0x78 0x87

seek next
0x53 0x2C 0xD8 0x27 hold down
0x53 0x2C 0xE4 0x1B release
0x53 0x2C 0x14 0xEB

seek prev
0x53 0x2C 0x58 0xA7 hold down
0x53 0x2C 0xE4 0x1B release
0x53 0x2C 0x14 0xEB

cd 1
0x53 0x2C 0x0C 0xF3
0x53 0x2C 0x14 0xEB
0x53 0x2C 0x38 0xC7
send new cd no. to confirm change, else:
0x53 0x2C 0xE4 0x1B beep, no cd (same as play)
0x53 0x2C 0x14 0xEB

cd 2
0x53 0x2C 0x8C 0x73
0x53 0x2C 0x14 0xEB
0x53 0x2C 0x38 0xC7
send new cd no. to confirm change, else:
0x53 0x2C 0xE4 0x1B beep, no cd (same as play)
0x53 0x2C 0x14 0xEB

cd 3
0x53 0x2C 0x4C 0xB3
0x53 0x2C 0x14 0xEB
0x53 0x2C 0x38 0xC7
send new cd no. to confirm change, else:
0x53 0x2C 0xE4 0x1B beep, no cd (same as play)
0x53 0x2C 0x14 0xEB

cd 4
0x53 0x2C 0xCC 0x33
0x53 0x2C 0x14 0xEB
0x53 0x2C 0x38 0xC7
send new cd no. to confirm change, else:
0x53 0x2C 0xE4 0x1B beep, no cd (same as play)
0x53 0x2C 0x14 0xEB

cd 5
0x53 0x2C 0x2C 0xD3
0x53 0x2C 0x14 0xEB
0x53 0x2C 0x38 0xC7
send new cd no. to confirm change, else:
0x53 0x2C 0xE4 0x1B beep, no cd (same as play)
0x53 0x2C 0x14 0xEB

cd 6
0x53 0x2C 0xAC 0x53
0x53 0x2C 0x14 0xEB
0x53 0x2C 0x38 0xC7
send new cd no. to confirm change, else:
0x53 0x2C 0xE4 0x1B beep, no cd (same as play)
0x53 0x2C 0x14 0xEB

scan (in 'play', 'shffl' or 'scan' mode)
0x53 0x2C 0xA0 0x5F

shuffle in 'play' mode
0x53 0x2C 0x60 0x9F

shuffle in 'shffl' mode
0x53 0x2C 0x08 0xF7
0x53 0x2C 0x14 0xEB
*/

/*
SendFrameByte:
		movf	ACKcount, f
		btfsc	STATUS, Z ;;;if Z=1 then goto SendByte
		goto	SendByte

		iorlw	00100000b		; flag acknowledgement

		call SendNewLineUSART

		incf	ACKcount, f
*/

void SetSystemLED(int state)
{
    switch(state)
    {
    case LED_STATUS_PLAYER:
        LEDRedPattern   = 0x000;
        LEDGreenPattern = 0x002;
        break;

    case LED_STATUS_PLAYER_STREND:
        LEDRedPattern   = 0x000;
        LEDGreenPattern = 0x318;
        break;

    case LED_STATUS_BT:
        LEDRedPattern   = 0x002;
        LEDGreenPattern = 0x000;
        break;

    case LED_STATUS_BT_STREND:
        LEDRedPattern   = 0x318;
        LEDGreenPattern = 0x000;
        break;

    case LED_STATUS_BT_CALL:
        LEDRedPattern   = 0;
        LEDGreenPattern = 0;
        break;

    case LED_STATUS_BT_RING:
        LEDRedPattern   = 0x3E0;
        LEDGreenPattern = 0x01F;
        break;

    default:
        LEDRedPattern   = 0;
        LEDGreenPattern = 0;
        break;
    }
}

static unsigned char TXBuffer[8];

//First and last byte in packet, based on vwcdcpic
static unsigned char FrameByte(unsigned char data)
{
    if(ACK < 0)
    {
        data = data & (~0x20);
        ++ACKData;
    }

    return data;
}

//StateIdle
void SendPacketIdle(void)
{
    int i;

    ACK = ACKData;

    TXBuffer[0] = FrameByte(0x74);
    TXBuffer[1] = 0xBE;
    TXBuffer[2] = 0xFE;
    TXBuffer[3] = 0xFF;

    TXBuffer[4] = 0xFF;
    TXBuffer[5] = 0xFF;
    TXBuffer[6] = 0x8F;
    TXBuffer[7] = FrameByte(0x7C);

    //TODO: Docelowo wejdzie tutaj DMA
    for(i=0; i<8; ++i)
        SPI2Transfer(TXBuffer[i]);
}

//BEFEFFFFFF
//TODO: Nie wiem obecnie czy pozycja disc nie powinna byc zmienna, w logach zawsze jest BE jakby wysylal dla jednej plyty

//StateInitPlay
void SendPacketInitPlay(volatile DisplayPacket *p)
{
    int i;

    ACK = ACKData;

    TXBuffer[0] = FrameByte(0x34);
    TXBuffer[1] = 0xBE;
    TXBuffer[2] = 0xFE;
    TXBuffer[3] = 0xFF;

    TXBuffer[4] = 0xFF;
    TXBuffer[5] = 0xFF;
    TXBuffer[6] = 0xEF;
    TXBuffer[7] = FrameByte(0x3C);

    //TODO: Docelowo wejdzie tutaj DMA
    for(i=0; i<8; ++i)
        SPI2Transfer(TXBuffer[i]);
}

//StateInitPlayAnnounceCD
//StatePlayLeadInAnnounceCD
void SendPacketPlayAnnounceCD(int disc)
{
    int i;

    ACK = ACKData;

    if(!(disc >=0 && disc <= 5))
        return;

    TXBuffer[0] = FrameByte(0x34);

    if(CDsInSystem[disc])
        TXBuffer[1] = ~(0xD1 + disc);
    else
        TXBuffer[1] = ~(0x90 + disc);

    TXBuffer[2] = 0x66;//TODO:Te liczby oznaczaja maksymalna ilosc piosenek na cd/ trzeb to znac
    TXBuffer[3] = 0x66;

    TXBuffer[4] = 0xA6;
    TXBuffer[5] = 0xB7;
    TXBuffer[6] = 0xFF;
    TXBuffer[7] = FrameByte(0x3C);

    //TODO: Docelowo wejdzie tutaj DMA
    for(i=0; i<8; ++i)
        SPI2Transfer(TXBuffer[i]);
}

//StatePlayLeadIn
//StateTrackLeadIn
void SendPacketPlayLeadIn(volatile DisplayPacket *p)
{
    int i;

    ACK = ACKData;

    TXBuffer[0] = FrameByte(0x34);
    TXBuffer[1] = (~(p->disc  + 0x41));
    TXBuffer[2] = (~(p->track + 0x01));
    TXBuffer[3] = (~(p->time_min));
    TXBuffer[4] = (~(p->time_sec));
    TXBuffer[5] = (~(p->mode));
    TXBuffer[6] = (0xAE);
    TXBuffer[7] = FrameByte(0x3C);

    //TODO: Docelowo wejdzie tutaj DMA
    for(i=0; i<8; ++i)
        SPI2Transfer(TXBuffer[i]);
}

//StatePlay
void SendPacketPlay(volatile DisplayPacket *p)
{
    int i;

    ACK = ACKData;

    TXBuffer[0] = FrameByte(0x34);
    TXBuffer[1] = (~(p->disc  + 0x41));
    TXBuffer[2] = (~(p->track + 0x01));
    TXBuffer[3] = (~(p->time_min));
    TXBuffer[4] = (~(p->time_sec));
    TXBuffer[5] = (~(p->mode));
    TXBuffer[6] = (0xCF);
    TXBuffer[7] = FrameByte(0x3C);

    //TODO: Docelowo wejdzie tutaj DMA
    for(i=0; i<8; ++i)
        SPI2Transfer(TXBuffer[i]);
}

unsigned char ParseRadioPacket(SerialCommand *p)
{
    if(p->command == ~p->command_inv)
        return p->command;
    else
        return 0;
}

void CommunicationTXCallback(TimerHandle_t xTimer)
{
    static int tx_cnt = 0;
    static int tx_disc_lp = 0;

    switch(CommunicationStateTX)
    {
    case COMM_RADIOTX_IDLE:
        SendPacketIdle();

        ++tx_cnt;

        if(tx_cnt >= 0)//TODO: Na pewno 0?
        {
            CommunicationStateTX = COMM_RADIOTX_INITPLAY_ANNOUNCECD;
            tx_cnt = 0;
        }
        break;

    case COMM_RADIOTX_INITPLAY_ANNOUNCECD:
        SendPacketPlayAnnounceCD(tx_disc_lp);
        CommunicationStateTX = COMM_RADIOTX_INITPLAY;
        break;

    case COMM_RADIOTX_INITPLAY:
        SendPacketInitPlay(&Display);

        ++tx_disc_lp;

        if(tx_disc_lp >= 5)
        {
            ++tx_cnt;
            tx_disc_lp = 0;

            if(tx_cnt >= 2)
            {
                CommunicationStateTX = COMM_RADIOTX_PLAY_LEADIN_ANNOUNCECD;
                tx_disc_lp = 0;
                tx_cnt      = 0;
            }
            else
                CommunicationStateTX = COMM_RADIOTX_INITPLAY_ANNOUNCECD;
        }
        else
        {
            CommunicationStateTX = COMM_RADIOTX_INITPLAY_ANNOUNCECD;
        }
        break;

    case COMM_RADIOTX_PLAY_LEADIN_ANNOUNCECD:
        SendPacketPlayAnnounceCD(tx_disc_lp);

        CommunicationStateTX = COMM_RADIOTX_PLAY_LEADIN;
        break;

    case COMM_RADIOTX_PLAY_LEADIN:
        SendPacketPlayLeadIn(&Display);

        ++tx_cnt;

        if(tx_cnt >= 5)
        {
            CommunicationStateTX = COMM_RADIOTX_PLAY;
            tx_cnt = 0;
        }
        else
        {
            CommunicationStateTX = COMM_RADIOTX_PLAY_LEADIN_ANNOUNCECD;
        }
        break;

    case COMM_RADIOTX_TRACK_LEADIN:
        if(xSemaphoreTake( xNewTrackLoaded, 10) == pdTRUE)
        {
            SendPacketPlayLeadIn(&Display);
            CommunicationStateTX = COMM_RADIOTX_PLAY;
            tx_cnt = 0;
        }
        else
        {
            ++tx_cnt;

            if(tx_cnt > 10)
            {
                //This should never happen, but if happen we have a solution
                SendPacketPlayLeadIn(&Display);
                CommunicationStateTX = COMM_RADIOTX_PLAY;
                xSemaphoreGive(xNewTrackLoaded);
                tx_cnt = 0;
            }
        }
        break;

    case COMM_RADIOTX_PLAY:
        SendPacketPlay(&Display);
        break;
    }
}

void vSerialCommunicationTask(void * pvArg)
{
    pvArg = pvArg;

    /*CDsInSystem[0] = 0;
    CDsInSystem[1] = 0;
    CDsInSystem[2] = 0;
    CDsInSystem[3] = 0;
    CDsInSystem[4] = 0;
    CDsInSystem[5] = 0;*/

    int tx_rp      = 0;
    int tx_disc_lp = 0;

    xTXTimer = xTimerCreate( "timer", 50, pdTRUE, (void*) 0, CommunicationTXCallback);
    xTimerStart( xTXTimer, 0 );

    for(;;)
    {
        SerialCommand RadioPacket;

        //vTaskDelay(50);

        switch(CommunicationStateRX)
        {
        case COMM_RADIORX_IDLE:
            if(xQueueReceive( SerialRXCommandHandler, &RadioPacket, SERIAL_RX_QUEUE_TIMEOUT))
            {
                switch(ParseRadioPacket(&RadioPacket))
                {
                case 0xE4://PLAY 0x53 0x2C 0xE4 0x1B
                    if(CommunicationStateTX != COMM_RADIOTX_PLAY || CommunicationStateTX != COMM_RADIOTX_TRACK_LEADIN)
                    {
                        CommunicationStateTX = COMM_RADIOTX_INITPLAY_ANNOUNCECD;
                        tx_disc_lp = 0;
                        tx_rp      = 0;
                    }

                    player_data = CMD_PLAYER_PLAY;
                    xQueueSend( SerialRXPlayerHandler, &player_data, SERIAL_RX_QUEUE_TIMEOUT);
                    break;

                case 0x10://PAUSE 0x53 0x2C 0x10 0xEF
                    player_data = CMD_PLAYER_PAUSE;
                    xQueueSend( SerialRXPlayerHandler, &player_data, SERIAL_RX_QUEUE_TIMEOUT);
                    break;

                case 0xF8://NEXT 0x53 0x2C 0xF8 0x07
                    CommunicationStateTX = COMM_RADIOTX_TRACK_LEADIN;
                    //TODO: Dodac semafor by watek odtwarzacza zaladowal nowy kawalek
                    xSemaphoreTake( xNewTrackLoaded, 1000);
                    tx_rp = 0;

                    player_data = CMD_PLAYER_NEXT;
                    xQueueSend( SerialRXPlayerHandler, &player_data, SERIAL_RX_QUEUE_TIMEOUT);
                    break;

                case 0x78://PREV  0x53 0x2C 0x78 0x87
                    CommunicationStateTX = COMM_RADIOTX_TRACK_LEADIN;
                    //TODO: Dodac semafor by watek odtwarzacza zaladowal nowy kawalek
                    xSemaphoreTake( xNewTrackLoaded, 1000);
                    tx_rp = 0;

                    player_data = CMD_PLAYER_PREV;
                    xQueueSend( SerialRXPlayerHandler, &player_data, SERIAL_RX_QUEUE_TIMEOUT);
                    break;

                case 0xD8://SEEK NEXT 0x53 0x2C 0xD8 0x27
                    player_data = CMD_PLAYER_SEEK_NEXT;
                    xQueueSend( SerialRXPlayerHandler, &player_data, SERIAL_RX_QUEUE_TIMEOUT);
                    break;

                case 0x58://SEEK PREV 0x53 0x2C 0x58 0xA7
                    player_data = CMD_PLAYER_SEEK_PREV;
                    xQueueSend( SerialRXPlayerHandler, &player_data, SERIAL_RX_QUEUE_TIMEOUT);
                    break;

                case 0x0C://CD1 0x53 0x2C 0x0C 0xF3
                    ChangeCDLp         = 1;
                    CommunicationStateRX = COMM_RADIORX_CDX1;
                    break;

                case 0x8C://CD2 0x53 0x2C 0x8C 0x73
                    ChangeCDLp         = 2;
                    CommunicationStateRX = COMM_RADIORX_CDX1;
                    break;

                case 0x4C://CD3 0x53 0x2C 0x4C 0xB3
                    ChangeCDLp         = 3;
                    CommunicationStateRX = COMM_RADIORX_CDX1;
                    break;

                case 0xCC://CD4 0x53 0x2C 0xCC 0x33
                    ChangeCDLp         = 4;
                    CommunicationStateRX = COMM_RADIORX_CDX1;
                    break;

                case 0x2C://CD5 0x53 0x2C 0x2C 0xD3
                    ChangeCDLp         = 5;
                    CommunicationStateRX = COMM_RADIORX_CDX1;
                    break;

                case 0xAC://CD6 0x53 0x2C 0xAC 0x53
                    ChangeCDLp         = 6;
                    CommunicationStateRX = COMM_RADIORX_CDX1;
                    break;

                case 0xA0://SCAN 0x53 0x2C 0xA0 0x5F
                    Display.mode ^= 0xD0;
                    break;

                case 0x60://SHUFFLE(play mode) 0x53 0x2C 0x60 0x9F (mix tracks in one disc)
                    Display.mode ^= 0x04;
                    break;

                case 0x08://SHUFFLE(shffl mode) 0x53 0x2C 0x08 0xF7 (mix button held down)
                    Display.mode ^= 0x04;
                    break;

                case 0x14://IDLE-END 0x53 0x2C 0x14 0xEB
                    break;

                default:
                    break;
                }
            }
            break;

        case COMM_RADIORX_CDX1:
            if(xQueueReceive( SerialRXCommandHandler, &RadioPacket, SERIAL_RX_QUEUE_TIMEOUT))
            {
                switch(ParseRadioPacket(&RadioPacket))
                {
                case 0x14://CDx step 2 0x53 0x2C 0x14 0xEB
                    CommunicationStateRX = COMM_RADIORX_CDX2;
                    break;

                default:
                    CommunicationStateRX = COMM_RADIORX_IDLE;
                    break;
                }
            }
            break;

        case COMM_RADIORX_CDX2:
            if(xQueueReceive( SerialRXCommandHandler, &RadioPacket, SERIAL_RX_QUEUE_TIMEOUT))
            {
            case 0x38://CDx step 3 0x53 0x2C 0x38 0xC7
                if(CommunicationStateTX != COMM_RADIOTX_PLAY || CommunicationStateTX != COMM_RADIOTX_TRACK_LEADIN)
                {
                    CommunicationStateTX = COMM_RADIOTX_INITPLAY_ANNOUNCECD;
                    tx_disc_lp = 0;
                    tx_rp      = 0;
                }

                player_data = ChangeCDLp*10;
                xQueueSend( SerialRXPlayerHandler, &player_data, SERIAL_RX_QUEUE_TIMEOUT);
                CommunicationStateRX = COMM_RADIORX_IDLE;
                //TODO: Send data to radio, track & time
                break;

            default:
                CommunicationStateRX = COMM_RADIORX_IDLE;
                break;
            }
            break;

        }
    }
}

//xQueueSend( SerialRXPlayerHandler, &data, SERIAL_RX_QUEUE_TIMEOUT);
//TIM5->CNT
//Timer tick -> 10us

void EXTI15_10_IRQHandler(void)
{
    unsigned short tmp;
    static unsigned int data = 0;
    static BaseType_t xHigherPriorityTaskWoken  = pdFALSE;
    static BaseType_t xHigherPriorityTaskWoken2 = pdFALSE;

    if(EXTI_PR_PR14_bb)
    {
        switch(CommandReceiverStatus)
        {
        case CMD_RCV_IDLE:
            TIM5->CNT = 0;
            data      = 0;
            CommandReceiverStatus = CMD_RCV_START_H;
            break;

        case CMD_RCV_START_H:
            tmp       = TIM5->CNT;
            TIM5->CNT = 0;

            if(tmp > 800 && tmp < 1000)
                CommandReceiverStatus = CMD_RCV_START_L;
            else
                CommandReceiverStatus = CMD_RCV_NOISE_RCV_BLOCK;

            break;

        case CMD_RCV_START_L:
            tmp       = TIM5->CNT;
            TIM5->CNT = 0;

            if(tmp > 400 && tmp < 500)
                CommandReceiverStatus = CMD_RCV_DATABITS_MEASURE_H;
            else
                CommandReceiverStatus = CMD_RCV_NOISE_RCV_BLOCK;

            break;

        case CMD_RCV_DATABITS_MEASURE_H:
            tmp       = TIM5->CNT;
            TIM5->CNT = 0;

            if(tmp > 40 && tmp < 70)
                CommandReceiverStatus = CMD_RCV_DATABITS_MEASURE_L;
            else
                CommandReceiverStatus = CMD_RCV_NOISE_RCV_BLOCK;

            break;

        case CMD_RCV_DATABITS_MEASURE_L:
            tmp       = TIM5->CNT;
            TIM5->CNT = 0;

            if(tmp > 40 && tmp < 70)
            {
                CommandReceiverStatus = CMD_RCV_DATABITS_MEASURE_H;
                ++ReceivedCommandBits;
                data = data << 1;
            }
            else if(tmp > 150 && tmp < 190)//170
            {
                CommandReceiverStatus = CMD_RCV_DATABITS_MEASURE_H;
                ++ReceivedCommandBits;
                data = (data | 0x0001) << 1;
            }
            else
            {
                CommandReceiverStatus = CMD_RCV_NOISE_RCV_BLOCK;
            }

            if(ReceivedCommandBits >= 32)
            {
                SerialCommand cmd;
                ReceivedCommandBits   = 0;
                CommandReceiverStatus = CMD_RCV_IDLE;

                if( ((data >> 24) == 0x53) && (((data >> 16) & 0xFF) == 0x2C) )
                {
                    cmd.command     = ( data >> 8 ) & 0xFF;
                    cmd.command_inv =   data & 0xFF;

                    //ACK data request, based on vwcdcpic
                    ACKData = -4;
                    xTimerResetFromISR( xTXTimer, &xHigherPriorityTaskWoken2 );

                    xQueueSendFromISR( SerialRXCommandHandler, &cmd, &xHigherPriorityTaskWoken);

                    if(xHigherPriorityTaskWoken || xHigherPriorityTaskWoken2)
                        portYIELD();
                }
            }
            break;

        case CMD_RCV_NOISE_RCV_BLOCK:
            CommandReceiverStatus = CMD_RCV_IDLE;
            break;
        }

        EXTI_PR_PR14_bb  = 1;
    }
}

void TIM3_IRQHandler(void)
{
    static unsigned char switch_prevstate    = 0;//(GPIOC->IDR & 1<<2) ? 1 : 0;
    static unsigned char switch_nonoise      = 1;
    static unsigned char switch_nonoise_prev = 1;

    static int switch_counter = 0;
    static BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    static unsigned char led_div_counter = 0;
    static unsigned char led_step = 0;

    unsigned char switch_state = (GPIOC->IDR & 1<<2) ? 1 : 0;

    if(TIM3_SR_UIF_bb)
    {
        //////////////////////////LED////////////////////////
        ++led_div_counter;

        if(led_div_counter >= 100)
        {
            led_div_counter = 0;

            ++led_step;

            if(led_step > 9)
                led_step = 0;

            if((LEDRedPattern>>led_step) & 0x01)
                EnableRedLED();
            else
                DisableRedLED();

            if((LEDGreenPattern>>led_step) & 0x01)
                EnableGreenLED();
            else
                DisableGreenLED();
        }
        /////////////////////////////////////////////////////

        ////////////////////////SWITCH///////////////////////
        if(switch_state ^ switch_prevstate)
        {
            switch_prevstate = switch_state;
        }
        else
        {
            switch_nonoise_prev = switch_nonoise;
            switch_nonoise = switch_state;
        }

        if(switch_nonoise_prev == 1 && switch_nonoise == 0)
            switch_counter = 0;

        if(!switch_nonoise)
        {
            if(switch_counter < SYSTEM_BUTTON_LONG_PRESS_TIME)
                ++switch_counter;
        }

        if(switch_nonoise_prev == 0 && switch_nonoise == 1)
        {
            unsigned char data;

            if(switch_counter < SYSTEM_BUTTON_LONG_PRESS_TIME)
                data = CMD_PLAYER_BUTTON;
            else
                data = CMD_PLAYER_BUTTON_LONG;

            switch_counter = 0;

            xQueueSendFromISR( SerialRXPlayerHandler, &data, &xHigherPriorityTaskWoken);

            if(xHigherPriorityTaskWoken)
                portYIELD();
        }
        /////////////////////////////////////////////////////


        TIM3_SR_UIF_bb = 0;
    }
}

