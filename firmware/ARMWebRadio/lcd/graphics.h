#ifndef GRAPHICS_H_INCLUDED
	#define GRAPHICS_H_INCLUDED

	#define LCD_MAX_X 127

	#include "time.h"
	#include "weather.h"

    #ifndef INFFT_BUFFER_PRESENT
    #define INFFT_BUFFER_PRESENT

        union InFFTbuffer{
            unsigned int inbuffer[64];
            signed short shortinbuffer[128];
        };
	#endif

	void LCDClearScreen(void);
	void LCDClearArea(int startx, int starty, int endx, int endy);
	void LCDWriteChar(char ch);
	void LCDWriteString(char *str, int x, int y);
	void LCDWriteLimitedString(char *str, int x, int y, int limit);
	void LCDWriteLimitedStringNeg(char *str, int x, int y, int limit);
	void LCDWriteCharNeg(char ch);
	void LCDWriteStringNeg(char *str, int x, int y);
	void LCDWriteStringAllLine(char *str, int y);
	void LCDWriteStringNegAllLine(char *str, int y);
	//void LCDDrawPicture(const char *picture, int x, int y, int height, int width);
	void LCDDrawProgressBar(int y, int pos, int max);
	void LCDDrawFreq(int freqHz, int x, int y);
	void LCDDrawBitrate(int bitrate, int x, int y);
	int  LCDDrawBigString(char *str, int x, int y, int offset);
	void LCDDrawTime(int seconds, int x, int y);
	void LCDDrawLine(int x, int value);
	void ResetArtistAndTitleOffset(void);
	void ResetEntryStringOffset(void);
	void LCDDrawArtistAndTitle(char *artist, char *title, int x, int y);
    void LCDDrawEntryString(char *str, int y);
	void LCDDrawPlayingStatus(int status, int x, int y);
	void LCDDrawList(int position, int range, const char* name, ...);
	void LCDDrawMenuHeader(char *str);
	void LCDDrawdB(int dB, int y);
	void LCDDrawSlider(int position, int range);
	void LCDDrawMenuSlider(int position);
	void LCDDrawStringCenter(int x, int y, int rangex, char* str);
	void LCDDrawStringNegCenter(int x, int y, int rangex, char* str);
	void LCDDrawStringCenterHighlight(int x, int y, int rangex, char *str, int highlight);
	void LCDDrawClearArea(int x, int y, int width, int height, unsigned char pattern);
	void LCDDrawPicture(int x, int y, int width, int height, const unsigned char *pic);
	void LCDDrawVolumeBar(int x, int y, int volume);
	void LCDDrawMusicFilePosition(int x, int y, unsigned int currentfile, unsigned int numoffiles);
	void LCDDrawPlayMode(int playmode, int x, int y);
	void LCDDrawMuteStatus(int mutestatus, int x, int y);
	void LCDClearSpectrumBuffers(void);
	void LCDDrawSpectrum(signed short *inbuff, int x, int y, unsigned int div);//unsigned int *bufferunsigned int *buffer, int x, int y, unsigned int div);

	void SetSpectrumPermanence(int val);
    int  GetSpectrumPermanence(void);

	void LCDDrawIPAddress(int y, unsigned short high, unsigned short low, int highlight);
	void LCDDrawConnectionBar(int val);
	void LCDDrawRTCTime(int hours, int minutes, int x, int y);
	void LCDDrawDate(int daylp, int day, int month, int year, int y, int mode);
	void LCDDrawBufferPosition(int position, int optimalposition, int y);
	void LCDDrawDateSet(int day, int month, int year, int set, int y);
	void LCDDrawTimeSet(int hour, int minutes, int set, int y);
	void LCDDrawCalendar(tmStruct *dat);
	void LCDDrawMiniClock(int x, int y, tmStruct *dat);
	void LCDDrawWeatherInfo(WeatherStruct *data, int forecastid);
#endif
