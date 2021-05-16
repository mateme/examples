#ifndef WEATHER_H_INCLUDED
	#define WEATHER_H_INCLUDED

	#include "FreeRTOS.h"
	#include "semphr.h"
	#include "task.h"
	#include "queue.h"

	#define WEATHERBUFFER_SEMAPHORE_WAITTIME 20

	//Weather forecast states for fields keep the same order as in FieldNames array
	#define WEATHER_LOCATION   0
	#define WEATHER_UNITS      1
	#define WEATHER_WIND       2
	#define WEATHER_ATMOSPHERE 3
	#define WEATHER_ASTRONOMY  4
	#define WEATHER_CONDITION  5
	#define WEATHER_FORECAST   6

	#define WEATHER_LAST_FIELD (WEATHER_FORECAST+1)

	#define WEATHER_UNIT_C 0
	#define WEATHER_UNIT_F 1

	#define WEATHER_SERVER_CONNECTED      1
	#define WEATHER_SERVER_DISCONNECTED   0

	#define WEATHER_SERVER_SUCCESS        0
	#define WEATHER_SERVER_UPDATING       1
	#define WEATHER_SERVER_TIMEOUT        2
	#define WEATHER_SERVER_GET_IP         3
	#define WEATHER_SERVER_SEND_FIRST_REQ 4

	#define WEATHER_TIMEOUT             90//s max 255

	typedef struct{
		char LocationName[20];
		unsigned char Unit;
		/*unsigned char Distance;
		unsigned char Pressure;
		unsigned char Speed;
		unsigned char SunriseHour;
		unsigned char SunriseMinutes;
		unsigned char SunsetHour;
		unsigned char SunsetMinutes;*/
	}WeatherMainStruct;

	typedef struct{
		signed char Temperature;
		unsigned char Code;
	}WeatherConditionStruct;

	typedef struct{
		unsigned char DayName;
		signed char TemperatureLow;
		signed char TemperatureHigh;
		unsigned char Code;
		/*unsigned char DateDay;
		unsigned char DateMonth;
		unsigned char DateYear;*/
	}WeatherForecastStruct;

	typedef struct{
		WeatherMainStruct MainData;
		WeatherConditionStruct CurrentConditions;
		WeatherForecastStruct Forecast[5];
	}WeatherStruct;

	extern xSemaphoreHandle xRadioBufferSemaphore;

	WeatherStruct WeatherData;
	unsigned short WeatherForecastLocalPort;
	void SendRequestToWeatherForecastServer(void);
	void weather_appcall(void);
	unsigned char WeatherGetUpdatingStatus(void);
	int WeatherGetRefreshTimeInSeconds(void);
	void WeatherTimeoutProc(void);
	void WeatherLoadData(void);
#endif
