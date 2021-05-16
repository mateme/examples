#include "uc1601s.h"
#include "graphics.h"
#include "backupreg.h"
#include "fonts.h"
#include "pictures.h"
#include "weathericons.h"
#include "itoa.h"
#include "time.h"
#include "printf.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>

static signed short offset    = 0;
static signed char  direction = 1;

static signed short ListOffset = 0;
static signed char  ListDirection = 1;

static signed short SpectrumMaxVal[32];
static signed short SpectrumDots[32];//<-unsigned int
static signed short SpectrumPermanence = 3;

void LCDClearScreen(void)
{
	int k;

	for(k=0; k<8; ++k)
	{
		SetPageAddress(k);
		SetColumnAddress(0);
		UC1601S_DataMode();
		UC1601S_SPIWrite128_00H();
	}
}

void LCDClearArea(int startx, int starty, int endx, int endy)
{
	int k, j;

	for(k=starty; k<endy; ++k)
	{
		SetPageAddress(k);
		SetColumnAddress(startx);
		UC1601S_DataMode();

		for(j=startx; j<endx; ++j)
			UC1601S_SPIWrite(0x00);
	}
}

void LCDWriteChar(char ch)
{
	int i;
	char *chptr;

	if(ch != 176)
	{
	    /*//If we have some undefined character replace it with space
	    if(!(ch >= ' ' && ch <= '}'))
            ch = ' ';*/

		ch -= 32;
		chptr = (char*)(font5x8 + ch*5);
	}
	else
	{
		chptr = (char*)font_degree;
	}
	UC1601S_DataMode();
	for(i = 0; i < 5; i++)
		UC1601S_SPIWrite(*(chptr++));

	UC1601S_SPIWrite(0x00);
}

void LCDWriteString(char *str, int x, int y)
{
	SetPageAddress(y);
	SetColumnAddress(x);

	while(*str)
		LCDWriteChar(*(str++));
}

void LCDWriteLimitedString(char *str, int x, int y, int limit)
{
	int i=0;
	SetPageAddress(y);
	SetColumnAddress(x);
	UC1601S_DataMode();
	UC1601S_SPIWrite(0x00);

	while(*str && (i++<limit))
		LCDWriteChar(*(str++));
	while(i++<limit)
		LCDWriteChar(' ');
}

void LCDWriteLimitedStringNeg(char *str, int x, int y, int limit)
{
	int i=0;
	SetPageAddress(y);
	SetColumnAddress(x);
	UC1601S_DataMode();
	UC1601S_SPIWrite(0xFF);

	while(*str && (i++<limit))
		LCDWriteCharNeg(*(str++));
	while(i++<limit)
		LCDWriteCharNeg(' ');
}

void LCDWriteCharNeg(char ch)
{
	int i;
	char *chptr;

	if(ch != 176)
	{
        /*//If we have some undefined character replace it with space
	    if(!(ch >= ' ' && ch <= '}'))
            ch = ' ';*/

		ch -= 32;
		chptr = (char*)(font5x8 + ch*5);
	}
	else
	{
		chptr = (char*)font_degree;
	}
	UC1601S_DataMode();
	for(i = 0; i < 5; i++)
		UC1601S_SPIWrite(~(*(chptr++)));

	UC1601S_SPIWrite(0xFF);
}

void LCDWriteStringNeg(char *str, int x, int y)
{
	SetPageAddress(y);
	SetColumnAddress(x);

	while(*str)
		LCDWriteCharNeg(*(str++));
}

void LCDWriteStringAllLine(char *str, int y)
{
	int i = strlen(str)*6+2;

	SetPageAddress(y);
	SetColumnAddress(0);
	UC1601S_DataMode();
	UC1601S_SPIWrite(0x00);
	UC1601S_SPIWrite(0x00);
	LCDWriteString(str, 2, y);
	SetPageAddress(y);
	SetColumnAddress(i);
	UC1601S_DataMode();

	while((i++)<LCD_MAX_X)
		UC1601S_SPIWrite(0x00);
}

void LCDWriteStringNegAllLine(char *str, int y)
{
	int i = strlen(str)*6+2;

	SetPageAddress(y);
	SetColumnAddress(0);
	UC1601S_DataMode();
	UC1601S_SPIWrite(0xFF);
	UC1601S_SPIWrite(0xFF);
	LCDWriteStringNeg(str, 2, y);
	SetPageAddress(y);
	SetColumnAddress(i);
	UC1601S_DataMode();

	while((i++)<LCD_MAX_X)
		UC1601S_SPIWrite(0xFF);
}

/*void LCDDrawPicture(const char *picture, int x, int y, int height, int width)
{
	int i,j;

	for(i=0; i<(height/8); ++i)
	{
		SetPageAddress(i+y/8);
		SetColumnAddress(x);
		UC1601S_DataMode();

		for(j=0; j<width; ++j)
			UC1601S_SPIWrite(*(picture++));
	}
}*/

void LCDDrawProgressBar(int y, int pos, int max)
{
	int i;

	pos = (pos*128)/max;

	if(pos > 128)
		pos = 128;
	else if(pos < 0)
		pos = 0;

	SetPageAddress(y);
	SetColumnAddress(0);
	UC1601S_DataMode();

	for(i=0;i<pos;++i)
	{
		if(i==0 || i==127)
			UC1601S_SPIWrite(0x18);
		else if(i==1 || i==126)
			UC1601S_SPIWrite(0x3C);
		else
			UC1601S_SPIWrite(0x7E);
	}

	for(i=pos;i<128;++i)
	{
		if(i==0 || i==127)
			UC1601S_SPIWrite(0x18);
		else if(i==1 || i==126)
			UC1601S_SPIWrite(0x24);
		else
			UC1601S_SPIWrite(0x42);
	}
}

void LCDDrawFreq(int freqHz, int x, int y)
{
	freqHz /= 100;

	if(freqHz <= 99)
	{
		char str[5];

		/*str[0] = freqHz/10+48;
		str[1] = 0;*/
		sprintf_(str, "%dkHz", freqHz/10);
		SetPageAddress(y);
		SetColumnAddress(x);
		UC1601S_DataMode();
		UC1601S_SPIWrite(0xFF);
		LCDWriteStringNeg(str, x+1, y);
		//LCDWriteStringNeg("kHz", x+7, y);

		//sprintf_("%dkHz")
	}
	else
	{
		if(freqHz%10 != 0)
		{
			char str[8];

			/*str[0] =  freqHz/100+48;
			str[1] = (freqHz%100)/10+48;
			str[2] = '.';
			str[3] = (freqHz%100)%10+48;
			str[4] = 0;*/
			sprintf_(str, "%.2d.%dkHz", freqHz/10, freqHz%10);
			SetPageAddress(y);
			SetColumnAddress(x);
			UC1601S_DataMode();
			UC1601S_SPIWrite(0xFF);
			LCDWriteStringNeg(str, x+1, y);
			//LCDWriteStringNeg("kHz", x+25, y);
		}
		else
		{
			char str[6];

			/*str[0] = freqHz/100+48;
			str[1] = freqHz%100+48;
			str[2] = 0;*/
			sprintf_(str, "%.2dkHz", freqHz/10);
			SetPageAddress(y);
			SetColumnAddress(x);
			UC1601S_DataMode();
			UC1601S_SPIWrite(0xFF);
			/*LCDWriteStringNeg(str, x+1, y);
			LCDWriteStringNeg("kHz", x+13, y);*/
			LCDWriteStringNeg(str, x+1, y);


		}
	}
}

void LCDDrawBitrate(int bitrate, int x, int y)
{
	/*bitrate /= 1000;

	if(bitrate < 10)
	{
		char str[2];

		str[0] = bitrate+48;
		str[1] = 0;
		SetPageAddress(y);
		SetColumnAddress(x);
		UC1601S_DataMode();
		UC1601S_SPIWrite(0xFF);
		LCDWriteStringNeg(str, x+1, y);
		LCDWriteStringNeg("kbps", x+7, y);
	}
	else if(bitrate < 100)
	{
		char str[3];

		str[0] = bitrate/10+48;
		str[1] = bitrate%10+48;
		str[2] = 0;
		SetPageAddress(y);
		SetColumnAddress(x);
		UC1601S_DataMode();
		UC1601S_SPIWrite(0xFF);
		LCDWriteStringNeg(str, x+1, y);
		LCDWriteStringNeg("kbps", x+13, y);
	}
	else
	{
		char str[4];

		str[0] = bitrate/100+48;
		str[1] =(bitrate%100)/10+48;
		str[2] = bitrate%10+48;
		str[3] = 0;
		SetPageAddress(y);
		SetColumnAddress(x);
		UC1601S_DataMode();
		UC1601S_SPIWrite(0xFF);
		LCDWriteStringNeg(str, x+1, y);
		LCDWriteStringNeg("kbps", x+19, y);
	}*/
	char str[8];
	sprintf_(str, "%dkbps", bitrate/1000);

    SetPageAddress(y);
    SetColumnAddress(x);
    UC1601S_DataMode();
    UC1601S_SPIWrite(0xFF);
    LCDWriteStringNeg(str, x+1, y);
}

int LCDDrawBigChar(char ch, int x , int y, int *offset)
{
	int width, i;
	unsigned char *ptr;//  = (unsigned char*)(Lucida_Console11x16 + (ch-32)*23);
	unsigned char *ptr2;// = ptr + 2;

	/*if(ch != 176)
    {
        //If we have some undefined character replace it with space
	    if(!(ch >= ' ' && ch <= '}'))
            ch = ' ';

		ptr = (unsigned char*)(Lucida_Console11x16 + (ch-32)*23);
    }
	else
		ptr = (unsigned char*)degreeBig;*/

    ptr = (unsigned char*)(Lucida_Console11x16 + (ch-32)*23);

	ptr2 = ptr + 2;

	width = (int)*ptr++;

	if(*offset >= width)
	{
		*offset -= width;
		return 0;
	}

	SetPageAddress(y);
	SetColumnAddress(x);
	UC1601S_DataMode();

	ptr += ((11-width)/2*2 + *offset*2);

	for(i=*offset; i<width; ++i)
	{
        if((i+x) >= LCD_MAX_X)
			break;

		UC1601S_SPIWrite(*ptr);
		ptr += 2;
	}

	SetPageAddress(y+1);
	SetColumnAddress(x);
	UC1601S_DataMode();
	ptr2 += ((11-width)/2*2 + *offset*2);

	for(i=*offset; i<width; ++i)
	{
        if((i+x) >= LCD_MAX_X)
			break;

		UC1601S_SPIWrite(*ptr2);
		ptr2 += 2;
	}

	width -= *offset;
	*offset = 0;

	return width;
}

int LCDDrawBigString(char *str, int x, int y, int offset)
{
	int tmpx = x;
	int off  = offset;

	SetPageAddress(y);
	SetColumnAddress(x);

	while(*str && tmpx <= LCD_MAX_X)
		tmpx += LCDDrawBigChar(*(str++), tmpx, y, &off);

    //Clear space after string to delete unwanted pixels from previous string (only if this space is 5px width or less)
    if((LCD_MAX_X-tmpx) <= 5)
        LCDClearArea(tmpx, y, LCD_MAX_X, y+2);

	return tmpx;
}

void ResetArtistAndTitleOffset(void)
{
	offset    = 0;
	direction = 1;
}

void ResetEntryStringOffset(void)
{
    ListOffset    = 0;
    ListDirection = 1;
}

void LCDDrawArtistAndTitle(char *artist, char *title, int x, int y)
{
	char str[80];

	//Draw only artist and title information if you have it
	if(*artist == 0)
	{
		strcpy(str, title);
	}
	else if(*title == 0)
	{
		strcpy(str, artist);
	}
	else if((*title == 0) && (*artist == 0))
	{
		return;
	}
	else
	{
		strcpy(str, artist);
		strcat(str,"-");
		strcat(str, title);
	}

	if(offset == 0)
	{
		if(LCDDrawBigString(str, x, y, offset) < LCD_MAX_X)
			direction = 2;
	}
	else
	{
		if(LCDDrawBigString(str, x, y, offset) < LCD_MAX_X)
			direction = 0;
	}

	switch(direction)
	{
		case 0:
			offset -=2;

			if(offset <= 0)
				direction = 1;
		break;

		case 1:
			offset +=2;
		break;

		default:

		break;
	}
}

void LCDDrawEntryString(char *str, int y)
{
    LCDWriteLimitedStringNeg((char*)(str+ListOffset), 10, y, 18);

    switch(ListDirection)
    {
        case 0:
            if(ListOffset > 0)
                --ListOffset;
            else
                ListDirection = 1;
        break;

        case 1:
            ListDirection = 2;
        break;

        case 2:
            if(strlen(str) > 18)
            {
                ++ListOffset;

                if(strlen((char*)(str+ListOffset)) <= 18)
                    ListDirection = 3;
            }
        break;

        default:
            ListDirection = 0;
        break;
    }
}

void LCDDrawTime(int seconds, int x, int y)
{
    char str[6];

	if(seconds < 0)
	{
        LCDWriteString("--:--", x, y);
        return;
	}
	else if(seconds > 3599)
	{
		//Change format from MM:SS to HH:MM
		seconds /= 60;
	}

	//SetPageAddress(y);
	//SetColumnAddress(x);
	sprintf_(str, "%2d:%.2d", seconds/60, seconds%60);
	LCDWriteString(str, x, y);

	/*if((seconds/60) < 10)
	{
		char str[3];

		str[0] = seconds/60+48;
		str[1] = (seconds%60)/10+48;
		str[2] = (seconds%60)%10+48;
		LCDWriteChar(' ');
		LCDWriteChar(str[0]);
		LCDWriteChar(':');
		LCDWriteChar(str[1]);
		LCDWriteChar(str[2]);
	}
	else
	{
		char str[4];

		str[0] = (seconds/60)/10+48;
		str[1] = (seconds/60)%10+48;
		str[2] = (seconds%60)/10+48;
		str[3] = (seconds%60)%10+48;
		LCDWriteChar(str[0]);
		LCDWriteChar(str[1]);
		LCDWriteChar(':');
		LCDWriteChar(str[2]);
		LCDWriteChar(str[3]);
	}*/
}

void LCDDrawLine(int x, int value)
{
	int y, tmp;

	if(value > 63)
		value = 63;
	if(value < 0)
	{
		value *= (-1);
		if(value > 63)
			value = 63;
		//value = 0;
	}

	for(y=7; y>=0; --y)
	{
		//unsigned char tmp = 0xFF;
		SetColumnAddress(x);
		SetPageAddress(y);
		UC1601S_DataMode();
		tmp = ((8-y)*8);

		if(value < tmp)
		{
			UC1601S_SPIWrite(0XFF<<(tmp-value));
			switch(tmp-value)
			{
			case 1:UC1601S_SPIWrite(0x80);
				break;
			case 2:UC1601S_SPIWrite(0xC0);
				break;

			case 3:UC1601S_SPIWrite(0xE0);
				break;

			case 4:UC1601S_SPIWrite(0xF0);
				break;

			case 5:UC1601S_SPIWrite(0xF8);

				break;

			case 6:UC1601S_SPIWrite(0xFC);

				break;

			case 7:UC1601S_SPIWrite(0xFE);

				break;

			default:
				UC1601S_SPIWrite(0x00);
				break;
			}
		}
		else
		{
			UC1601S_SPIWrite(0XFF);
		}
	}
}

void LCDDrawPlayingStatus(int status, int x, int y)
{
	unsigned char *ptr;

	switch(status)
	{
		case 0:
			ptr = (unsigned char*)IconStop;
		break;

		case 1:
			ptr = (unsigned char*)IconPlay;
		break;

		case 2:
			ptr = (unsigned char*)IconPause;
		break;

		default:
			return;
		break;
	}

	SetPageAddress(y);
	SetColumnAddress(x);
	UC1601S_DataMode();

	for(x=0; x<7; ++x)
		UC1601S_SPIWrite(*(ptr++));
}

void LCDDrawList(int position, int range, const char* name,...)
{
	int i;
	va_list arg;
	va_start(arg, name);

	if(position >= range)
        position = range-1;

	if(position < 7)
    {
        if(position != 0)
            LCDWriteStringAllLine((char*)name, 1);
        else
            LCDWriteStringNegAllLine((char*)name, 1);
    }
    /*else
    {
        for(i=0;i<range;++i)
        {

        }
    }*/

	for(i=1;i<range;++i)
	{
		if(position != i)
			LCDWriteStringAllLine((char*)va_arg(arg,const char*), i+1);
		else
			LCDWriteStringNegAllLine((char*)va_arg(arg,const char*), i+1);
	}

	va_end(arg);
}

void LCDDrawMenuHeader(char *str)
{
	int i, len;

	len = (128-strlen(str)*6)/2;
	SetColumnAddress(0);
	SetPageAddress(0);
	UC1601S_DataMode();

	for(i=0; i<128; ++i)
	{
		if((len-1) != i)
			UC1601S_SPIWrite(0x08);
		else
			UC1601S_SPIWrite(0x00);
	}

	LCDWriteString(str, len,0);
}

void LCDDrawdB(int dB, int y)
{
	char str[6];

	/*if(dB != 0)
	{
		if(dB > 0)
			str[0] = '+';
		else
		{
			dB    *= (-1);
			str[0] = '-';
		}

		if(dB >= 10)
		{
			str[1] = dB/10+48;
			str[2] = dB%10+48;
			str[3] = 'd';
			str[4] = 'B';
			str[5] = 0;
		}
		else
		{
			str[1] = dB+48;
			str[2] = 'd';
			str[3] = 'B';
			str[4] = ' ';
			str[5] = 0;

			LCDWriteString(" ", 46, y);
			LCDWriteString(" ", 76, y);
			LCDWriteString(str, 49, y);
			return;
		}
	}
	else
	{
		str[0] = str[5] = ' ';
		str[1] = '0';
		str[2] = 'd';
		str[3] = 'B';
		str[4] = ' ';
		str[5] = 0;
	}

	LCDWriteString(str, 46, y);*/

	if(dB > 0)
        sprintf_(str, "+%2ddB", dB);
    else
        sprintf_(str, " %2ddB", dB);

    LCDDrawStringCenter(0, y, 128, str);
}

void LCDDrawSlider(int position, int range)
{
	int j;

	if(position > range)
		position = range;

	position = position*48/range;

	for(j=1; j<(position/8)+1; ++j)
	{
		SetPageAddress(j);
		SetColumnAddress(122);
		UC1601S_DataMode();
		UC1601S_SPIWrite(0xFF);
		UC1601S_SPIWrite(0x00);
		UC1601S_SPIWrite(0x00);
	}

	SetPageAddress(position/8+1);
	SetColumnAddress(122);
	UC1601S_DataMode();

	if((position%8) != 0)
	{
		UC1601S_SPIWrite(~(0xFF<<(position%8+1)));
		UC1601S_SPIWrite(0x42<<(position%8));
		UC1601S_SPIWrite(0x3C<<(position%8));
	}
	else
	{
		UC1601S_SPIWrite(0x81);
		UC1601S_SPIWrite(0x42);
		UC1601S_SPIWrite(0x3C);
	}

	SetPageAddress(position/8+2);
	SetColumnAddress(122);
	UC1601S_DataMode();

	UC1601S_SPIWrite(~(0xFF>>(8-position%8+1)));
	UC1601S_SPIWrite(0x42>>(8-position%8));
	UC1601S_SPIWrite(0x3C>>(8-position%8));

	for(j=(position/8)+3; j<8; ++j)
	{
		SetPageAddress(j);
		SetColumnAddress(122);
		UC1601S_DataMode();
		UC1601S_SPIWrite(0xFF);
		UC1601S_SPIWrite(0x00);
		UC1601S_SPIWrite(0x00);
	}
}

void LCDDrawMenuSlider(int position)
{
	int i;

	if(position > 6)
		return;
	++position;

	//Draw option number in right corner of screen
	SetPageAddress(0);
	SetColumnAddress(122);
	LCDWriteChar(position+48);

	//Draw slidebar
	for(i=1; i<8; ++i)
	{
		SetPageAddress(i);
		SetColumnAddress(122);
		UC1601S_DataMode();

		if(position == i)
		{
			UC1601S_SPIWrite(0x81);
			UC1601S_SPIWrite(0x42);
			UC1601S_SPIWrite(0x3C);
		}
		else
		{
			UC1601S_SPIWrite(0xFF);
			UC1601S_SPIWrite(0x00);
			UC1601S_SPIWrite(0x00);
		}
	}
}

void LCDDrawStringCenter(int x, int y, int rangex, char* str)
{
	LCDWriteString(str, x+((rangex-strlen(str)*6)/2), y);
}

void LCDDrawStringNegCenter(int x, int y, int rangex, char* str)
{
	LCDWriteStringNeg(str, x+((rangex-strlen(str)*6)/2), y);
}

void LCDDrawStringCenterHighlight(int x, int y, int rangex, char *str, int highlight)
{
	int i;

	SetPageAddress(y);
	SetColumnAddress(x+((rangex-strlen(str)*6)/2));
	UC1601S_DataMode();

	for(i=0;*str;++i)
	{
		if(i == highlight)
			LCDWriteCharNeg(*str++);
		else
			LCDWriteChar(*str++);
	}
}

void LCDDrawClearArea(int x, int y, int width, int height, unsigned char pattern)
{
	int i, j;
	height += y;

	for(i=y; i<height; ++i)
	{
		SetPageAddress(i);
		SetColumnAddress(x);
		UC1601S_DataMode();

		for(j=x;j<(width+x);++j)
			UC1601S_SPIWrite(pattern);
	}
}

void LCDDrawPicture(int x, int y, int width, int height, const unsigned char *pic)
{
	int i, j;

	for(i=y; i<(height+y); ++i)
	{
		SetPageAddress(i);
		SetColumnAddress(x);
		UC1601S_DataMode();

		for(j=x;j<(width+x);++j)
			UC1601S_SPIWrite(*(pic++));
	}
}

void LCDDrawVolumeBar(int x, int y, int value)
{
	const unsigned char PatternUpOff[]   = {0xC0, 0x30, 0x08, 0x04, 0x02, 0x02};
	const unsigned char PatternUpOn[]    = {0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFE};
	const unsigned char PatternDownOff[] = {0x03, 0x0C, 0x10, 0x20, 0x40, 0x40};
	const unsigned char PatternDownOn[]  = {0x03, 0x0F, 0x1F, 0x3F, 0x7F, 0x7F};
	/*int i;

	if(value > 252)
		value = 252;

	value = (252-value)/6;
	LCDDrawPicture(x, y, 42, 3, volume);

	SetPageAddress(y+1);
	SetColumnAddress(x);
	UC1601S_DataMode();

	if(value < 22)
	{
		for(i=0;i<value;++i)
			UC1601S_SPIWrite(0xFF<<(8-i*8/21));
	}
	else
	{
		for(i=0;i<22;++i)
			UC1601S_SPIWrite(0xFF<<(8-i*8/21));

		for(i=22;i<value;++i)
			UC1601S_SPIWrite(0xFF);
	}*/
	int i;

	if(value > 252)
		value = 252;

	value = 252-value;

	value = value*24/100;

	SetPageAddress(y);
	SetColumnAddress(x);
	UC1601S_DataMode();

	for(i=0;i<60;++i)
	{
		if(i < 6)
		{
			if(i <= value)
				UC1601S_SPIWrite(PatternUpOn[i]);
			else
				UC1601S_SPIWrite(PatternUpOff[i]);
		}
		else if(i >= 54)
		{
			if(i <= value)
				UC1601S_SPIWrite(PatternUpOn[59-i]);
			else
				UC1601S_SPIWrite(PatternUpOff[59-i]);
		}
		else
		{
			if(i <= value)
				UC1601S_SPIWrite(0xFF);
			else
				UC1601S_SPIWrite(0x01);
		}
	}

	SetPageAddress(y+1);
	SetColumnAddress(x);
	UC1601S_DataMode();

	for(i=0;i<60;++i)
	{
		if(i < 6)
		{
			if(i <= value)
				UC1601S_SPIWrite(PatternDownOn[i]);
			else
				UC1601S_SPIWrite(PatternDownOff[i]);
		}
		else if(i >= 54)
		{
			if(i <= value)
				UC1601S_SPIWrite(PatternDownOn[59-i]);
			else
				UC1601S_SPIWrite(PatternDownOff[59-i]);
		}
		else
		{
			if(i <= value)
				UC1601S_SPIWrite(0xFF);
			else
				UC1601S_SPIWrite(0x80);
		}
	}
}

void LCDDrawMusicFilePosition(int x, int y, unsigned int currentfile, unsigned int numoffiles)
{
	char str[8];

	if(!(currentfile <= numoffiles))
    {
        LCDWriteString(" --/-- ", x, y);
        return;
    }

	/*if(numoffiles < 10)
        sprintf_(str, "%.3d/%d  ",  currentfile, numoffiles);
	else if(numoffiles < 100)
        sprintf_(str, "%.3d/%.2d ", currentfile, numoffiles);
    else
        sprintf_(str, "%.3d/%.3d",  currentfile, numoffiles);*/
    sprintf_(str, "%3d/%d", currentfile, numoffiles);
	LCDWriteString(str, x, y);
}

void LCDDrawPlayMode(int playmode, int x, int y)
{
	unsigned char *ptr;

	switch(playmode)
	{
		case 0:
			ptr = (unsigned char*)normal;
		break;
		case 1:
			ptr = (unsigned char*)repeatone;
		break;

		case 2:
			ptr = (unsigned char*)repeatall;
		break;

		case 3:
			ptr = (unsigned char*)repeatfolder;
		break;

		default:
			return;
		break;
	}

	SetPageAddress(y);
	SetColumnAddress(x);
	UC1601S_DataMode();

	for(x=0; x<24; ++x)
		UC1601S_SPIWrite(*(ptr++));
}

void LCDDrawMuteStatus(int mutestatus, int x, int y)
{
	/*unsigned char *ptr;

	switch(mutestatus)
	{
		case 0:
			ptr = (unsigned char*)muteoff;
		break;

		default:
			ptr = (unsigned char*)muteon;
		break;
	}

	SetPageAddress(y);
	SetColumnAddress(x);
	UC1601S_DataMode();

	for(x=0; x<12; ++x)
		UC1601S_SPIWrite(*(ptr++));*/
    if(mutestatus)
        LCDDrawPicture(x, y, 12, 1, muteon);
    else
        LCDDrawPicture(x, y, 12, 1, muteoff);
}

void LCDClearSpectrumBuffers(void)
{
    memset(SpectrumMaxVal,0,sizeof(SpectrumMaxVal));
    memset(SpectrumDots,0,sizeof(SpectrumDots));
}

void LCDDrawSpectrum(signed short *inbuff, int x, int y, unsigned int div)//unsigned int *buffer
{
	register int i;
	signed short buff[32];
	signed short *ptr=buff;
	//register signed short tmp;
	static int ena = 0;
	const unsigned char pattern[8]={0x80, 0xC0, 0xE0, 0xF0,
			                        0xF8, 0xFC, 0xFE, 0xFF};

    const unsigned char patterndot[8]={0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x03};

	/*for(i=0; i<32; ++i)
        *ptr++ = (signed short)((*(buffer++)&0xFFFF0000)>>16);
		*ptr++ = (signed short)(*(buffer++)&0x0000FFFF);
    */
    /*for(i=0; i<32; ++i)
    {
        signed int Re, Im;
        Re = (signed int)(*(buffer++)&0x0000FFFF);
        Im = (signed int)((*(buffer++)&0xFFFF0000)>>16);

        *ptr++ = (signed short)sqrt(Re*Re/2+Im*Im/2);
    }*/

    //ptr++;

    for(i=0; i<8; ++i)
    {
        signed int Re, Im;

        Im = *inbuff++;
        Re = *inbuff++;

        *ptr++ = (signed short)sqrt(Re*Re/2+Im*Im/2);

        Im = *inbuff++;
        Re = *inbuff++;

        *ptr++ = (signed short)sqrt(Re*Re/2+Im*Im/2);

        Im = *inbuff++;
        Re = *inbuff++;

        *ptr++ = (signed short)sqrt(Re*Re/2+Im*Im/2);

        Im = *inbuff++;
        Re = *inbuff++;

        *ptr++ = (signed short)sqrt(Re*Re/2+Im*Im/2);
    }

	ptr=buff;

	for(i=0; i<32; ++i)
	{
		if(*ptr < 0)
			*ptr = ((*ptr)*(-1))/div;
		else
			*ptr = *ptr/div;

		if(*ptr > SpectrumMaxVal[i] && *ptr <=24)
			SpectrumMaxVal[i] = *ptr;

        if(SpectrumMaxVal[i] > SpectrumDots[i])
            SpectrumDots[i] = SpectrumMaxVal[i];

		ptr++;
		/*if(!(i%2))
			div-=2;*/
	}

	SetPageAddress(y);
	SetColumnAddress(x);
	UC1601S_DataMode();

	ptr=SpectrumMaxVal;

	for(i=0; i<32; ++i)
	{
		if(*ptr >=24)
		{
			UC1601S_SPIWrite(0xFF);
			UC1601S_SPIWrite(0xFF);
		}
		else if(*ptr > 16)
		{
		    if(SpectrumDots[i] > 16 && SpectrumDots[i] < 24)
            {
                UC1601S_SPIWrite(pattern[*ptr-16] | patterndot[SpectrumDots[i]-16]);// | ((1<<SpectrumDots[i])>>16));
                UC1601S_SPIWrite(pattern[*ptr-16] | patterndot[SpectrumDots[i]-16]);// | ((1<<SpectrumDots[i])>>16));
            }
            else
            {
                UC1601S_SPIWrite(pattern[*ptr-16]);// | ((1<<SpectrumDots[i])>>16));
                UC1601S_SPIWrite(pattern[*ptr-16]);// | ((1<<SpectrumDots[i])>>16));
            }
		}
		else
		{
		    if(SpectrumDots[i] > 16 && SpectrumDots[i] < 24)
            {
                UC1601S_SPIWrite(patterndot[SpectrumDots[i] -16]);// | ((1<<SpectrumDots[i])>>16));
                UC1601S_SPIWrite(patterndot[SpectrumDots[i] -16]);// | ((1<<SpectrumDots[i])>>16));
            }
            else
            {
                UC1601S_SPIWrite(0);// | ((1<<SpectrumDots[i])>>16));
                UC1601S_SPIWrite(0);// | ((1<<SpectrumDots[i])>>16));
            }
		}

		ptr++;
	}

	SetPageAddress(y+1);
	SetColumnAddress(x);
	UC1601S_DataMode();

	ptr=SpectrumMaxVal;

	for(i=0; i<32; ++i)
	{
		if(*ptr >= 16)
		{
			UC1601S_SPIWrite(0xFF);
			UC1601S_SPIWrite(0xFF);
		}
        else if(*ptr > 8)
		{
		    if(SpectrumDots[i] > 8 && SpectrumDots[i] < 16)
            {
                UC1601S_SPIWrite(pattern[*ptr-8] | patterndot[SpectrumDots[i] -8]);// | ((1<<SpectrumDots[i])>>16));
                UC1601S_SPIWrite(pattern[*ptr-8] | patterndot[SpectrumDots[i] -8]);// | ((1<<SpectrumDots[i])>>16));
            }
            else
            {
                UC1601S_SPIWrite(pattern[*ptr-8]);// | ((1<<SpectrumDots[i])>>16));
                UC1601S_SPIWrite(pattern[*ptr-8]);// | ((1<<SpectrumDots[i])>>16));
            }
		}
		else
		{
		    if(SpectrumDots[i] > 8 && SpectrumDots[i] < 16)
            {
                UC1601S_SPIWrite(patterndot[SpectrumDots[i] -8]);// | ((1<<SpectrumDots[i])>>16));
                UC1601S_SPIWrite(patterndot[SpectrumDots[i] -8]);// | ((1<<SpectrumDots[i])>>16));
            }
            else
            {
                UC1601S_SPIWrite(0);// | ((1<<SpectrumDots[i])>>16));
                UC1601S_SPIWrite(0);// | ((1<<SpectrumDots[i])>>16));
            }
		}
		/*else if(*ptr > 8)
		{
            UC1601S_SPIWrite(pattern[*ptr-8] | ((1<<SpectrumDots[i])>>8));
            UC1601S_SPIWrite(pattern[*ptr-8] | ((1<<SpectrumDots[i])>>8));
		}
		else
		{
			UC1601S_SPIWrite((1<<SpectrumDots[i])>>8);
			UC1601S_SPIWrite((1<<SpectrumDots[i])>>8);
		}*/
		ptr++;
	}

	SetPageAddress(y+2);
	SetColumnAddress(x);
	UC1601S_DataMode();

	ptr=SpectrumMaxVal;

	for(i=0; i<32; ++i)
	{
		if(*ptr >= 8)
		{
			UC1601S_SPIWrite(0xFF);
			UC1601S_SPIWrite(0xFF);
		}
        else if(*ptr > 0)
		{
		    if(SpectrumDots[i] > 0 && SpectrumDots[i] < 8)
            {
                UC1601S_SPIWrite(pattern[*ptr] | patterndot[SpectrumDots[i] ]);// | ((1<<SpectrumDots[i])>>16));
                UC1601S_SPIWrite(pattern[*ptr] | patterndot[SpectrumDots[i] ]);// | ((1<<SpectrumDots[i])>>16));
            }
            else
            {
                UC1601S_SPIWrite(pattern[*ptr]);// | ((1<<SpectrumDots[i])>>16));
                UC1601S_SPIWrite(pattern[*ptr]);// | ((1<<SpectrumDots[i])>>16));
            }
		}
		else
		{
		    if(SpectrumDots[i] > 0 && SpectrumDots[i] < 8)
            {
                UC1601S_SPIWrite(patterndot[SpectrumDots[i] ]);// | ((1<<SpectrumDots[i])>>16));
                UC1601S_SPIWrite(patterndot[SpectrumDots[i] ]);// | ((1<<SpectrumDots[i])>>16));
            }
            else
            {
                UC1601S_SPIWrite(0);// | ((1<<SpectrumDots[i])>>16));
                UC1601S_SPIWrite(0);// | ((1<<SpectrumDots[i])>>16));
            }
		}
		/*else if(*ptr > 0)
		{
            UC1601S_SPIWrite(pattern[*ptr] | (1<<SpectrumDots[i]));
            UC1601S_SPIWrite(pattern[*ptr] | (1<<SpectrumDots[i]));
		}
		else
		{
			UC1601S_SPIWrite(1<<SpectrumDots[i]);
			UC1601S_SPIWrite(1<<SpectrumDots[i]);
		}*/
		ptr++;
	}

	for(i=0; i<32; ++i)
	{
		if(SpectrumMaxVal[i] > 25)
			SpectrumMaxVal[i] -= SpectrumPermanence;//3
		else if(SpectrumMaxVal[i] > 5)
			SpectrumMaxVal[i] -= (SpectrumPermanence-1);//2
		else if(SpectrumMaxVal[i] > 0)
			SpectrumMaxVal[i] -= ((SpectrumPermanence-2) > 0) ? (SpectrumPermanence-2) : 1;//1
	}

	for(i=0; i<32 && ena==0; ++i)
	{
		if(SpectrumDots[i] > 0)
			--SpectrumDots[i];
	}

	++ena;
	if(ena>2)
        ena = 0;
}

void SetSpectrumPermanence(int val)
{
    SpectrumPermanence = (signed short)val;
    SetSpectrumMode(val);
}

int GetSpectrumPermanence(void)
{
    SpectrumPermanence = GetSpectrumMode();
    return (int)SpectrumPermanence;
}

/*static void ByteToString(unsigned char value, char *str)
{
	if(value < 10)
	{
		*str++ = value+48;
		*str   = 0;
	}
	else if(value < 100)
	{
		*str++ = value/10+48;
		*str++ = value%10+48;
		*str   = 0;
	}
	else
	{
		*str++ = value/100+48;
		*str++ =(value%100)/10+48;
		*str++ = value%10+48;
		*str   = 0;
	}
}*/

void LCDDrawIPAddress(int y, unsigned short high, unsigned short low, int highlight)
{
	int i,j;
	char str[16];
	char *pch = str;
	/*char strtmp[4];
	str[0] =0;

	ByteToString((unsigned char)(high>>8), strtmp);
	strcat(str, strtmp);
	strcat(str, ".");
	ByteToString((unsigned char)(high&0x00FF), strtmp);
	strcat(str, strtmp);
	strcat(str, ".");
	ByteToString((unsigned char)(low>>8), strtmp);
	strcat(str, strtmp);
	strcat(str, ".");
	ByteToString((unsigned char)(low&0x00FF), strtmp);
	strcat(str, strtmp);*/

	LCDDrawClearArea(18, y, 92, 1, 0x00);

	if(highlight >= 1 && highlight <= 12)
	{
		j = 1;

		sprintf_(str,"%3d.%3d.%3d.%3d", (high>>8), (high&0x00FF), (low>>8), (low&0x00FF));
		SetPageAddress(y);

		SetColumnAddress((128-strlen(str)*6)/2);

		for(i=1;i<17;++i)
		{
			if(*pch == '.')
			{
				LCDWriteChar(*(pch++));
				continue;
			}
			else if(*pch == 0)
				break;

			if(highlight==j)
				LCDWriteCharNeg(*(pch++));
			else
				LCDWriteChar(*(pch++));

			++j;
		}
	}
	else
	{
		//LCDDrawClearArea(18, y, 92, 1, 0x00);
		sprintf_(str,"%d.%d.%d.%d", (high>>8), (high&0x00FF), (low>>8), (low&0x00FF));
		LCDDrawStringCenter(0, y, 128, str);
	}

	/*switch(highlight)
	{
		case 1:
			SetPageAddress(y);
			SetColumnAddress((128-strlen(str)*6)/2);
			//LCDWriteString(str, x+((rangex-strlen(str)*6)/2), y);
			for(i=0;i<3;++i)
			{
				LCDWriteCharNeg(*pch);
			}
		break;

		case 2:

		break;

		case 3:

		break;

		case 4:

		break;

		default:
			LCDDrawStringCenter(0, y, 128, str);
		break;
	}*/

	//LCDWriteString(str, (128-strlen(str)*6)/2, y);
	/*if(highlight)
	{
		LCDDrawClearArea(18, y, 92, 1, 0xFF);
		LCDDrawStringNegCenter(0, y, 128, str);
	}
	else
	{
		LCDDrawClearArea(18, y, 92, 1, 0x00);
		LCDDrawStringCenter(0, y, 128, str);
	}*/
}

void LCDDrawConnectionBar(int val)
{
	int i;
	int pattern = 0x7F80>>val;

	SetPageAddress(5);
	SetColumnAddress(14);
	UC1601S_DataMode();

	UC1601S_SPIWrite(0xE0);
	UC1601S_SPIWrite(0x20);

	for(i=0; i<98; ++i)
		UC1601S_SPIWrite(0xA0);

	UC1601S_SPIWrite(0x20);
	UC1601S_SPIWrite(0xE0);

	SetPageAddress(6);
	SetColumnAddress(14);
	UC1601S_DataMode();

	UC1601S_SPIWrite(0xFF);
	UC1601S_SPIWrite(0x00);
	UC1601S_SPIWrite(0xFF);

	for(i=0; i<96; ++i)
	{
		UC1601S_SPIWrite(pattern);

		pattern = pattern >> 1;

		if(pattern == 0x00)
			pattern = 0x7F80;
	}

	UC1601S_SPIWrite(0xFF);
	UC1601S_SPIWrite(0x00);
	UC1601S_SPIWrite(0xFF);

	SetPageAddress(7);
	SetColumnAddress(14);
	UC1601S_DataMode();

	UC1601S_SPIWrite(0x07);
	UC1601S_SPIWrite(0x04);

	for(i=0; i<98; ++i)
		UC1601S_SPIWrite(0x05);

	UC1601S_SPIWrite(0x04);
	UC1601S_SPIWrite(0x07);
}

void LCDDrawRTCTime(int hours, int minutes, int x, int y)
{
	int i, j, k;
	char *pic;
	unsigned char pm = 0;

	if(GetTimeFormatSetting() && hours > 12)
    {
        pm     = 1;
        hours %= 12;
    }

	for(k=0; k<5; ++k)
	{
		switch(k)
		{
			case 0:
				if(hours >= 10)
					pic = (char*)(numbers + (hours/10)*10);
				else
					pic = (char*)(numbers + 110);
			break;

			case 1:
				pic = (char*)(numbers + (hours%10)*10);
			break;

			case 2:
				pic = (char*)(numbers + 100);
			break;

			case 3:
				pic = (char*)(numbers + (minutes/10)*10);
			break;

			case 4:
				pic = (char*)(numbers + (minutes%10)*10);
			break;
		}

		for(i=y; i<(3+y); ++i)
		{
			SetPageAddress(i);
			SetColumnAddress(x);
			UC1601S_DataMode();

			for(j=x;j<(10+x);++j)
				UC1601S_SPIWrite(*(pic++));

			pic = pic + 110;
		}

		if(k==0 && GetTimeFormatSetting())
        {
            if(pm)
                LCDWriteString("P", x, y+2);
            else
                LCDWriteString("A", x, y);
        }

		x += 15; //Free space between numbers
	}
}

void LCDDrawDate(int daylp, int day, int month, int year, int y, int mode)
{
	const char *DayNames[]={"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
	char buffer[15];

	if(daylp >= 0 && daylp < 7)
		strcpy(buffer, DayNames[daylp]);
	else
		strcpy(buffer, "");

	strcat(buffer, " ");

	switch(mode)
	{
		case 1:
			sprintf_(&buffer[strlen(buffer)], "%d.%d.%d", year, month, day);
		break;

		case 2:
			sprintf_(&buffer[strlen(buffer)], "%d.%d.%d", month, day, year);
		break;

		default:
			sprintf_(&buffer[strlen(buffer)], "%d.%d.%d", day, month, year);
		break;
	}

	LCDDrawStringCenter(0, y, 128, buffer);
}

void LCDDrawDateSet(int day, int month, int year, int set, int y)
{
	char buffer[5];

	sprintf_(buffer, "%.2d", day);

	if(set == 1)
		LCDWriteStringNeg(buffer, 34, y);
	else
		LCDWriteString(buffer, 34, y);

	LCDWriteString(".", 46, y);

	sprintf_(buffer, "%.2d", month);

	if(set == 2)
		LCDWriteStringNeg(buffer, 52, y);
	else
		LCDWriteString(buffer, 52, y);

	LCDWriteString(".", 64, y);

	sprintf_(buffer, "%.4d", year);

	if(set == 3)
		LCDWriteStringNeg(buffer, 72, y);
	else
		LCDWriteString(buffer, 72, y);
}

void LCDDrawTimeSet(int hour, int minutes, int set, int y)
{
	char buffer[9];

    if(GetTimeFormatSetting())
    {
        switch(set)
        {
            case 1:
                if(hour > 12)
                {
                    sprintf_(buffer, "%2d:", hour%12);
                    LCDWriteStringNeg(buffer, 40, y);

                    /*if(minutes>=10)
                        sprintf_(buffer, "%d PM", minutes);
                    else
                        sprintf_(buffer, "0%d PM", minutes);*/
                    sprintf_(buffer, "%2d PM", minutes);

                    LCDWriteString(buffer, 58, y);
                }
                else
                {
                    sprintf_(buffer, "%2d:", hour);
                    LCDWriteStringNeg(buffer, 40, y);

                    /*if(minutes>=10)
                        sprintf_(buffer, "%d AM", minutes);
                    else
                        sprintf_(buffer, "0%d AM", minutes);*/
                    sprintf_(buffer, "%2d AM", minutes);

                    LCDWriteString(buffer, 58, y);
                }
            break;

            case 2:
                if(hour > 12)
                {
                    sprintf_(buffer, "%.2d:", hour%12);

                    LCDWriteString(buffer, 40, y);

                    if(minutes>=10)
                        sprintf_(buffer, "%d PM", minutes);
                    else
                        sprintf_(buffer, "0%d PM", minutes);

                    LCDWriteStringNeg(buffer, 58, y);
                }
                else
                {
                    sprintf_(buffer, "%.2d:", hour);

                    LCDWriteString(buffer, 40, y);

                    if(minutes>=10)
                        sprintf_(buffer, "%d AM", minutes);
                    else
                        sprintf_(buffer, "0%d AM", minutes);

                    LCDWriteStringNeg(buffer, 58, y);
                }
            break;

            default:
                if(hour > 12)
                {
                    if(minutes>=10)
                        sprintf_(buffer, "%.2d:%d PM", hour%12, minutes);
                    else
                        sprintf_(buffer, "%.2d:0%d PM", hour%12, minutes);
                }
                else
                {
                    if(minutes>=10)
                        sprintf_(buffer, "%.2d:%d AM", hour, minutes);
                    else
                        sprintf_(buffer, "%.2d:0%d AM", hour, minutes);
                }

                LCDWriteString(buffer, 40, y);
            break;
        }
    }
    else
    {
        switch(set)
        {
            case 1:
                sprintf_(buffer, "%.2d:", hour);
                LCDWriteStringNeg(buffer, 49, y);

                if(minutes>=10)
                    sprintf_(buffer, "%d", minutes);
                else
                    sprintf_(buffer, "0%d", minutes);

                LCDWriteString(buffer, 67, y);
            break;

            case 2:
                sprintf_(buffer, "%.2d:", hour);
                LCDWriteString(buffer, 49, y);

                if(minutes>=10)
                    sprintf_(buffer, "%d", minutes);
                else
                    sprintf_(buffer, "0%d", minutes);

                LCDWriteStringNeg(buffer, 67, y);
            break;

            default:
                if(minutes>=10)
                    sprintf_(buffer, "%.2d:%d", hour, minutes);
                else
                    sprintf_(buffer, "%.2d:0%d", hour, minutes);

                LCDWriteString(buffer, 49, y);
            break;
        }
    }
}

void LCDDrawBufferPosition(int position, int optimalposition, int y)
{
	int i;//, j;

	if(position > 0)
	{
		position = (position - optimalposition)/1480;

		if(position > 22)
			position = 22;
		else if(position < -22)
			position = -22;

		position += 22;

		SetPageAddress(y);
		SetColumnAddress(0);
		UC1601S_DataMode();
		UC1601S_SPIWrite(0x3E);
		for(i=0; i<46; ++i)
		{
			if(i==position || i==(position+1))
				UC1601S_SPIWrite(0x7F);
			else
				UC1601S_SPIWrite(0x41);
		}
		UC1601S_SPIWrite(0x3E);
	}
	else
	{
		LCDClearArea(0, y, 48, y);
		LCDWriteString("BUF ERR", 0, y);
	}
}

void LCDDrawCalendar(tmStruct *dat)
{
	int x, y;
	char str[3];
	int i;
	const char *Days[]={"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
	const char *Months[]={"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

	tmStruct tmp = *dat;

	tmp.tm_mday  = 1;
	GetTime(SetTime(&tmp), &tmp);

	//Draw month name
	LCDClearScreen();
	if(dat->tm_mon < 12 && dat->tm_mon >= 0)
		LCDDrawStringCenter(0, 0, 128, (char*)Months[dat->tm_mon]);

	//Draw days names
	x = 16;
	for(y=0; y<7; ++y)
	{
		LCDWriteString((char*)Days[y], x, 1);
		x += 14;
	}

	i = 1;
	x = 16 + tmp.tm_wday*14;

	for(y=2;y<=7;++y)
	{
		for(;x<112;x+=14)
		{
            //If we have February we should check that current year is lapyear or not
			if(dat->tm_mon == 1)
			{
				if(IsLapYear(dat->tm_year))
				{
					if(i>29)
						return;
				}
				else
				{
					if(i>28)
						return;
				}
			}
			else
			{
				if(i>MonthDays[dat->tm_mon])
					return;
			}

			/*if(i<10)
			{
				sprintf_(str," %d", i);

				if(dat->tm_mday == i)
					LCDWriteStringNeg(str, x, y);
				else
					LCDWriteString(str, x, y);
			}
			else
			{
				sprintf_(str,"%d", i);

				if(dat->tm_mday == i)
					LCDWriteStringNeg(str, x, y);
				else
					LCDWriteString(str, x, y);
			}*/
            sprintf_(str,"%2d", i);

            if(dat->tm_mday == i)
                LCDWriteStringNeg(str, x, y);
            else
                LCDWriteString(str, x, y);

			++i;
		}

		x = 16;
	}
}

void LCDDrawMiniClock(int x, int y, tmStruct *dat)
{
	char str[6];

	LCDDrawPicture(x, y, 7, 1, miniclock);

	/*if(dat->tm_min < 10)
		sprintf_(str, "%.2d:0%d", dat->tm_hour, dat->tm_min);
	else
		sprintf_(str, "%.2d:%.2d", dat->tm_hour, dat->tm_min);*/
    sprintf_(str, "%2d:%.2d", dat->tm_hour, dat->tm_min);

	LCDWriteString(str, x+8, y);
}

void LCDDrawWeatherInfo(WeatherStruct *data, int forecastid)
{
	int i;
	char buffer[16];
	const char *Days[]={"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};

	LCDDrawMenuHeader(data->MainData.LocationName);

	for(i=0;i<5;++i)
	{
		if(data->MainData.Unit == WEATHER_UNIT_C)
		{
			sprintf_(buffer,"%s%3d°C%3d°C", Days[data->Forecast[i].DayName], data->Forecast[i].TemperatureLow, data->Forecast[i].TemperatureHigh);

			if(i == forecastid)
				LCDWriteStringNeg(buffer, 53, i+3);
			else
				LCDWriteString(buffer, 53, i+3);
		}
		else
		{
			sprintf_(buffer,"%s%3d°F%3d°F", Days[data->Forecast[i].DayName], data->Forecast[i].TemperatureLow, data->Forecast[i].TemperatureHigh);

			if(i == forecastid)
				LCDWriteStringNeg(buffer, 53, i+3);
			else
				LCDWriteString(buffer, 53, i+3);
		}
	}

	//Draw weather icon
	if(forecastid >= 0 && forecastid <= 4)
	{
		unsigned char *pic;

		if(data->Forecast[forecastid].Code >= 1 && data->Forecast[forecastid].Code <= 47)
			pic = (unsigned char*)WeatherIcons[data->Forecast[forecastid].Code-1];
		else
			pic = 0;

		if(pic != 0)
			LCDDrawPicture(0, 1, 52, 7, pic);
		else
		{
            LCDClearArea(0, 1, 52, 7);

            if(data->Forecast[forecastid].Code >= 19 && data->Forecast[forecastid].Code <= 25)
            {
                const char *WeatherNames[] = {"DUST", "FOG", "HAZE", "SMOKE", "WINDY", "WINDY", "FRIGID"};
                LCDDrawStringCenter(0, 4, 56, (char*)WeatherNames[data->Forecast[forecastid].Code-19]);
            }
            else
            {
                sprintf_(buffer,"C:%d", data->Forecast[forecastid].Code);
                LCDWriteString(buffer, 0, 5);
            }
		}
	}
	else
	{
		unsigned char *pic;

		if(data->CurrentConditions.Code >= 1 && data->CurrentConditions.Code <= 47)
			pic = (unsigned char*)WeatherIcons[data->CurrentConditions.Code-1];
		else
			pic = 0;

		if(pic != 0)
			LCDDrawPicture(0, 1, 52, 7, pic);
		else
		{
			LCDClearArea(0, 1, 52, 7);
			sprintf_(buffer,"C:%d", data->Forecast[forecastid].Code);
			LCDWriteString(buffer, 0, 5);
		}
	}

	if(data->MainData.Unit == WEATHER_UNIT_C)
		sprintf_(buffer, "%3d°C", data->CurrentConditions.Temperature);
	else
		sprintf_(buffer, "%3d°F", data->CurrentConditions.Temperature);

	LCDDrawBigString(buffer, 65, 1, 0);
}
