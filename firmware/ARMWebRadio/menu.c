#include "menu.h"
#include "options.h"
#include "gpio.h"
#include "hdr_gpio.h"
#include "hdr_tim.h"
#include "hdr_exti.h"
#include "hardware.h"
#include "graphics.h"
#include "pictures.h"
#include "weather.h"
#include "backupreg.h"
#include "printf.h"
#include "alarm.h"
#include "shoutcast.h"
#include "ntp.h"

#include "sd_spi.h"
#include "m25pe80.h"
#include "fs_tools.h"
#include "uda1380.h"
#include "rtcbb.h"
#include "hdr_rcc.h"
#include "emac.h"
#include "stm32fxxx_eth.h"
#include <time.h>

#include "task.h"

//USB
#include "usbd_msc_core.h"
#include "usbd_usr.h"
#include "usbd_desc.h"
#include "usb_conf.h"
#include "usbd_msc_core.h"
#include "usbd_msc_bot.h"
#include "usb_dcd_int.h"

__ALIGN_BEGIN USB_OTG_CORE_HANDLE *USB_OTG_dev = NULL __ALIGN_END ;

//extern uint32_t USBD_OTG_ISR_Handler (USB_OTG_CORE_HANDLE *pdev);

void (*ActiveOption)(void);
void (*PrevOption)(void);

int SleepCounter = -1;
signed short KeypadState        = 0;
signed short KeypadContinous    = 0;
signed short RemoteControlState = 64;
signed char  SDCardInsertStatus = 0;

unsigned char DeviceSleep = 0;

//typedef enum {BACKLIGHT_ON=0, BACKLIGHT_OFF, BACKLIGHT_DECR, BACKLIGHT_PULSE} BacklightState_enum;

volatile BacklightState_enum BacklightState = BACKLIGHT_ON;

void SleepDevice(void);

extern xTaskHandle xEthernetTaskHandle;
extern xSemaphoreHandle xUSART2DMA;
extern xSemaphoreHandle xSPI1DMA;

void vApplicationStackOverflowHook( xTaskHandle xTask, signed char *pcTaskName );

void vMenuTask(void * pvArg)
{
	pvArg = pvArg;

    //Init keyboard pins
  	gpio_pin_cfg( GPIOB, 15, GPIO_CRx_MODE_CNF_IN_PULL_U_D_value);//OK
  	gpio_pin_cfg( GPIOB, 14, GPIO_CRx_MODE_CNF_IN_PULL_U_D_value);//OK
  	gpio_pin_cfg( GPIOD, 15, GPIO_CRx_MODE_CNF_IN_PULL_U_D_value);//OK
  	gpio_pin_cfg( GPIOD, 14, GPIO_CRx_MODE_CNF_IN_PULL_U_D_value);//OK
  	gpio_pin_cfg( GPIOE, 13, GPIO_CRx_MODE_CNF_IN_PULL_U_D_value);//OK
  	gpio_pin_cfg( GPIOE, 12, GPIO_CRx_MODE_CNF_IN_PULL_U_D_value);//OK
  	gpio_pin_cfg( GPIOE, 11, GPIO_CRx_MODE_CNF_IN_PULL_U_D_value);//OK
  	gpio_pin_cfg( GPIOC,  9, GPIO_CRx_MODE_CNF_IN_PULL_U_D_value);//OK

  	//Enable pull-up resistors
  	GPIOB->ODR |= 1<<15 | 1<<14;
  	GPIOD->ODR |= 1<<15 | 1<<14;
  	GPIOE->ODR |= 1<<13 | 1<<12 | 1<<11;
  	GPIOC->ODR |= 1<<9;

  	//Load weather backup data
  	WeatherLoadData();

	//Task working correctly so we can enable interrupts in safe way
	__enable_irq();
	//Set the first option in device and draw all graphics
	ActiveOption    = DrawOptionMainScreen;
	DrawFullMenu_bb = 1;
    LCDClearScreen();
	LCDDrawPicture(17, 2, 94, 3, mateme);
	SetLCDBacklight(100);

    xSemaphoreTake(xUSART2DMA, USART2_DMA_WAITTIME);
	xSemaphoreTake(xSPI1DMA,   SPI1_DMA_WAITTIME);

    if(GetUSBSettings() && !BKP_ALARM_ENABLE_WAKEUP_bb)
    {
        ActiveOption    = DrawOptionUSBConnection;
        USB_OTG_dev = (USB_OTG_CORE_HANDLE*)pvPortMalloc(sizeof(USB_OTG_CORE_HANDLE));

        if(USB_OTG_dev != NULL)
            USBD_Init(USB_OTG_dev, USB_OTG_FS_CORE_ID, &USR_desc, &USBD_MSC_cb, &USR_cb);

        RedLEDPWM(RED_LED_DISABLE);
        GreenLEDPWM(GREEN_LED_DISABLE);
    }
    else
    {
        fsInit();
    }

	//Dummy start
	TIM4_CR1_CEN_bb  = 1;

	vTaskDelay(1000);

	if(BKP_ALARM_ENABLE_WAKEUP_bb)
	{
	    IRQOption       = ActiveOption;
	    ActiveOption    = DrawOptionAlarm;

	    //Alarm sound is different from radio
	    if(GetAlarmSound() != 5)
        {
	        SetLCDBacklightMode(BACKLIGHT_MODE_PULSE);
            BacklightState  = BACKLIGHT_PULSE;
        }

        BKP_ALARM_ENABLE_WAKEUP_bb = 0;
	}

	AlarmSaveSoundsFromSD();
    xQueueReset(KeyboardQueueHandler);

	for(;;)
	{
        unsigned short keyboardirvalue;

        if( xQueueReceive( KeyboardQueueHandler, &keyboardirvalue, 5) )
        {
            if(keyboardirvalue & SOURCE_KEYPAD)
                KeypadState = (keyboardirvalue & (~SOURCE_KEYPAD));
            else if(keyboardirvalue & SOURCE_IR)
                RemoteControlState = (keyboardirvalue & (~SOURCE_IR));
        }

		//Execute periodical functions
		if(DrawParameter28_bb)
		{
			if(ActiveOption == DrawOptionMainScreen)
				DrawParameter1_bb = 1;
			else if(ActiveOption == DrawOptionInternetRadio)
				DrawParameter1_bb = 1;

			++InternetRadioTimer;
			NTPTimeoutProc();
			WeatherTimeoutProc();
			ShoutcastTimeProc();

			if(SleepCounter > 0)
                --SleepCounter;
            else if(SleepCounter == 0)
                SleepDevice();

			//Perform weather forecast updating
			if(WeatherGetUpdatingStatus() != WEATHER_SERVER_UPDATING && AUDIO_Player_status != PLAYER_MUSIC && AUDIO_Player_status != PLAYER_RADIO)
			{
                if(ETH_ReadPHYRegister(uipPHY_ADDRESS, PHY_BSR) & PHY_Linked_Status)
                {
                    unsigned int counter;
                    GetRTCHM(&counter);

                    if(WeatherGetUpdatingStatus() == WEATHER_SERVER_SEND_FIRST_REQ)
                        SendRequestToWeatherForecastServer();

                    if((counter >= (GetWeatherUpdateTime()+WeatherGetRefreshTimeInSeconds())) || WeatherGetUpdatingStatus() == WEATHER_SERVER_GET_IP)
                        SendRequestToWeatherForecastServer();
                }
			}

			//Perform NTP synchronization
			if(NTPGetConnectionStatus() == NTP_CONN_NO_CONNECTED)
            {
                if(ETH_ReadPHYRegister(uipPHY_ADDRESS, PHY_BSR) & PHY_Linked_Status)
                {
                    unsigned int counter;
                    GetRTCHM(&counter);

                    if(counter >= (GetNTPUpdateTime()+NTPGetRefreshTimeInSeconds()))
                        NTPSendRequestToServer();
                }
            }

			DrawParameter28_bb = 0;
		}

		if(RemoteControlState <= RC5_TOP_COMMAND || KeypadState > 0)
		{
			switch(BacklightState)
			{
				case BACKLIGHT_ON:
					ResetTimer7();
				break;

				case BACKLIGHT_PULSE:
                    SetLCDBacklightMode(BACKLIGHT_MODE_PWM);
                    BacklightState = BACKLIGHT_ON;
					TIM7_CR1_CEN_bb  = 0;
					SetLCDBacklight(100);
					ResetTimer7();
					TIM7->PSC        = 3500;
					TIM7_CR1_CEN_bb  = 1;

                break;

				default:
					TIM7_CR1_CEN_bb  = 0;
					SetLCDBacklight(100);
					ResetTimer7();
					TIM7->PSC        = 3500;
					BacklightState   = BACKLIGHT_ON;
					TIM7_CR1_CEN_bb  = 1;
				break;
			}

            if(!DeviceSleep)
            {
                if(RemoteControlState == RC5_POWER || ButtonPlayPauseLong_bb)
                    SleepDevice();
            }
            else
            {
                if(RemoteControlState == RC5_POWER)
                    ResetDevice();
                else
                {
                    DisableSysTick();
                    asm volatile("wfi");
                    EnableSysTick();
                }
            }
		}

		if(SDCARD_INS_bb)
		{
		    SDCardDisablePower();
			SDCardInsertStatus = -50;
		}
		else
		{
			if(SDCardInsertStatus < 0 && USB_OTG_dev == NULL)
			{
				vTaskDelay(5);
				++SDCardInsertStatus;

				//If card is insert again try to initialize it
				if(SDCardInsertStatus == 0)
				{
					printf_("Init again\r\n");

					if(fsInit() != FR_OK)
					{
					    SDCardDisablePower();
						SDCardInsertStatus = -50;
						printf_("File system initialization error. I try again later\r\n");
					}
				}
			}
		}

		//Execute active option
		ActiveOption();
		//Reset used and not used signals from keyboard and remote control
		KeypadState        = 0;
		RemoteControlState = 64;
	}
}

void DisableUSBFunctionality(void)
{
    DCD_DevDisconnect (USB_OTG_dev);
    vTaskDelay(50);
    RCC_AHBENR_OTGFSEN_bb = 0;

    if(USB_OTG_dev != NULL)
    {
        MSC_BOT_DeInit(USB_OTG_dev);
        vPortFree(USB_OTG_dev);
    }

    USB_Radio_DeInit(USB_OTG_dev);
    USB_OTG_dev = NULL;
    fsInit();
    RedLEDPWM(RED_LED_DISABLE);
    GreenLEDPWM(GREEN_LED_CONTINUOUS);
}

void SleepDevice(void)
{
    int i;
    u16 PHYRegister;

    switch(AUDIO_Player_status)
    {
        case PLAYER_MUSIC:
            AUDIO_Playback_Stop();
            AUDIO_Playback_status = STOP;
            AUDIO_Player_status = PLAYER_SAVE;

            for(i=0;((AUDIO_Player_status == PLAYER_SAVE) && (i<15));++i)
                vTaskDelay(10);
        break;

        case PLAYER_RADIO:
            AUDIO_Playback_Stop();
            AUDIO_Playback_status = STOP;
            CloseShoutcastConnection();

            for(i=0;((GetShoutcastConnectionStatus() != SHOUTCAST_CONN_DISCONNECTED) && (i<15));++i)
                vTaskDelay(10);
        break;

        default:
            AUDIO_Player_status = PLAYER_SUSPEND;
        break;
    }

    vTaskDelay(10);

    printf_("Prepare device to enter to sleep mode\r\n");
    DeviceSleep = 1;
    DeInitInterrupts();

    //Disable flash memory
    M25PE80EnablePowerdown();
    //Disable codec
    vhUDA1380EnterSleepMode();

    while(!RTC_CRL_RTOFF_bb){};
    RTC_CRL_CNF_bb = 1;

    RTC_CRH_SECIE_bb = 0;

    RTC_CRL_CNF_bb = 0;
    while(!RTC_CRL_RTOFF_bb){};

    //Disable PHY
    PHYRegister = ETH_ReadPHYRegister(uipPHY_ADDRESS, PHY_BCR);

    if(PHYRegister != ETH_ERROR)
    {
        printf_("PHY read success: 0x%x\r\n", PHYRegister);
        PHYRegister |= PHY_Powerdown;

        if(ETH_WritePHYRegister(uipPHY_ADDRESS, PHY_BCR, PHYRegister) == ETH_SUCCESS)
            printf_("PHY power down success\r\n");

        printf_("PHY read BCR: 0x%x\r\n", ETH_ReadPHYRegister(uipPHY_ADDRESS, PHY_BCR));

        ETH_PowerDownCmd(ENABLE);
    }
    else
    {
        printf_("Read PHY error\r\n");
    }

    //Disable ethernet task
    vTaskSuspend(xEthernetTaskHandle);

    printf_("Reset and disable peripherals\r\n");

    //Reset peripherals
    RCC_APB2RSTR_SPI1RST_bb   = 1;
    //RCC_APB2RSTR_TIM1RST_bb   = 1;
    //RCC_APB2RSTR_AFIORST_bb   = 1;
    //RCC_APB1RSTR_UART4RST_bb  = 1;
    RCC_APB1RSTR_USART2RST_bb = 1;
    RCC_APB1RSTR_SPI3RST_bb   = 1;
    RCC_APB1RSTR_TIM7RST_bb   = 1;
    RCC_APB1RSTR_TIM6RST_bb   = 1;
    RCC_APB1RSTR_TIM5RST_bb   = 1;
    RCC_APB1RSTR_TIM4RST_bb   = 1;
    //RCC_APB1RSTR_TIM3RST_bb   = 1;
    RCC_APB1RSTR_TIM2RST_bb   = 1;
    RCC_AHBRSTR_ETHMACRST_bb  = 1;
    RCC_AHBRSTR_OTGFSRST_bb   = 1;

    //Disable peripherals clock
    RCC_APB2ENR_SPI1EN_bb    = 0;
    //RCC_APB2ENR_TIM1EN_bb    = 0;
    //RCC_APB2ENR_AFIOEN_bb    = 0;
    //RCC_APB1ENR_UART4EN_bb   = 0;
    RCC_APB1ENR_USART2EN_bb	 = 0;
    RCC_APB1ENR_SPI3EN_bb    = 0;
    RCC_APB1ENR_TIM7EN_bb    = 0;
    RCC_APB1ENR_TIM6EN_bb	 = 0;
    RCC_APB1ENR_TIM5EN_bb	 = 0;
    RCC_APB1ENR_TIM4EN_bb    = 0;
    //RCC_APB1ENR_TIM3EN_bb    = 0;
    RCC_APB1ENR_TIM2EN_bb    = 0;
    RCC_AHBENR_ETHMACRXEN_bb = 0;
    RCC_AHBENR_ETHMACTXEN_bb = 0;
    RCC_AHBENR_ETHMACEN_bb	 = 0;
    RCC_AHBENR_OTGFSEN_bb    = 0;
    RCC_AHBENR_DMA2EN_bb     = 0;
    RCC_AHBENR_DMA1EN_bb     = 0;

    ConfigureEXTI14();

    ActiveOption    = DrawOptionGoodBye;
    DrawFullMenu_bb = 1;

    RedLEDPWM(RED_LED_CONTINUOUS);
	GreenLEDPWM(GREEN_LED_DISABLE);
}

void vApplicationStackOverflowHook( xTaskHandle xTask, signed char *pcTaskName )
{
    xTask = xTask;
    printf_("Task %s stack overflow\r\n", pcTaskName);
    while(1){};
}

void TIM5_IRQHandler(void)
{
	if(TIM5_SR_UIF_bb)
	{
		DrawParameter31_bb = 1;
		TIM5_SR_UIF_bb     = 0;
	}
}

void OTG_FS_IRQHandler(void)
{
    if(USB_OTG_dev != NULL)
        USBD_OTG_ISR_Handler(USB_OTG_dev);
}

void TIM6_IRQHandler(void)
{
	unsigned short key, keyt;
	static unsigned short keyp    = 0xFFFF;//key toggle
	static unsigned short keytp   = 0x0000;//key toggle previous value
	static unsigned short keymask  = 0x0000;
	static unsigned short keymaskb = 0x0000;

    static unsigned char ButtonPlayCounter;
	static unsigned char ButtonNextCounter;
	static unsigned char ButtonPrevCounter;
	//static unsigned char ButtonUpCounter;
	//static unsigned char ButtonDownCounter;

	static unsigned char ButtonFastClickCounter;
	static unsigned char ButtonFastClickFrequency;

	if(TIM6_SR_UIF_bb)
	{
		key = (GPIOB->IDR>>14) | ((GPIOD->IDR>>12)&0x000C) | ((GPIOE->IDR>>7)&0x0070) | ((GPIOC->IDR>>2)&0x0080);
		keyt  = key ^ keyp;
		keyt  = (~(key | keyt)) & 0x00FF;
		keyp  = key;
		KeypadContinous = keyt;
		//key   = 0;
		key   = (keytp ^ keyt) & (keytp);//(keytp ^ keyt) & (~keytp); |=
		keytp = keyt;

		if(ButtonPlayPause_T_bb)
        {
            if(ButtonPlayCounter < KEYBOARD_LONG_PRESS_TIME)
                ++ButtonPlayCounter;
            else if(ButtonPlayCounter == KEYBOARD_LONG_PRESS_TIME)
            {
                key     |= (1<<ButtonPlayPauseLong_bit);
                keymask |= (1<<ButtonPlayPause_bit);
                 ++ButtonPlayCounter;
            }
        }
        else
            ButtonPlayCounter = 0;

		if(ButtonNext_T_bb)
        {
            if(ButtonNextCounter < KEYBOARD_LONG_PRESS_TIME)
                ++ButtonNextCounter;
            else if(ButtonNextCounter == KEYBOARD_LONG_PRESS_TIME)
            {
                key     |= (1<<ButtonNextLong_bit);
                keymask |= (1<<ButtonNext_bit);
                ++ButtonNextCounter;
            }
        }
        else
            ButtonNextCounter = 0;

		if(ButtonPrevious_T_bb)
        {
            if(ButtonPrevCounter < KEYBOARD_LONG_PRESS_TIME)
                ++ButtonPrevCounter;
            else if(ButtonPrevCounter == KEYBOARD_LONG_PRESS_TIME)
            {
                key     |= (1<<ButtonPreviousLong_bit);
                keymask |= (1<<ButtonPrevious_bit);
                ++ButtonPrevCounter;
            }
        }
        else
            ButtonPrevCounter = 0;

        //Fast clicking routine (only for VolumePlus and VolumeMinus button) look at KEYBOARD_FAST_CLICK_... macros to configure time dependencies
        if(ButtonVolumePlus_T_bb && !ButtonVolumeMinus_T_bb)
        {
            if(ButtonFastClickCounter < KEYBOARD_FAST_CLICK_LONG_PRESS_TIME)
                ++ButtonFastClickCounter;
            else if(ButtonFastClickCounter == KEYBOARD_FAST_CLICK_LONG_PRESS_TIME)
            {
                key      |= (1<<ButtonVolumePlus_bit);
                keymaskb |= (1<<ButtonVolumePlus_bit);
                ButtonFastClickCounter -= ButtonFastClickFrequency;

                if(ButtonFastClickFrequency > KEYBOARD_FAST_CLICK_TIME_MIN)
                    --ButtonFastClickFrequency;
            }
        }
        else if(ButtonVolumeMinus_T_bb && !ButtonVolumePlus_T_bb)
        {
           if(ButtonFastClickCounter < KEYBOARD_FAST_CLICK_LONG_PRESS_TIME)
                ++ButtonFastClickCounter;
            else if(ButtonFastClickCounter == KEYBOARD_FAST_CLICK_LONG_PRESS_TIME)
            {
                key      |= (1<<ButtonVolumeMinus_bit);
                keymaskb |= (1<<ButtonVolumeMinus_bit);
                ButtonFastClickCounter -= ButtonFastClickFrequency;

                if(ButtonFastClickFrequency > KEYBOARD_FAST_CLICK_TIME_MIN)
                    --ButtonFastClickFrequency;
            }
        }
        else
        {
            ButtonFastClickCounter   = 0;
            ButtonFastClickFrequency = KEYBOARD_FAST_CLICK_TIME_FREQ;
        }

		if(key > 0 && key != 255)
		{
		    unsigned short tmp;
		    portBASE_TYPE xTaskWoken = pdFALSE;

            //Delete normal press indication if we have long press
		    tmp      = ~(key & keymask);
		    key     &= tmp;
		    keymask &= tmp;

            if(key & (1<<ButtonVolumePlus_bit) && keymaskb & (1<<ButtonVolumePlus_bit) && !ButtonVolumePlus_T_bb)
            {
                key     &= (~(1<<ButtonVolumePlus_bit));
                keymaskb = 0;
            }

            if(key & (1<<ButtonVolumeMinus_bit) && keymaskb & (1<<ButtonVolumeMinus_bit) && !ButtonVolumeMinus_T_bb)
            {
                key     &= (~(1<<ButtonVolumeMinus_bit));
                keymaskb = 0;
            }

			key |= SOURCE_KEYPAD;
			xQueueSendFromISR(KeyboardQueueHandler,(void*)&key, &xTaskWoken);
		}

		TIM6_SR_UIF_bb = 0;
	}
}

void TIM7_IRQHandler(void)
{
	static int BacklightVal = 100;

	if(TIM7_SR_UIF_bb)
	{
		switch(BacklightState)
		{
			case BACKLIGHT_ON:
				TIM7_CR1_CEN_bb  = 0;
				TIM7->PSC        = 30;//60;//30
				BacklightVal     = 100;
				BacklightState   = BACKLIGHT_DECR;
				TIM7_CR1_CEN_bb  = 1;
			break;

			case BACKLIGHT_DECR:
				if(BacklightVal > 0)
					SetLCDBacklight(BacklightVal--);
				else
				{
					BacklightState   = BACKLIGHT_OFF;
					TIM7_CR1_CEN_bb  = 0;
					SetLCDBacklight(0);
				}
			break;

			default:
			break;
		}

		TIM7_SR_UIF_bb     = 0;
	}
}

void EXTI15_10_IRQHandler(void)
{
	if(EXTI_PR_PR10_bb)
	{
		TIM4->CNT        = 0;
		TIM4->ARR        = 2546;
		TIM4_CR1_CEN_bb  = 1;
		EXTI_IMR_MR10_bb = 0;
		EXTI_PR_PR10_bb  = 1;
	}

	if(EXTI_PR_PR14_bb)
    {
        //Simulate switch as power command from remote control
        portBASE_TYPE xTaskWoken = pdFALSE;
        unsigned int buffer = (RC5_POWER & RC5_COMMAND_MASK) | SOURCE_IR;
        xQueueSendFromISR(KeyboardQueueHandler,(void*)&buffer, &xTaskWoken);

        EXTI_PR_PR14_bb  = 1;

        if(xTaskWoken == pdTRUE)
            vPortYieldFromISR();
    }
}

void TIM4_IRQHandler(void)
{
	static int lpbit  = 0;
	static unsigned int buffer = 1;

	portBASE_TYPE xTaskWoken = pdFALSE;

	//TODO: Add multiple checking input line

	if(TIM4_SR_UIF_bb)
	{
		if(lpbit == 0)
		{
			buffer <<= 1;
			if(GPIOE->IDR & 1<<10)
				buffer |= 1;

			TIM4->ARR        = 3398;
			lpbit++;
		}
		else if(lpbit >= 13)
		{
			EXTI_IMR_MR10_bb = 1;
			TIM4_CR1_CEN_bb  = 0;
			lpbit  = 0;

			if(((buffer & RC5_START_MASK)        == RC5_START_MASK) &&
               (((buffer & RC5_ADDRESS_MASK)>>6) == RC5_DEVICE_CODE))
			{
				buffer = (buffer & RC5_COMMAND_MASK) | SOURCE_IR;
				xQueueSendFromISR(KeyboardQueueHandler,(void*)&buffer, &xTaskWoken);
			}

			buffer = 1;
		}
		else
		{
			lpbit++;
			buffer <<= 1;

			if(GPIOE->IDR & 1<<10)
				buffer |= 1;
		}

		TIM4_SR_UIF_bb = 0;

        if(xTaskWoken == pdTRUE)
            vPortYieldFromISR();
	}
}

void RTC_IRQHandler(void)
{
	if(RTC_CRL_SECF_bb)
	{
		DrawParameter28_bb = 1;
		RTC_CRL_SECF_bb = 0;
	}

	if(RTC_CRL_ALRF_bb)
	{
	    SetLCDBacklightMode(BACKLIGHT_MODE_PULSE);

	    IRQOption       = ActiveOption;
	    ActiveOption    = DrawOptionAlarm;
	    DrawFullMenu_bb = 1;
	    BacklightState  = BACKLIGHT_PULSE;

        BKP_ALARM_ENABLE_bb = 0;
		RTC_CRL_ALRF_bb = 0;
	}
}

void RTCAlarm_IRQHandler(void)
{
	if(EXTI_PR_PR17_bb)
    {
        EXTI_PR_PR17_bb  = 1;

        if(DeviceSleep)
        {
            BKP_ALARM_ENABLE_bb = 0;
            //Simulate switch as power command from remote control
            portBASE_TYPE xTaskWoken = pdFALSE;
            unsigned int buffer = (RC5_POWER & RC5_COMMAND_MASK) | SOURCE_IR;
            xQueueSendFromISR(KeyboardQueueHandler,(void*)&buffer, &xTaskWoken);

            if(xTaskWoken == pdTRUE)
                vPortYieldFromISR();

            BKP_ALARM_ENABLE_WAKEUP_bb = 1;
        }
    }
}

void ETH_WKUP_IRQHandler(void)
{
	if(EXTI_PR_PR19_bb)
    {
        EXTI_PR_PR19_bb  = 1;

        if(DeviceSleep)
        {
            //Simulate switch as power command from remote control
            portBASE_TYPE xTaskWoken = pdFALSE;
            unsigned int buffer = (RC5_POWER & RC5_COMMAND_MASK) | SOURCE_IR;
            xQueueSendFromISR(KeyboardQueueHandler,(void*)&buffer, &xTaskWoken);

            if(xTaskWoken == pdTRUE)
                vPortYieldFromISR();
        }
    }
}
