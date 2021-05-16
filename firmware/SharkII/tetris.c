#include "tetris.h"
#include "pictures.h"
#include "graphics.h"
#include "eeprom.h"
#include "data.h"
#include "pad.h"
#include "sound.h"
#include "video.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdlib.h>

#define CurrentPart(x)    Data[x+160]
#define prevTimerTick     Data[164]
#define CurrentPartNumber Data[165]
#define CurrentRotate     Data[166]
#define GameSpeed         Data[167]
#define GameFaster        Data[168]
#define MoveElement       Data[169]
#define PartNumber        Data[170]
#define game              Data[171]

#define PART_PALKA   0
#define PART_KWADRAT 1
#define PART_KOREK   2
#define PART_ELKA1   3
#define PART_ELKA2   4
#define PART_ZIGZAK1 5
#define PART_ZIGZAK2 6

#define GAME_SPEED_NORMAL 19
#define GAME_SPEED_FAST   3
#define GAME_PART_SPEED   5

PROGMEM unsigned char Parts[28] ={
	5, 15, 25, 35,//palka
	4,  5, 14, 15,//kwadrat
	4, 13, 14, 15,//korek
	5, 15, 25, 24,//elka1
	5, 15, 25, 26,//elka2
	14,15, 23, 24,//zygzak1
	13,14, 24, 25 //zygzak2
};

static void DrawBlock(unsigned char type, unsigned char column, unsigned char row)
{
	volatile unsigned char *ptr = &Buffer[column+row*96+24];
	
	if(type > 0)
	{
		*(ptr)    = 0xAA;
		*(ptr+24) = 0xAA;//BE;
		*(ptr+48) = 0xAA;//BE;
		*(ptr+72) = 0xAA;
	}
	else
	{
		*(ptr)    = 0x55;
		*(ptr+24) = 0x55;
		*(ptr+48) = 0x55;
		*(ptr+72) = 0x55;			
	}		
}

static inline void DrawNextPart(void)
{
	unsigned char temp;
	for(unsigned char i=0; i<16; ++i)
		DrawBlock(0, 16+i%4, i/4+8);
		
	for(unsigned char i=0; i<4; ++i)
	{
		temp = pgm_read_byte(&Parts[i+PartNumber*4]);
		DrawBlock(1, 13+temp%10, temp/10+8);
	}
}

static inline void DrawBorder(void)
{
	volatile unsigned char *ptr = &Buffer[96];
	
	*(ptr++) = 0x57;
	for(unsigned char i=0; i<10; ++i)
		*(ptr++) = 0xFF;
	*ptr = 0xD5;
	
	ptr = ptr + 13;
	
	for(unsigned char i=0; i<64; ++i)
	{
		*(ptr)   |= 0x57;
		*(ptr+11) |= 0xD5;
		
		ptr = ptr + 24;
	}
	
	*(ptr++) = 0x57;
	for(unsigned char i=0; i<10; ++i)
		*(ptr++) = 0xFF;
	*ptr = 0xD5;
}

static inline void DrawBlocks(void)
{
	volatile unsigned char *ptr = &Buffer[121];
	unsigned char *ptrData = Data;
	
	for(unsigned char y=0; y<16; ++y)
	{
		for(unsigned char x=0; x<10; ++x)
		{
			if(*(ptrData++) > 0)
			{
				*(ptr)    = 0xAA;
				*(ptr+24) = 0xAA;//BE;
				*(ptr+48) = 0xAA;//BE;
				*(ptr+72) = 0xAA;
			}
			else
			{
				*(ptr)    = 0x55;
				*(ptr+24) = 0x55;
				*(ptr+48) = 0x55;
				*(ptr+72) = 0x55;			
			}
			ptr++;
		}
		ptr = ptr+86;
	}
}

static inline void CheckPad(void)
{
	register unsigned char temp;
	register unsigned char i;

	if(TimerTick2 > GAME_PART_SPEED && MoveElement == 1)
	{
		TimerTick2 = 0;
		MoveElement = 0;
	}
	
	if(CHECK_KEY_LEFT && MoveElement == 0)
	{
		for(i=0; i<4; ++i)
		{
			temp = CurrentPart(i);
			if(temp%10 == 0)
				return;
				
			if(Data[temp-1] > 0)
				return;
		}
		
		for(i=0; i<4; ++i)
			CurrentPart(i) = CurrentPart(i) - 1;
		
		TimerTick2 = 0;
		MoveElement = 1;
		return;
	}
	else if(CHECK_KEY_RIGHT && MoveElement == 0)
	{
		for(i=0; i<4; ++i)
		{
			temp = CurrentPart(i);
			if(temp%10 == 9)
				return;
				
			if(Data[temp+1] > 0)
				return;
		}
		
		for(i=0; i<4; ++i)
			CurrentPart(i) = CurrentPart(i) + 1;
		
		TimerTick2 = 0;
		MoveElement = 1;
		return;
	}
	
	if((!CHECK_KEY_LEFT) && (!CHECK_KEY_RIGHT))
		MoveElement = 0;
}

static inline void ClearPart(void)
{
	for(register unsigned char i=0; i<4; ++i)
		Data[CurrentPart(i)] = 0;
}

static inline void CreatePart(void)
{
	for(register unsigned char i=0; i<4; ++i)
		Data[CurrentPart(i)] = 1;
}

static inline void PlaceNewPart(void)
{
	register unsigned char place;
	register unsigned char i;
	CurrentPartNumber = PartNumber;
	PartNumber = rand()%7;
	CurrentRotate     = 0;
	
	for(i=0; i<4; ++i)
	{
		place = pgm_read_byte(&Parts[i+CurrentPartNumber*4]);
		CurrentPart(i) = place;
	}
	
	for(i=0; i<4; ++i)
	{
		if(Data[CurrentPart(i)+10] > 0)
		{
			CreatePart();
			game = 0;
			return;
		}
	}
	
	CreatePart();
	srand(TCNT2);
	DrawNextPart();
}

static inline void RotatePart(void)
{
	static unsigned char ena=0;
	unsigned char TempPart[4];
	unsigned char TempRotate;
	unsigned char action = 0;
	unsigned char Keys = PadStatus;
	
	if( ((!(Keys & 1<<KEY_A)) || (!(Keys & 1<<KEY_B))) && ena == 0)
	{
		TempRotate = CurrentRotate;
		
		if(!(Keys & 1<<KEY_A))
		{
			CurrentRotate = CurrentRotate + 1;
			
			if(CurrentRotate > 3)
				CurrentRotate = 0;
		}
		else if(!(Keys & 1<<KEY_B))
		{
			if(CurrentRotate > 0)
				CurrentRotate = CurrentRotate - 1;
			else
				CurrentRotate = 3;
		}
		
		TempPart[0] = CurrentPart(0);
		TempPart[1] = CurrentPart(1);
		TempPart[2] = CurrentPart(2);
		TempPart[3] = CurrentPart(3);
			
		switch(CurrentPartNumber)
		{
			case PART_PALKA:
				if(CurrentRotate%2)
				{
					CurrentPart(0) = CurrentPart(0) + 12;
					CurrentPart(2) = CurrentPart(2) - 11;
					CurrentPart(3) = CurrentPart(3) - 19;			
				}
				else
				{
					CurrentPart(0) = CurrentPart(0) - 12;
					CurrentPart(2) = CurrentPart(2) + 11;
					CurrentPart(3) = CurrentPart(3) + 19;			
				}
			break;
			
			case PART_KWADRAT:
			break;
			
			case PART_KOREK:
				if(!(Keys & 1<<KEY_A))
				{
					switch(CurrentRotate)
					{
						case 0:
							CurrentPart(3) = CurrentPart(3) - 9;
						break;
						
						case 1:
							CurrentPart(1) = CurrentPart(1) + 11;
						break;
						
						case 2:
							CurrentPart(1) = CurrentPart(1) - 11;
							CurrentPart(0) = CurrentPart(0) + 20;
						break;
						
						case 3:
							CurrentPart(0) = CurrentPart(0) - 20;
							CurrentPart(3) = CurrentPart(3) + 9;
						break;
					}
				}
				else
				{
					switch(CurrentRotate)
					{
						case 0:
							CurrentPart(1) = CurrentPart(1) - 11;
						break;
						
						case 1:
							CurrentPart(1) = CurrentPart(1) + 11;
							CurrentPart(0) = CurrentPart(0) - 20;
						break;
						
						case 2:
							CurrentPart(0) = CurrentPart(0) + 20;
							CurrentPart(3) = CurrentPart(3) - 9;
						break;
						
						case 3:
							CurrentPart(3) = CurrentPart(3) + 9;
						break;
					}			
				}
			break;
			
			case PART_ELKA1:
				if(!(Keys & 1<<KEY_A))
				{
					switch(CurrentRotate)
					{
						case 0:
							CurrentPart(3) = CurrentPart(3) - 2;
							CurrentPart(2) = CurrentPart(2) + 11;
							CurrentPart(0) = CurrentPart(0) - 11;
						break;
						
						case 1:
							CurrentPart(3) = CurrentPart(3) - 20;
							CurrentPart(2) = CurrentPart(2) - 11;
							CurrentPart(0) = CurrentPart(0) + 11;
						break;
						
						case 2:
							CurrentPart(3) = CurrentPart(3) + 2;
							CurrentPart(2) = CurrentPart(2) + 11;
							CurrentPart(0) = CurrentPart(0) - 11;
						break;
						
						case 3:
							CurrentPart(3) = CurrentPart(3) + 20;
							CurrentPart(2) = CurrentPart(2) - 11;
							CurrentPart(0) = CurrentPart(0) + 11;
						break;
					}
				}
				else
				{
					switch(CurrentRotate)
					{
						case 0:
							CurrentPart(3) = CurrentPart(3) + 20;
							CurrentPart(2) = CurrentPart(2) + 11;
							CurrentPart(0) = CurrentPart(0) - 11;
						break;
						
						case 1:
							CurrentPart(3) = CurrentPart(3) - 2;
							CurrentPart(2) = CurrentPart(2) - 11;
							CurrentPart(0) = CurrentPart(0) + 11;
						break;
						
						case 2:
							CurrentPart(3) = CurrentPart(3) - 20;
							CurrentPart(2) = CurrentPart(2) + 11;
							CurrentPart(0) = CurrentPart(0) - 11;
						break;
						
						case 3:
							CurrentPart(3) = CurrentPart(3) + 2;
							CurrentPart(2) = CurrentPart(2) - 11;
							CurrentPart(0) = CurrentPart(0) + 11;
						break;
					}			
				}
			break;
			
			case PART_ELKA2:
				if(!(Keys & 1<<KEY_A))
				{
					switch(CurrentRotate)
					{
						case 0:
							CurrentPart(3) = CurrentPart(3) + 20;
							CurrentPart(2) = CurrentPart(2) + 9;
							CurrentPart(0) = CurrentPart(0) - 9;
						break;
						
						case 1:
							CurrentPart(3) = CurrentPart(3) - 2;
							CurrentPart(2) = CurrentPart(2) - 11;
							CurrentPart(0) = CurrentPart(0) + 11;					
						break;
						
						case 2:
							CurrentPart(3) = CurrentPart(3) - 20;
							CurrentPart(2) = CurrentPart(2) - 9;
							CurrentPart(0) = CurrentPart(0) + 9;	
						break;
						
						case 3:
							CurrentPart(3) = CurrentPart(3) + 2;
							CurrentPart(2) = CurrentPart(2) + 11;//plus
							CurrentPart(0) = CurrentPart(0) - 11;//- 9 
						break;
					}
				}
				else
				{
					switch(CurrentRotate)
					{
						case 0:
							CurrentPart(3) = CurrentPart(3) + 2;
							CurrentPart(2) = CurrentPart(2) + 11;//plus
							CurrentPart(0) = CurrentPart(0) - 11;//- 9 
						break;
						
						case 1:
							CurrentPart(3) = CurrentPart(3) + 20;
							CurrentPart(2) = CurrentPart(2) + 9;
							CurrentPart(0) = CurrentPart(0) - 9;
						break;
						
						case 2:
							CurrentPart(3) = CurrentPart(3) - 2;
							CurrentPart(2) = CurrentPart(2) - 11;
							CurrentPart(0) = CurrentPart(0) + 11;					
						break;
						
						case 3:
							CurrentPart(3) = CurrentPart(3) - 20;
							CurrentPart(2) = CurrentPart(2) - 9;
							CurrentPart(0) = CurrentPart(0) + 9;	
						break;
					}				
				}
			break;
			
			case PART_ZIGZAK1:
				if(CurrentRotate%2)
				{
					CurrentPart(1) = CurrentPart(1) - 12;
					CurrentPart(2) = CurrentPart(2) - 10;			
				}
				else
				{
					CurrentPart(1) = CurrentPart(1) + 12;
					CurrentPart(2) = CurrentPart(2) + 10;			
				}
			break;
			
			case PART_ZIGZAK2:
				if(CurrentRotate%2)
				{
					CurrentPart(0) = CurrentPart(0) - 8;
					CurrentPart(3) = CurrentPart(3) - 10;			
				}
				else
				{
					CurrentPart(0) = CurrentPart(0) + 8;
					CurrentPart(3) = CurrentPart(3) + 10;			
				}
			break;
		}
		
		for(unsigned char i=0; i<4; ++i)
		{
			if((TempPart[i]%10 == 9) || ( (TempPart[i]%10 == 8) && (CurrentPartNumber == PART_PALKA)))
			{
				action = 1;
				i = 100;
			}
			else if(TempPart[i]%10 == 0)
			{
				action = 2;
				i = 100;
			}
		}
		
		if(action == 1)
		{
			for(unsigned char i=0; i<4; ++i)
			{
				if((CurrentPart(i)%10) == 0)
				{
					CurrentPart(0) = TempPart[0];
					CurrentPart(1) = TempPart[1];
					CurrentPart(2) = TempPart[2];
					CurrentPart(3) = TempPart[3];
					CurrentRotate  = TempRotate;
					return;			
				}
			}
		}
		else if(action == 2)
		{
			for(unsigned char i=0; i<4; ++i)
			{
				if((CurrentPart(i)%10) == 9)
				{
					CurrentPart(0) = TempPart[0];
					CurrentPart(1) = TempPart[1];
					CurrentPart(2) = TempPart[2];
					CurrentPart(3) = TempPart[3];
					CurrentRotate  = TempRotate;
					return;			
				}
			}
		}
			
		for(unsigned char i=0; i<4; ++i)
		{
			if(Data[CurrentPart(i)])
			{
				CurrentPart(0) = TempPart[0];
				CurrentPart(1) = TempPart[1];
				CurrentPart(2) = TempPart[2];
				CurrentPart(3) = TempPart[3];
				CurrentRotate  = TempRotate;
				return;
			}
		}
		ena = 1;
	}
	
	if((!CHECK_KEY_A) && (!CHECK_KEY_B) && ena == 1)
		ena = 0;
}

static inline void CheckCompleteLines(void)
{
	register unsigned char *ptr  = &Data[150];
	register unsigned char *ptr2 = &Data[140];
	register unsigned char *ptr3;
	register unsigned char lines = 0;
	register unsigned char i;
	
	while(ptr2 > Data)
	{
		for(i=0; i<10 ; ++i)
		{
			if(!(*(ptr++)))
				i = 20;
		}

		ptr = ptr2 + 10;
		ptr3 = ptr2;
	
		if(i < 20)
		{
			while(ptr2 > Data)//>=
			{
				for(i=0; i<10; ++i)
					*(ptr++) = *(ptr2++);
				
				ptr  = ptr - 20;
				ptr2 = ptr2 - 20;
			}
			
			++lines;
			ptr3 = &Data[150];
		}
		
		ptr = ptr3;
		ptr2 = ptr3 - 10;
	}
	
	if(!lines)
		return;
	
	score2 += lines;
	score  += lines*lines;
	DisplayFullScore(15, 9,score);
	DisplayFullScore(15, 65, score2);
	GenerateSoundWait(25, 3);
	GenerateSoundKwik();
	GenerateSound(40, 1);
}

static inline void MoveDownFaster(void)
{
	if(CHECK_KEY_DOWN && GameFaster == 0)
	{
		GameSpeed = GAME_SPEED_FAST;
		GameFaster = 1;
	}
	
	if((!CHECK_KEY_DOWN) && GameFaster == 1)
	{
		GameSpeed = GAME_SPEED_NORMAL;
		GameFaster = 0;
	}
}

static inline void MoveWorld(void)
{
	unsigned char value, i;
	
	ClearPart();
	CheckPad();
	RotatePart();
	CreatePart();
	DrawBlocks();
	MoveDownFaster();

	if(TimerTick >= GameSpeed)
	{
		GenerateSound(39, 1);
		ClearPart();
		CheckCompleteLines();
		
		for(i=0; i<4; ++i)
		{
			value = CurrentPart(i) + 10;
			if(Data[value] > 0)
			{
				CreatePart();
				PlaceNewPart();
				DrawBlocks();	
				TimerTick = 0;
				
				return;
			}
		}

		for(i=0; i<4; ++i)
			CurrentPart(i) = CurrentPart(i) + 10;
			
		CreatePart();

		for(i=0; i<4; ++i)
		{
			value = CurrentPart(i);
			if(value >= 150 && value <= 159)
			{
				PlaceNewPart();
				break;
			}
		}

		DrawBlocks();	
		TimerTick = 0;
	}
}

void TetrisGame(void)
{
	for(;;)
	{
		DisplayImageFull((prog_char*)&tetrislogo);
		while(!CHECK_KEY_START){};
		ClearScreen();
		GenerateSoundKwik();

		DisplayImageXY((prog_char*)&scorepic, 8, 13, 8, 16);
		DisplayImageXY((prog_char*)&lines, 8, 42, 8, 16);
		DisplayFullScore(9, 22, ReadScore(ADDRESS_TETRIS_RECORD));
		DisplayFullScore(9, 51, ReadScore(ADDRESS_TETRIS_LINES));
		
		Wait(110);
		ClearScreen();
		
		for(unsigned char i=0; i<180; ++i)
			Data[i] = 0;
		
		GameSpeed   = GAME_SPEED_NORMAL;
		GameFaster  = 0;
		MoveElement = 0;
		score       = 0;
		score2      = 0;
		game        = 1;
		
		DrawBorder();
		srand(TCNT2);
		PartNumber = rand()%7;
		DrawBlocks();
		DisplayImageXY((prog_char*)&scorepic, 14, 0, 8, 16);
		DisplayImageXY((prog_char*)&next, 14, 22, 7, 29);
		DisplayImageXY((prog_char*)&lines, 14, 56, 8, 16);
		PlaceNewPart();
		DisplayFullScore(15, 9, score);
		DisplayFullScore(15, 65, score2);
			
		while(game)
		{
			MoveWorld();
			
			if(CHECK_KEY_START)
			{
				for(unsigned char y=1; y<=16; ++y)
				{
					for(unsigned char x=1; x<=10; ++x)
						DrawBlock(0, x, y);
				}
				GenerateSoundKwik();
				while(CHECK_KEY_START){};
				Wait(40);
				while(!CHECK_KEY_START){};
				while(CHECK_KEY_START){};
				GenerateSoundKwik();
				TimerTick  = 0;
				TimerTick2 = 0;
				DrawBlocks();
			}

		}
		
		Wait(80);
		
		if(score > ReadScore(ADDRESS_TETRIS_RECORD))
			WriteScore(score, ADDRESS_TETRIS_RECORD);
		if(score2 > ReadScore(ADDRESS_TETRIS_LINES))
			WriteScore(score2, ADDRESS_TETRIS_LINES);
			
		DrawGameOverScreen();
	}
}