#include "weather.h"
#include "printf.h"
#include "uip.h"
#include "uipopt.h"
#include "backupreg.h"
#include "ip12b256.h"
#include "extsram.h"
#include "hardware.h"
#include <string.h>
#include <stdio.h>
#include "backupreg.h"
#include "player.h"
#include "wconf.h"

//#define WEATHER_DEBUG_ENABLE 1

//#ifdef WEATHER_DEBUG_ENABLE
    //#define WEATHERDEBUG( ... )		printf_(__VA_ARGS__)
//#else
    #define WEATHERDEBUG( ... )		//printf_(__VA_ARGS__)
//#endif

const char WeatherServerRequestTemplate[]={"HTTP/1.1\r\n"
                                           "User-Agent: ARMWebRadio Mateme\r\n"
                                           "Host: weather.yahooapis.com\r\n"
									       "Accept: text/html, application/xhtml+xml, application/xml\r\n"
									       "Referer: http://weather.yahooapis.com/forecastrss?w=514254&u=c\r\n"
		                                   "Connection: Keep-Alive\r\n\r\n"};
WeatherStruct WeatherData;

unsigned short WeatherForecastLocalPort;
static struct uip_conn *ConnectionHandler;
static int MemoryAddress = 0;
static unsigned char ConnectionStatus = WEATHER_SERVER_DISCONNECTED;
//TODO: Add here timeout callback function
static unsigned char UpdatingStatus   = WEATHER_SERVER_SEND_FIRST_REQ;//WEATHER_SERVER_SUCCESS;
static unsigned char UpdatingTimeout  = 0;

static int WeatherCheckFieldType(char *stream);
static int WeatherGetFieldValue(char *stream, const char *fieldname);
static int WeatherGetDay(char *stream);
static void WeatherGetFieldString(char *stream, const char *fieldname, char *string, int bufferlen);
static void WeatherSaveData(void);

void weather_appcall(void)
{
    unsigned char AnalyseData = 0;

	if(uip_connected())
	{
	    char woeid[11];
		WEATHERDEBUG("Send req\r\n");
		MemoryAddress = 0;
		WOEIDRead(woeid);

		sprintf_(uip_sappdata, "GET /forecastrss?w=%s", woeid);
		//strcpy(uip_sappdata, "GET /forecastrss?w=514254");

		if(GetTemperatureUniSettings())
			strcat(uip_sappdata, "&u=f ");
		else
			strcat(uip_sappdata, "&u=c ");

		strcat(uip_sappdata, WeatherServerRequestTemplate);
		uip_sendappbuffer(strlen(uip_sappdata));

		ConnectionStatus = WEATHER_SERVER_CONNECTED;
		return;
	}

	if(uip_newdata() && AUDIO_Player_status != PLAYER_MUSIC)
	{
	    WEATHERDEBUG("New data\r\n");
		if(xSemaphoreTake(xRadioBufferSemaphore, WEATHERBUFFER_SEMAPHORE_WAITTIME) == pdTRUE)
		{
		    char *pch, *dat;

		    WEATHERDEBUG("Add data\r\n");
			SRAMWriteMemory(uip_appdata, MemoryAddress, uip_datalen(), SRAM_BANK0);
			MemoryAddress += uip_datalen();

			xSemaphoreGive(xRadioBufferSemaphore);

			//Terminate application data and try find /rss tag in string
            dat = uip_appdata;
            dat += uip_datalen();
            *dat = 0;
            WEATHERDEBUG("%s\r\n", uip_appdata);
            pch = strstr(uip_appdata, "</rss>");

            if(pch != NULL)
            {
                AnalyseData = 1;
                WEATHERDEBUG("tag RSS founded\r\n");
            }

		}
		else
        {
            WEATHERDEBUG("Can't add data\r\n");
        }
	}

	if(uip_closed() || AnalyseData)
	{
	    WEATHERDEBUG("Closed\r\n");
        int add = 0;
		int offset = 0;
		int forecastid = 0;
		char *str;
		char buff[250];
		const char tagend[]   = "/>";
		const char yweather[] = "yweather:";

		uip_close();
		//uip_abort();
		//uip_unlisten(htons(WeatherForecastLocalPort));

		if(ConnectionStatus == WEATHER_SERVER_DISCONNECTED)
			return;

		ConnectionStatus = WEATHER_SERVER_DISCONNECTED;

        if(AUDIO_Player_status != PLAYER_MUSIC)
        {
            WEATHERDEBUG("Analyze data\r\n");
            while(add < MemoryAddress)
            {
                if(xSemaphoreTake(xRadioBufferSemaphore, WEATHERBUFFER_SEMAPHORE_WAITTIME) == pdTRUE)
                {
                    if((MemoryAddress-add) >= 250)
                    {
                        SRAMReadMemory(buff, add, 250, SRAM_BANK0);
                        add   += 249;
                        offset = 249;
                    }
                    else
                    {
                        unsigned int counter;

                        SRAMReadMemory(buff, add, (MemoryAddress-add), SRAM_BANK0);
                        offset = (MemoryAddress-add);
                        add   +=  offset+2;

                        GetRTCHM(&counter);
                        UpdatingStatus = WEATHER_SERVER_SUCCESS;
                        SetWeatherUpdateTime(counter);
                    }

                    xSemaphoreGive(xRadioBufferSemaphore);
                }

                buff[249] = 0;
                str = strstr(buff, yweather);

                if(str != NULL)
                {
                    //If yweather tag was founded copy data to buffer from this point
                    str += strlen(yweather);
                    add -= (offset-(str-buff));

                    if(xSemaphoreTake(xRadioBufferSemaphore, WEATHERBUFFER_SEMAPHORE_WAITTIME) == pdTRUE)
                    {
                        SRAMReadMemory(buff, add, 250, SRAM_BANK0);
                        add   += 249;
                        offset = 249;
                        xSemaphoreGive(xRadioBufferSemaphore);
                    }

                    buff[249] = 0;
                    str = strstr(buff, tagend);

                    if(str != NULL)
                    {
                        char strunit[2];

                        *str = 0;
                        WEATHERDEBUG("Tag:%s\r\n", buff);//Display tag data

                        switch(WeatherCheckFieldType(buff))
                        {
                            case WEATHER_LOCATION:
                                WeatherGetFieldString(buff, "city", WeatherData.MainData.LocationName, 20);
                            break;

                            case WEATHER_UNITS:
                                WeatherGetFieldString(buff, "temperature", strunit, 2);

                                if(strunit[0] == 'C')
                                    WeatherData.MainData.Unit = WEATHER_UNIT_C;
                                else
                                    WeatherData.MainData.Unit = WEATHER_UNIT_F;
                            break;

                            case WEATHER_WIND:
                            break;

                            case WEATHER_ATMOSPHERE:
                            break;

                            case WEATHER_ASTRONOMY:
                            break;

                            case WEATHER_CONDITION:
                                WeatherData.CurrentConditions.Code        = WeatherGetFieldValue(buff, "code");
                                WeatherData.CurrentConditions.Temperature = WeatherGetFieldValue(buff, "temp");
                            break;

                            case WEATHER_FORECAST:
                                if(forecastid >= 5)
                                    break;

                                WeatherData.Forecast[forecastid].DayName         = WeatherGetDay(buff);
                                WeatherData.Forecast[forecastid].Code            = WeatherGetFieldValue(buff, "code");
                                WeatherData.Forecast[forecastid].TemperatureLow  = WeatherGetFieldValue(buff, "low");
                                WeatherData.Forecast[forecastid].TemperatureHigh = WeatherGetFieldValue(buff, "high");
                                ++forecastid;
                            break;
                        }

                        str +=  strlen(tagend);
                        add -= ((offset-(str-buff)));
                    }
                }
                else
                {
                    if(add >= 250 && add < MemoryAddress)
                        add -= (strlen(yweather)+2);//If we don't find anything try again a little bit early
                }

                WEATHERDEBUG("Save weather data\r\n");
                WeatherSaveData();
            }
        }
	}
}

void SendRequestToWeatherForecastServer(void)
{
	//Yahoo! weather forecast server address
    const char servername[] = "weather.yahooapis.com";

    uip_ipaddr_t *ipaddr;
    uip_ipaddr_t addr;
    ipaddr = &addr;

    ipaddr = (uip_ipaddr_t*)resolv_lookup((char*)servername);

	if(ipaddr == NULL)
    {
        if(UpdatingStatus != WEATHER_SERVER_GET_IP)
        {
            resolv_query((char*)servername);
            UpdatingStatus  = WEATHER_SERVER_GET_IP;
            UpdatingTimeout = WEATHER_TIMEOUT-WEATHER_TIMEOUT/4;
            WEATHERDEBUG("Obtain IP addres of %s\r\n", servername);
        }
        return;
    }

	ConnectionHandler = uip_connect(ipaddr, htons(80));

	if(ConnectionHandler == NULL)
	{
		WEATHERDEBUG("Connection error\r\n");
		uip_abort();
		return;
	}

    UpdatingTimeout          = 0;
	UpdatingStatus           = WEATHER_SERVER_UPDATING;
	WeatherForecastLocalPort = htons(ConnectionHandler->lport);
	WEATHERDEBUG("Weather forecast local port %d  %d\r\n", ConnectionHandler->lport, htons(ConnectionHandler->lport));
}

int WeatherCheckFieldType(char *stream)
{
	int i;
	const char *FieldNames[]={"location", "units", "wind", "atmosphere", "astronomy", "condition", "forecast"};

	for(i=0;i<WEATHER_LAST_FIELD; ++i)
	{
		if(strstr(stream, FieldNames[i]) != NULL)
			return i;
	}

	return -1;
}

int WeatherGetFieldValue(char *stream, const char *fieldname)
{
	char *str;
	int value = 0;
	int minus = 0;

	str = strstr(stream, fieldname);

	if(str != NULL)
	{
		str += (strlen(fieldname) + 2);

		if(*str == '-')
		{
			minus = 1;
			++str;
		}

		while(*str >= '0' && *str <= '9')
		{
			value = value*10+(*str++-48);
		}
	}

	if(minus)
		return ((-1)*value);
	else
		return value;
}

int WeatherGetDay(char *stream)
{
	int i;
	char *str;
	const char *WeatherDayNames[]={"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

	str = strstr(stream, "day");

	if(str != NULL)
	{
		str += 5;

		for(i=0;i<7;++i)
		{
			if(strncmp(str, WeatherDayNames[i], 3)==0)
				return i;
		}
	}

	return 0;
}

void WeatherGetFieldString(char *stream, const char *fieldname, char *string, int bufferlen)
{
	int i;
	char *str;

	str = strstr(stream, fieldname);

	if(str != NULL)
	{
		str += (strlen(fieldname)+2);

		for(i=0;;++i)
		{
			if(*str != '"' && i < bufferlen)
				*string++ = *str++;
			else
			{
				*string = 0;
				return;
			}
		}
	}

	*string = 0;
}

unsigned char WeatherGetUpdatingStatus(void)
{
	return UpdatingStatus;
}

void WeatherTimeoutProc(void)
{
    if(UpdatingTimeout < WEATHER_TIMEOUT)
        ++UpdatingTimeout;
    else
        UpdatingStatus = WEATHER_SERVER_TIMEOUT;
}

int WeatherGetRefreshTimeInSeconds(void)
{
	const int timesec[]={899, 1799, 3599, 7199, 10799, 17999, 25199};

	int index = GetWheatherForecastRefreshTimeIndex();

	if(index >= 0 && index <= (int)((sizeof(timesec)/sizeof(int))))
	{
		return timesec[index];
	}

    return timesec[0];
}

static void WeatherSaveData(void)
{
	BKP_WEATHER_BACKUP_1  = ((unsigned short)WeatherData.Forecast[0].TemperatureHigh<<8 | WeatherData.Forecast[0].TemperatureLow);
	BKP_WEATHER_BACKUP_2  = ((unsigned short)WeatherData.Forecast[0].DayName<<8 | WeatherData.Forecast[0].Code);
	BKP_WEATHER_BACKUP_3  = ((unsigned short)WeatherData.Forecast[1].TemperatureHigh<<8 | WeatherData.Forecast[1].TemperatureLow);
    BKP_WEATHER_BACKUP_4  = ((unsigned short)WeatherData.Forecast[1].DayName<<8 | WeatherData.Forecast[1].Code);
	BKP_WEATHER_BACKUP_5  = ((unsigned short)WeatherData.Forecast[2].TemperatureHigh<<8 | WeatherData.Forecast[2].TemperatureLow);
	BKP_WEATHER_BACKUP_6  = ((unsigned short)WeatherData.Forecast[2].DayName<<8 | WeatherData.Forecast[2].Code);
	BKP_WEATHER_BACKUP_7  = ((unsigned short)WeatherData.Forecast[3].TemperatureHigh<<8 | WeatherData.Forecast[3].TemperatureLow);
	BKP_WEATHER_BACKUP_8  = ((unsigned short)WeatherData.Forecast[3].DayName<<8 | WeatherData.Forecast[3].Code);
	BKP_WEATHER_BACKUP_9  = ((unsigned short)WeatherData.Forecast[4].TemperatureHigh<<8 | WeatherData.Forecast[4].TemperatureLow);
	BKP_WEATHER_BACKUP_10 = ((unsigned short)WeatherData.Forecast[4].DayName<<8 | WeatherData.Forecast[4].Code);
    BKP_WEATHER_BACKUP_11 = ((unsigned short)WeatherData.CurrentConditions.Temperature<<8 | (WeatherData.CurrentConditions.Code & 0x3F) | (WeatherData.MainData.Unit<<7));
}

void WeatherLoadData(void)
{
    WeatherData.Forecast[0].TemperatureHigh = BKP_WEATHER_BACKUP_1>>8;
    WeatherData.Forecast[0].TemperatureLow  = BKP_WEATHER_BACKUP_1&0x00FF;
    WeatherData.Forecast[0].DayName         = BKP_WEATHER_BACKUP_2>>8;
    WeatherData.Forecast[0].Code            = BKP_WEATHER_BACKUP_2&0x00FF;
    WeatherData.Forecast[1].TemperatureHigh = BKP_WEATHER_BACKUP_3>>8;
    WeatherData.Forecast[1].TemperatureLow  = BKP_WEATHER_BACKUP_3&0x00FF;
    WeatherData.Forecast[1].DayName         = BKP_WEATHER_BACKUP_4>>8;
    WeatherData.Forecast[1].Code            = BKP_WEATHER_BACKUP_4&0x00FF;
    WeatherData.Forecast[2].TemperatureHigh = BKP_WEATHER_BACKUP_5>>8;
    WeatherData.Forecast[2].TemperatureLow  = BKP_WEATHER_BACKUP_5&0x00FF;
    WeatherData.Forecast[2].DayName         = BKP_WEATHER_BACKUP_6>>8;
    WeatherData.Forecast[2].Code            = BKP_WEATHER_BACKUP_6&0x00FF;
    WeatherData.Forecast[3].TemperatureHigh = BKP_WEATHER_BACKUP_7>>8;
    WeatherData.Forecast[3].TemperatureLow  = BKP_WEATHER_BACKUP_7&0x00FF;
    WeatherData.Forecast[3].DayName         = BKP_WEATHER_BACKUP_8>>8;
    WeatherData.Forecast[3].Code            = BKP_WEATHER_BACKUP_8&0x00FF;
    WeatherData.Forecast[4].TemperatureHigh = BKP_WEATHER_BACKUP_9>>8;
    WeatherData.Forecast[4].TemperatureLow  = BKP_WEATHER_BACKUP_9&0x00FF;
    WeatherData.Forecast[4].DayName         = BKP_WEATHER_BACKUP_10>>8;
    WeatherData.Forecast[4].Code            = BKP_WEATHER_BACKUP_10&0x00FF;

    WeatherData.CurrentConditions.Temperature =  BKP_WEATHER_BACKUP_11>>8;
    WeatherData.CurrentConditions.Code        =  BKP_WEATHER_BACKUP_11&0x3F;
    WeatherData.MainData.Unit                 = (BKP_WEATHER_BACKUP_11&0x0080)>>7;

    strcpy(WeatherData.MainData.LocationName, "...");
}

