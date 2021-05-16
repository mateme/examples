#include "player.h"
#include "wavdec.h"
#include "hdr_dma.h"
#include "i2s.h"
#include "uda1380.h"
#include "hardware.h"
#include "graphics.h"
#include "options.h"
#include "alarm.h"

#include "config.h"
#include "printf.h"

#include "radiobuffer.h"
#include "listsort.h"
#include "filelist.h"
#include "backupreg.h"
#include "stations.h"
#include "shoutcast.h"

#include <string.h>
#include <wchar.h>
#include <stdlib.h>

#include "uip.h"

#define PLAYER_DEBUG_ENABLE 1

#ifdef PLAYER_DEBUG_ENABLE
    #define PLAYERDEBUG( ... )		printf_(__VA_ARGS__)
#else
    #define PLAYERDEBUG( ... )		//printf_(__VA_ARGS__)
#endif

#ifdef PLAYER_DEBUG_ENABLE
const char *PlayerMP3Errors[]={
	"INDATA_UNDERFLOW",//-1
	"MAINDATA_UNDERFLOW",//-2
	"FREE_BITRATE_SYNC",//...
	"OUT_OF_MEMORY",
	"NULL_POINTER",
	"INVALID_FRAMEHEADER",
	"INVALID_SIDEINFO",
	"INVALID_SCALEFACT",
	"INVALID_HUFFCODES",
	"INVALID_DEQUANTIZE",
	"INVALID_IMDCT",
	"INVALID_SUBBAND"};
#endif

xSemaphoreHandle xPlayerSemaphore;
xSemaphoreHandle xEquSemaphore;
xTaskHandle xPlayerTaskHandle;

extern xTaskHandle xEthernetTaskHandle;


//copy do tego pierwszego dodano volatile
volatile AUDIO_PlaybackBuffer_Status BufferStatus; /* Status of buffer */
s16 AudioBuffer[4608];          /* Playback buffer - Value must be 4608 to hold 2xMp3decoded frames */

unsigned int EquBuffer[64];

static HMP3Decoder hMP3Decoder; /* Content is the pointers to all buffers and information for the MP3 Library */
MP3FrameInfo mp3FrameInfo;      /* Content is the output from MP3GetLastFrameInfo,
                                   we only read this once, and conclude it will be the same in all frames
                                   Maybe this needs to be changed, for different requirements */
volatile uint32_t bytesLeft;    /* Saves how many bytes left in readbuf	*/
volatile uint32_t outOfData;    /* Used to set when out of data to quit playback */
volatile AUDIO_PlaybackBuffer_Status  audio_buffer_fill ;
AUDIO_Playback_status_enum AUDIO_Playback_status ;
AUDIO_Playback_mode_enum AUDIO_Playback_mode = CONTINUOUS;
AUDIO_Format_enum AUDIO_Format ;
AUDIO_Playback_navigation_enum AUDIO_Playback_navigation;
AUDIO_Player_status_enum       AUDIO_Player_status = PLAYER_SUSPEND;
s8  *Audio_buffer;
uint8_t readBuf[READBUF_SIZE];       /* Read buffer where data from SD card is read to */
uint8_t *readPtr;                    /* Pointer to the next new data */
/* file system*/
//FATFS fs;                   /* Work area (file system object) for logical drive */
FIL mp3FileObject;          /* file objects */
FRESULT res;                /* FatFs function common result code	*/
UINT n_Read;                /* File R/W count */

uint32_t MP3_Data_Index;

static AUDIO_PlaybackBuffer_Status AUDIO_PlaybackBuffer_GetStatus(AUDIO_PlaybackBuffer_Status value);
//static void AUDIO_Playback_Stop(void);
static int FillBuffer(FIL *FileObject , uint8_t start);
static int FillBufferFromStream(uint8_t start);
static int FillBufferFromFlash(uint8_t start);
static void AUDIO_Init_audio_mode(AUDIO_Length_enum length, uint16_t frequency, AUDIO_Format_enum format);
static void PlayAudioFile(FIL *FileObject,char *path);//, FILINFO *info);
static void PlayAudioStream(void);
static void PlayAudioAlarm(void);
static void ReduceUnusedSpaces(char *str);
static int  PathCmp(char *str1, char *str2);

static FIL fileR;
static FILINFO info;
static DIR currentDir;

static unsigned int NumberOfMusicFilesInFolder = 0;
static unsigned int CurrentMusicFileInFolder   = 1;
static unsigned int CurrentFileStructureLevel  = 0;
static unsigned int NumberOfMusicFileToPlay    = 1;

static unsigned int AlarmMusicFile             = 1;
static unsigned char AlarmFile                 = 0;
static signed char DecodeErrors;

static unsigned char FileType = FILE_TYPE_UNKNOWN;

//static char *ExtPlayerPath;
char PlayerPath[PLAYER_PATH_LEN]="";

static int FileStructure[STRUCTURE_ENTRIES];
static int MaxFileStructure[STRUCTURE_ENTRIES];

FRESULT ScanSDCard(char *path);

#if _USE_LFN
	static char lfilename[_MAX_LFN + 1];
	//static char prevfilename[_MAX_LFN + 1];
	//static char tmpbuf[_MAX_LFN + 1];
	//static char diskfilename[8+5];
#endif

void vPlayerTask(void * pvArg)
{
	pvArg = pvArg;

	//Clear output buffer
	memset(AudioBuffer,0,sizeof(AudioBuffer));

    RedLEDPWM(RED_LED_DISABLE);
	GreenLEDPWM(GREEN_LED_CONTINUOUS);

	for(;;)
	{
		#if _USE_LFN
			info.lfname =  lfilename;
			info.lfsize = sizeof(lfilename);
		#endif

		switch(AUDIO_Player_status)
		{
			case PLAYER_MUSIC:
			    vhUDA1380SetMixerVolume(UDA1380_MIXER_VOLUME_MAX);
			    GreenLEDPWM(GREEN_LED_BLINK);
				PLAYERDEBUG("Music player\r\n");
				AlarmFile = 0;
				CurrentFileStructureLevel = 0;
                memset(FileStructure, 0, sizeof(FileStructure));
                strcpy(PlayerPath, "");

                if(GetPlayerStateSettings())
                {
                    if(f_open(&fileR, "PLAYER.INI", FA_READ | FA_OPEN_EXISTING) == FR_OK)
                    {
                        char *ch, *zr;
                        //Dummy read
                        f_gets(PlayerPath, sizeof(PlayerPath), &fileR);
                        //Get file number
                        f_gets(PlayerPath, sizeof(PlayerPath), &fileR);

                        ch = strchr(PlayerPath, '=');

                        if(ch++ != NULL)
                        {
                            zr = strchr(PlayerPath, '\r');

                            if(zr != NULL)
                                *zr = 0;
                            AlarmMusicFile = atoi(ch);

                            PLAYERDEBUG("Music file from PLAYER.INI: %d\r\n", AlarmMusicFile);
                        }

                        //Dummy reads
                        f_gets(PlayerPath, sizeof(PlayerPath), &fileR);
                        f_gets(PlayerPath, sizeof(PlayerPath), &fileR);
                        f_gets(PlayerPath, sizeof(PlayerPath), &fileR);

                        //Get file path
                        f_gets(PlayerPath, sizeof(PlayerPath), &fileR);

                        ch = strchr(PlayerPath, '=');

                        if(ch != NULL)
                        {
                            strcpy(FilePath, ++ch);
                            ch = strchr(FilePath, '\r');

                            if(ch != NULL)
                                *ch = 0;

                            PLAYERDEBUG("%s\r\n", FilePath);
                        }

                        strcpy(PlayerPath, "");
                        AUDIO_Playback_navigation = FILELIST;
                        AlarmFile                 = 1;

                        f_close(&fileR);
                    }
                }

				do{
					ScanSDCard(PlayerPath);
				}while((AUDIO_Playback_mode == REPEAT_ALL && (AUDIO_Playback_navigation == NONE || AUDIO_Playback_navigation == NEXT)) || AUDIO_Playback_navigation == FILELIST);

                //PLAYERDEBUG("")
				if(AUDIO_Player_status != PLAYER_SAVE)
				{
					strcpy(PlayerPath, "");
					PLAYERDEBUG("Save player status\r\n");
					AUDIO_Player_status = PLAYER_SUSPEND;
				}
			break;

			case PLAYER_SAVE:
				f_close(&fileR);

                if(GetPlayerStateSettings())
                {
                    if(f_open(&fileR, "PLAYER.INI", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK)
                    {
                        f_puts("[MUSIC PLAYER SETTINGS]\r\n", &fileR);

                        f_printf(&fileR, "FileNumber=%d\r\n",   ReturnCurrentFileNumber());
                        f_printf(&fileR, "NumOfFiles=%d\r\n",   ReturnNumOfFilesInFolder());
                        f_printf(&fileR, "FileTime=%d\r\n",     mp3_info.position);
                        f_printf(&fileR, "FileDuration=%d\r\n", mp3_info.duration);
                        f_printf(&fileR, "FilePath=%s\r\n",     PlayerPath);
                        f_close(&fileR);
                        PLAYERDEBUG("File saved\r\n");
                    }
                }

				//Clear player path to start from begin
				strcpy(PlayerPath, "");

				AUDIO_Player_status = PLAYER_SUSPEND;
			break;

			case PLAYER_RADIO:
			    GreenLEDPWM(GREEN_LED_BLINK);
				PLAYERDEBUG("Radio\r\n");
				PlayAudioStream();
			break;

			case PLAYER_ALARM:
                PLAYERDEBUG("Alarm\r\n");

                switch(GetAlarmSound())
                {
                    RadioStationStruct RadioStationData;

                    case 5:
                        //TODO: Add here wait time and timeout for address catching if DHCP is enable
                        AUDIO_Playback_Stop();
                        AUDIO_Player_status   = PLAYER_SUSPEND;
                        CloseShoutcastConnection();

                        if(GetDHCPSettings())
                        {
                            int t;

                            for(t=0; t<50; ++t)
                            {
                                uip_ipaddr_t addr;
                                uip_gethostaddr(addr);

                                if(addr[0] != 0 && addr[1] != 0)
                                    break;

                                vTaskDelay(100);
                            }

                            if(t == 50)
                            {
                                if(AlarmLoadSound(0))
                                    PlayAudioAlarm();

                                IRQOption       = ActiveOption;
                                ActiveOption    = DrawOptionAlarm;

                                SetLCDBacklightMode(BACKLIGHT_MODE_PULSE);
                                BacklightState  = BACKLIGHT_PULSE;

                                break;
                            }
                        }

                        ResetArtistAndTitleOffset();
                        strcpy(mp3_info.artist, "");
                        strcpy(mp3_info.title, "");

                        RadioBufferInit();
                        InitShoutcastBuffer(mp3_info.artist, 40);
                        GetStationData(0, &RadioStationData);
                        ConnectToRadioStation(&RadioStationData);
                        ActiveOption    = DrawOptionConnection;
                        DrawFullMenu_bb = 1;
                    break;

                    case 4:
                        if(f_open(&fileR, "ALARM.INI", FA_READ | FA_OPEN_EXISTING) == FR_OK)
                        {
                            char *ch, *zr;

                            //Dummy read
                            f_gets(PlayerPath, sizeof(PlayerPath), &fileR);
                            //Get file number
                            f_gets(PlayerPath, sizeof(PlayerPath), &fileR);

                            ch = strchr(PlayerPath, '=');

                            if(ch++ != NULL)
                            {
                                zr = strchr(PlayerPath, '\r');

                                if(zr != NULL)
                                    *zr = 0;
                                AlarmMusicFile = atoi(ch);

                                PLAYERDEBUG("Music file: %d\r\n", AlarmMusicFile);
                            }

                            //Get file path
                            f_gets(PlayerPath, sizeof(PlayerPath), &fileR);

                            ch = strchr(PlayerPath, '=');

                            if(ch != NULL)
                            {
                                strcpy(FilePath, ++ch);
                                ch = strchr(FilePath, '\r');

                                if(ch != NULL)
                                    *ch = 0;

                                PLAYERDEBUG("%s\r\n", FilePath);
                            }

                            strcpy(PlayerPath, "");

                            AUDIO_Player_status       = PLAYER_MUSIC;
                            AUDIO_Playback_navigation = FILELIST;
                            AlarmFile                 = 1;

                            f_close(&fileR);
                        }
                        else
                        {
                            PLAYERDEBUG("No ALARM.INI load default sound\r\n");
                            if(AlarmLoadSound(0))
                            {
                                PLAYERDEBUG("Play alarm audio\r\n");
                                PlayAudioAlarm();
                            }
                            else
                            {
                                PLAYERDEBUG("Reading alarm sound from flash error\r\n");
                                AUDIO_Player_status = PLAYER_SUSPEND;
                            }
                        }
                    break;

                    default:
                        PLAYERDEBUG("Load alarm sound id: %d\r\n", GetAlarmSound());
                        if(AlarmLoadSound(GetAlarmSound()))
                        {
                            PLAYERDEBUG("Play alarm audio\r\n");
                            PlayAudioAlarm();
                        }
                        else
                        {
                            PLAYERDEBUG("Reading alarm sound from flash error\r\n");
                            AUDIO_Player_status = PLAYER_SUSPEND;
                        }
                    break;
                }
            break;

			default:
			case PLAYER_SUSPEND:
			    GreenLEDPWM(GREEN_LED_CONTINUOUS);
				PLAYERDEBUG("Suspend\r\n");
				vTaskSuspend( NULL ); //Suspend ourselves
			break;
		}
	}
}

AUDIO_PlaybackBuffer_Status AUDIO_PlaybackBuffer_GetStatus(AUDIO_PlaybackBuffer_Status value)
{
    if ( value )
        audio_buffer_fill &= ~value;
    return audio_buffer_fill;
}


void AUDIO_Playback_Stop(void)
{
	AUDIO_Playback_status = STOP;
	memset(AudioBuffer,0,sizeof(AudioBuffer));
	MP3_Data_Index = 0;
}

int FillBuffer(FIL *FileObject , uint8_t start)
{
    uint8_t word_Allignment; // Variable used to make sure DMA transfer is alligned to 32bit
    int err=0;               // Return value from MP3decoder
    int offset;              // Used to save the offset to the next frame

    //If we are in start position we overwrite bufferstatus
    if(!start)
        BufferStatus = AUDIO_PlaybackBuffer_GetStatus( (AUDIO_PlaybackBuffer_Status) 0 );
    else
        BufferStatus = (AUDIO_PlaybackBuffer_Status) ( LOW_EMPTY | HIGH_EMPTY );

        //somewhat arbitrary trigger to refill buffer - should always be enough for a full frame
		if (bytesLeft < 2*MAINBUF_SIZE) //&& !eofReached
		{
          // move last, small chunk from end of buffer to start, then fill with new data
          word_Allignment = 4 - (bytesLeft % 4 );   // Make sure the first byte in writing pos. is word alligned
          memmove(readBuf+word_Allignment, readPtr, bytesLeft);
          res = f_read( FileObject, (uint8_t *)readBuf+word_Allignment + bytesLeft,READBUF_SIZE - bytesLeft - word_Allignment,&n_Read);

		  if( n_Read !=0 && res != FR_TIMEOUT)
		  {
		     MP3_Data_Index += n_Read;
             //zero-pad to avoid finding false sync word after last frame (from old data in readBuf)
             if (n_Read < READBUF_SIZE - bytesLeft - word_Allignment)
             {
		        memset(readBuf + bytesLeft + n_Read, 0, READBUF_SIZE - bytesLeft - n_Read);
		     }
             bytesLeft += n_Read;
		     readPtr = readBuf+word_Allignment;
		  }
		  else
		  {
		     outOfData = 1;
		  }
		}

        if(outOfData == 1)
        {
            // Stop Playback
            AUDIO_Playback_Stop();
            return 0;
        }

		// find start of next MP3 frame - assume EOF if no sync found
		offset = MP3FindSyncWord(readPtr, bytesLeft);
		if (offset < 0)
		{
		    bytesLeft = 0;
		    return -1;
		}

		readPtr += offset;			  // data start point
		bytesLeft -= offset;		  // in buffer
		if (BufferStatus & LOW_EMPTY) // Decode data to low part of buffer
        {
           err = MP3Decode(hMP3Decoder, &readPtr, (int *)&bytesLeft, (short *)AudioBuffer, 0);
  		   BufferStatus = AUDIO_PlaybackBuffer_GetStatus(LOW_EMPTY);
        }
 		else if (BufferStatus & HIGH_EMPTY)     // Decode data to the high part of buffer
        {
		   err = MP3Decode(hMP3Decoder, &readPtr, (int *)&bytesLeft, (short *)AudioBuffer+mp3FrameInfo.outputSamps,0);
		   BufferStatus = AUDIO_PlaybackBuffer_GetStatus(HIGH_EMPTY);
        }
        if (err!=0)
        {
            MP3DecInfo *dec = hMP3Decoder;
            bytesLeft=0;
            readPtr = readBuf;
            err = err*(-1) - 1;

            if(err > 0 && err <= 11)
                PLAYERDEBUG("Fill buffer->file: %s\n\r", PlayerMP3Errors[err]);

            PLAYERDEBUG("SubbanInfoPS: %x\n\r", dec->SubbandInfoPS);

            if(start)
                ++DecodeErrors;

			return -1;
        }

  return 0;
}

int FillBufferFromStream(uint8_t start)
{
    uint8_t word_Allignment; // Variable used to make sure DMA transfer is alligned to 32bit
    int err=0;               // Return value from MP3decoder
    int offset;              // Used to save the offset to the next frame
    unsigned char volumetmp;

    //If we are in start position we overwrite bufferstatus
    if(!start)
        BufferStatus = AUDIO_PlaybackBuffer_GetStatus( (AUDIO_PlaybackBuffer_Status) 0 );
    else
        BufferStatus = (AUDIO_PlaybackBuffer_Status) ( LOW_EMPTY | HIGH_EMPTY );

    //somewhat arbitrary trigger to refill buffer - should always be enough for a full frame
    if (bytesLeft < 2*MAINBUF_SIZE) //&& !eofReached
    {
        // move last, small chunk from end of buffer to start, then fill with new data
        word_Allignment = 4 - (bytesLeft % 4 );   // Make sure the first byte in writing pos. is word alligned
        memmove(readBuf+word_Allignment, readPtr, bytesLeft);

        if((int)(RadioPlayerPosition+READBUF_SIZE - bytesLeft - (int)word_Allignment) >= ServerPosition)
        {
            int sub = 0;
            int repeats = 0;

            volumetmp = (unsigned char)vhUDA1380GetVolume();
            vhUDA1380SetVolumeNoBackup(252);

            //Load more data from server
            memset(AudioBuffer,0,sizeof(AudioBuffer));
            BufferStatus = (AUDIO_PlaybackBuffer_Status) ( LOW_EMPTY | HIGH_EMPTY );

            while((int)(RadioPlayerPosition+READBUF_SIZE - bytesLeft - (int)word_Allignment+(RADIOBUFFER_SIZE/2)) >= ServerPosition)
            {
                DrawParameter6_bb = 1;
                vTaskDelay(20);

                //Try to obtain that server don't send new data. If we don't have any data for desired time try to connect again.
                if(sub == (ServerPosition - RadioPlayerPosition))
                    ++repeats;

                sub = ServerPosition - RadioPlayerPosition;

                if(repeats >= 40)
                {
                    RadioStationStruct RadioStationData;
                    PLAYERDEBUG("Reconnect to server\r\n");

                    AUDIO_Playback_Stop();
                    AUDIO_Player_status   = PLAYER_SUSPEND;

                    ShoutcastKillConnection();

                    //AbortShoutcastConnection();
                    ActiveOption    = DrawOptionConnection;
                    DrawFullMenu_bb = 1;

                    //uip_init();
                    uip_arp_init();
                    //PLAYERDEBUG("uIP Init OK\r\n");

                    ResetArtistAndTitleOffset();
                    strcpy(mp3_info.artist, "");
                    strcpy(mp3_info.title, "");

                    vTaskDelay(50);

                    RadioBufferInit();
                    InitShoutcastBuffer(mp3_info.artist, 40);
                    GetStationData(ReturnCurrentStationAddress(), &RadioStationData);
                    ConnectToRadioStation(&RadioStationData);
                    return -1;
                }

                if(AUDIO_Playback_status != IS_PLAYING)
                    break;
            }

            vhUDA1380SetVolumeNoBackup(volumetmp);
        }

        n_Read = READBUF_SIZE - bytesLeft - word_Allignment;
        RadioBufferLoadData((char*)(readBuf+word_Allignment + bytesLeft), n_Read);

        MP3_Data_Index += n_Read;

        //zero-pad to avoid finding false sync word after last frame (from old data in readBuf)
        if (n_Read < READBUF_SIZE - bytesLeft - word_Allignment)
        {
            memset(readBuf + bytesLeft + n_Read, 0, READBUF_SIZE - bytesLeft - n_Read);
        }

        bytesLeft += n_Read;
        readPtr = readBuf+word_Allignment;
    }

    // find start of next MP3 frame - assume EOF if no sync found
    offset = MP3FindSyncWord(readPtr, bytesLeft);
    if (offset < 0)
    {
        bytesLeft = 0;
        return -1;
    }

    readPtr += offset;			  // data start point
    bytesLeft -= offset;		  // in buffer

    if (BufferStatus & LOW_EMPTY) // Decode data to low part of buffer
    {
        err = MP3Decode(hMP3Decoder, &readPtr, (int *)&bytesLeft, (short *)AudioBuffer, 0);
        BufferStatus = AUDIO_PlaybackBuffer_GetStatus(LOW_EMPTY);
    }
    else if (BufferStatus & HIGH_EMPTY)     // Decode data to the high part of buffer
    {
        err = MP3Decode(hMP3Decoder, &readPtr, (int *)&bytesLeft, (short *)AudioBuffer+mp3FrameInfo.outputSamps,0);
        BufferStatus = AUDIO_PlaybackBuffer_GetStatus(HIGH_EMPTY);
    }

    if (err != 0)
    {
        bytesLeft=0;
        readPtr = readBuf;
        err = err*(-1) - 1;

        if(err > 0 && err <= 11)
            PLAYERDEBUG("Fill buffer->stream: %s\n\r", PlayerMP3Errors[err]);

        if(start)
            ++DecodeErrors;

        return -1;
    }

    return 0;
}

int FillBufferFromFlash(uint8_t start)
{
    uint8_t word_Allignment; // Variable used to make sure DMA transfer is alligned to 32bit
    int err=0;               // Return value from MP3decoder
    int offset;              // Used to save the offset to the next frame

    //If we are in start position we overwrite bufferstatus
    if(!start)
        BufferStatus = AUDIO_PlaybackBuffer_GetStatus( (AUDIO_PlaybackBuffer_Status) 0 );
    else
        BufferStatus = (AUDIO_PlaybackBuffer_Status) ( LOW_EMPTY | HIGH_EMPTY );

        //somewhat arbitrary trigger to refill buffer - should always be enough for a full frame
		if (bytesLeft < 2*MAINBUF_SIZE) //&& !eofReached
		{
          // move last, small chunk from end of buffer to start, then fill with new data
          word_Allignment = 4 - (bytesLeft % 4 );   // Make sure the first byte in writing pos. is word alligned
          memmove(readBuf+word_Allignment, readPtr, bytesLeft);
          //res = f_read( FileObject, (uint8_t *)readBuf+word_Allignment + bytesLeft,READBUF_SIZE - bytesLeft - word_Allignment,&n_Read);
          n_Read = READBUF_SIZE - bytesLeft - word_Allignment;
          AlarmGetData((char*)(readBuf+word_Allignment + bytesLeft), n_Read);


            MP3_Data_Index += n_Read;
             //zero-pad to avoid finding false sync word after last frame (from old data in readBuf)
             /*if (n_Read < READBUF_SIZE - bytesLeft - word_Allignment)
             {
		        memset(readBuf + bytesLeft + n_Read, 0, READBUF_SIZE - bytesLeft - n_Read);
		     }*/
             bytesLeft += n_Read;
		     readPtr = readBuf+word_Allignment;
		}

        /*if(outOfData == 1)
        {
            // Stop Playback
            AUDIO_Playback_Stop();
            return 0;
        }*/

		// find start of next MP3 frame - assume EOF if no sync found
		offset = MP3FindSyncWord(readPtr, bytesLeft);
		if (offset < 0)
		{
		    bytesLeft = 0;
		    return -1;
		}

		readPtr += offset;			  // data start point
		bytesLeft -= offset;		  // in buffer
		if (BufferStatus & LOW_EMPTY) // Decode data to low part of buffer
        {
           err = MP3Decode(hMP3Decoder, &readPtr, (int *)&bytesLeft, (short *)AudioBuffer, 0);
  		   BufferStatus = AUDIO_PlaybackBuffer_GetStatus(LOW_EMPTY);
        }
 		else if (BufferStatus & HIGH_EMPTY)     // Decode data to the high part of buffer
        {
		   err = MP3Decode(hMP3Decoder, &readPtr, (int *)&bytesLeft, (short *)AudioBuffer+mp3FrameInfo.outputSamps,0);
		   BufferStatus = AUDIO_PlaybackBuffer_GetStatus(HIGH_EMPTY);
        }
        if (err!=0)
        {
            bytesLeft=0;
            readPtr = readBuf;
			PLAYERDEBUG("MP3Err: %d\n\r", err);
			//++DecodeErrors;

            //error occurred
            /*switch (err) // There is not implemeted any error handling. it will quit the playing in case of error
            {
                // do nothing - next call to decode will provide more mainData
                default:
				  return -1;
            }*/
			return -1;
        }

  return 0;
}

void AUDIO_Init_audio_mode(AUDIO_Length_enum length, uint16_t frequency, AUDIO_Format_enum format)
{
    unsigned char volumetmp;
   //The MONO mode uses working buffer to duplicate datas on the two channels
   //and switch buffer half by half => uses DMA in circular mode
   mp3_info.length = length;
   mp3_info.sampling = frequency;
   mp3_info.bit_rate = mp3FrameInfo.bitrate;

   AUDIO_Playback_status = STOP;
   AUDIO_Format = format;

    //PLAYERDEBUG("Samp freq: %d\r\n", frequency);
   //Buffers are supposed to be empty here
   audio_buffer_fill = (AUDIO_PlaybackBuffer_Status) ( LOW_EMPTY | HIGH_EMPTY ) ;

   if(AUDIO_Format == STEREO)
        vhUDA1380SetSamplingFreq(frequency);
   else
        vhUDA1380SetSamplingFreq(frequency/2);

    volumetmp = (unsigned char)vhUDA1380GetVolume();
    vhUDA1380SetVolume(252);

   //Clear output buffer
   memset(AudioBuffer,0,sizeof(AudioBuffer));

   //Start DMA transfer
   I2S3DMATransfer((const unsigned short*)AudioBuffer, (mp3FrameInfo.outputSamps*2));

   vhUDA1380SetVolume(volumetmp);

   DrawParameter29_bb = 1;
}

void PlayAudioFile(FIL *FileObject,char *path)//, FILINFO *info)
{
    unsigned char volumetmp;

	//Try open file
	res = f_open(FileObject , path , FA_OPEN_EXISTING | FA_READ);

	if(res != FR_OK)
	{
		PLAYERDEBUG("File open error %d\r\nPath: %s\r\n", res, path);
		f_close(FileObject);
		return;
	}

	//CheckFileType(path);

	//Reset counters
	bytesLeft    = 0;
	outOfData    = 0;
  	readPtr      = readBuf;
  	DecodeErrors = 0;

  	hMP3Decoder = MP3InitDecoder();

  	if(hMP3Decoder == 0)
    {
        f_close(FileObject);
        return;
    }

  	mp3FileObject = *FileObject;

  	memset(&mp3_info,0,sizeof(mp3_info));
  	Read_ID3V2(FileObject,&mp3_info);
  	Read_ID3V1(FileObject,&mp3_info);

  	//If we don't have information about artist and title from tags place file name as artist
  	if((*(mp3_info.artist) == 0) && (*(mp3_info.title) == 0))
  	{
  		char *string;
  		PLAYERDEBUG("No artist and title info!\r\n");

        string = strrchr(path, '/');
        string++;

  		if(strstr(string,".MP3") != NULL)
  		{
  			int namelen = (int)(strstr(string,".MP3")-string);

  			if(namelen < 39)
  				strncpy(mp3_info.artist, string, namelen);
  			else
  				strncpy(mp3_info.artist, string, 39);
  		}
  		else if(strstr(string,".mp3") != NULL)
  		{
  			int namelen = (int)(strstr(string,".mp3")-string);

  			if(namelen < 39)
  				strncpy(mp3_info.artist, string, namelen);
  			else
  				strncpy(mp3_info.artist, string, 39);
  		}
  	}
  	else
  	{
  		ReduceUnusedSpaces(mp3_info.artist);
		ReduceUnusedSpaces(mp3_info.title);
  	}

	//Reset file info and draw new data
  	DrawParameter30_bb = 1;
	DrawParameter31_bb = 1;
	ResetArtistAndTitleOffset();

  	//Calculate file duration and recalculate it to seconds
  	mp3_info.duration = GetMP3Time(path);

  	PLAYERDEBUG("duration %d \r\n", mp3_info.duration);

    mp3_info.duration = ((mp3_info.duration%1000) >= 500) ? (mp3_info.duration/1000+1) : (mp3_info.duration/1000);

    PLAYERDEBUG("File time: %d:%d\r\n", mp3_info.duration/60, mp3_info.duration%60);

  	MP3_Data_Index = 0;
  	mp3_info.position = -1;

  	res = f_lseek(FileObject,mp3_info.data_start);

	if(res != FR_OK)
	{
	    MP3FreeDecoder(hMP3Decoder);
		f_close(FileObject);
		return;
	}

	//Read the first data to get info about the MP3 File
	while( FillBuffer( FileObject  , 1 ) && DecodeErrors < PLAYER_MAX_DECODE_ERRORS){};

	if(DecodeErrors >= PLAYER_MAX_DECODE_ERRORS)
    {
        PLAYERDEBUG("Too much errors in header reading\r\n");
        goto ExitStreamPlayer;
    }

	//Get the length of the decoded data, so we can set correct play length
	MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);

	//Select the correct samplerate and Mono/Stereo
	AUDIO_Init_audio_mode( ( AUDIO_Length_enum )mp3FrameInfo.bitsPerSample,  \
						((MP3DecInfo *)hMP3Decoder)->samprate,  \
						(((MP3DecInfo *)hMP3Decoder)->nChans==1) ? MONO : STEREO);

    PLAYERDEBUG("Channels: %d\r\n", (((MP3DecInfo *)hMP3Decoder)->nChans));

	//Start the playback
	AUDIO_Playback_status = IS_PLAYING;
	DrawFullMenu_bb = 1;

	if(!vhUDA1380GetMuteStatus())
		vhUDA1380Mute(UDA1380_MUTE_DISABLE);

    PLAYERDEBUG("File Sampling: %d kHz\r\n", mp3_info.sampling);
    PLAYERDEBUG("File Bit rate: %d kHz\r\n", mp3_info.bit_rate);

    vTaskPrioritySet(xPlayerTaskHandle, PLAYER_TASK_PRIORITY+1);

	while( !(outOfData == 1) && (AUDIO_Playback_status == IS_PLAYING || AUDIO_Playback_status == PAUSE))// && (DecodeErrors < 7))
	{
		//Synchronization with IRQ
		if(xSemaphoreTake(xPlayerSemaphore, PLAYER_SEMPHR_WAITTIME ) == pdTRUE)
		{
			s16 *ptr2;
			s16 *ptr3;
			s16 *ptr  = (s16*)&buff;

            FillBuffer(FileObject ,0 );

			switch(audio_buffer_fill)
			{
				case LOW_EMPTY:
					ptr2 =  AudioBuffer;
					ptr3 = &AudioBuffer[1];//1152
				break;

				case HIGH_EMPTY:
					ptr2 = &AudioBuffer[2304];
					ptr3 = &AudioBuffer[2305];//3456
				break;

				default:
					ptr2 =  AudioBuffer;
					ptr3 = &AudioBuffer[1];//2304
				break;
			}

			if( xSemaphoreTake( xEquSemaphore, SPECTRUM_SEMPHR_WAITTIME)== pdTRUE)
			{
				int l;

				for(l=0;l<64;++l)
				{
					//Average two channels to one value
					*ptr++  = *(ptr2)/2 + *(ptr3)/2;
					*ptr++ = 0;

					 ptr2 += 2;
					 ptr3 += 2;
				}

				DrawParameter6_bb = 1;
				xSemaphoreGive(xEquSemaphore);
			}
		}
	}

	ExitStreamPlayer:

	vTaskPrioritySet(xPlayerTaskHandle, PLAYER_TASK_PRIORITY);

    volumetmp = (unsigned char)vhUDA1380GetVolume();
    vhUDA1380SetVolumeNoBackup(252-volumetmp*3/4);
    vTaskDelay(5);
    vhUDA1380SetVolumeNoBackup(252-volumetmp/2);
    vTaskDelay(5);
    vhUDA1380SetVolumeNoBackup(252-volumetmp/4);
    vTaskDelay(5);
    vhUDA1380SetVolumeNoBackup(252);

	AUDIO_Playback_Stop();

	vhUDA1380SetVolumeNoBackup(volumetmp);
	//Release mp3 decoder
	MP3FreeDecoder(hMP3Decoder);
	f_close(FileObject);
}

void PlayAudioStream(void)
{
    int loop = 5;
	PLAYERDEBUG("Play audio stream\r\n");
	//Reset counters
	bytesLeft    = 0;
	outOfData    = 0;
  	readPtr      = readBuf;
  	DecodeErrors = 0;
  	hMP3Decoder  = MP3InitDecoder();

  	if(hMP3Decoder == 0)
        return;

    memset(readBuf, 0, sizeof(readBuf));
    memset(AudioBuffer,0,sizeof(AudioBuffer));

	//Read the first data to get info about the MP3 File
	while(FillBufferFromStream(1) && DecodeErrors < PLAYER_MAX_DECODE_ERRORS){};

	if(DecodeErrors >= PLAYER_MAX_DECODE_ERRORS)
    {
        PLAYERDEBUG("Too much errors in header reading\r\n");

        RadioStationStruct RadioStationData;
        PLAYERDEBUG("Reconnect to server\r\n");

        AUDIO_Playback_Stop();
        AUDIO_Player_status   = PLAYER_SUSPEND;

        ShoutcastKillConnection();

        ActiveOption    = DrawOptionConnection;
        DrawFullMenu_bb = 1;
        ResetArtistAndTitleOffset();
        strcpy(mp3_info.artist, "");
        strcpy(mp3_info.title, "");
        RadioBufferInit();
        InitShoutcastBuffer(mp3_info.artist, 40);
        GetStationData(ReturnCurrentStationAddress(), &RadioStationData);
        ConnectToRadioStation(&RadioStationData);

        goto ExitStreamPlayer;
    }

  	PLAYERDEBUG("Header OK get length of decoded data\r\n");
	//Get the length of the decoded data, so we can set correct play length
	MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);
    PLAYERDEBUG("Bitrate from  shoutcast server: %d\r\n", GetShoutcastBitrate());
    PLAYERDEBUG("Bitrate %d\r\nFreq %d Freq %d\r\n", mp3FrameInfo.bitrate,((MP3DecInfo *)hMP3Decoder)->samprate, mp3FrameInfo.samprate);
	//Select the correct samplerate and Mono/Stereo

	while((GetShoutcastBitrate() != mp3FrameInfo.bitrate || GetShoutcastBitrate() == 0) && loop--)
    {
        mp3FrameInfo.bitrate = GetShoutcastBitrate();
        memset(readBuf, 0, sizeof(readBuf));
        while(FillBufferFromStream(1)); 	 //Must Read MP3 file header information
        PLAYERDEBUG("Bitrate %d\r\nFreq %d Freq %d\r\n", mp3FrameInfo.bitrate,((MP3DecInfo *)hMP3Decoder)->samprate, mp3FrameInfo.samprate);
    }

    if(!loop)
    {
        PLAYERDEBUG("Bitrate mismatch\r\n");
        AUDIO_Playback_Stop();
        //Release mp3 decoder
        MP3FreeDecoder(hMP3Decoder);
        AUDIO_Player_status = PLAYER_SUSPEND;
        return;
    }

	AUDIO_Init_audio_mode( ( AUDIO_Length_enum )mp3FrameInfo.bitsPerSample,  \
						((MP3DecInfo *)hMP3Decoder)->samprate,  \
						(((MP3DecInfo *)hMP3Decoder)->nChans==1) ? MONO : STEREO);

	PLAYERDEBUG("Settings OK\r\n");
	//Start the playback
	AUDIO_Playback_status = IS_PLAYING;
	DrawFullMenu_bb = 1;

	if(!vhUDA1380GetMuteStatus())
		vhUDA1380Mute(UDA1380_MUTE_DISABLE);

    vTaskPrioritySet(xPlayerTaskHandle, PLAYER_TASK_PRIORITY+2);
    vTaskPrioritySet(xEthernetTaskHandle, tskIDLE_PRIORITY + 2+1);

	while((AUDIO_Playback_status == IS_PLAYING))// && (DecodeErrors < 7))
	{
		//Synchronization with IRQ
		if(xSemaphoreTake(xPlayerSemaphore, PLAYER_SEMPHR_WAITTIME) == pdTRUE)
		{
			s16 *ptr2;
			s16 *ptr3;
			s16 *ptr  = (s16*)&buff;

            FillBufferFromStream(0);

			switch(audio_buffer_fill)
			{
				case LOW_EMPTY:
					ptr2 =  AudioBuffer;
					ptr3 = &AudioBuffer[1];
				break;

				case HIGH_EMPTY:
					ptr2 = &AudioBuffer[2304];
					ptr3 = &AudioBuffer[2305];
				break;

				default:
					ptr2 =  AudioBuffer;
					ptr3 = &AudioBuffer[1];
				break;
			}

			if( xSemaphoreTake( xEquSemaphore, SPECTRUM_SEMPHR_WAITTIME)== pdTRUE)
			{
				int l;

				for(l=0;l<64;++l)
				{
					//Average two channels to one value
					*ptr++  = *(ptr2)/2 + *(ptr3)/2;
					*ptr++ = 0;

					 ptr2 += 2;
					 ptr3 += 2;
				}

				DrawParameter6_bb = 1;
				xSemaphoreGive(xEquSemaphore);
			}
		}
	}

	ExitStreamPlayer:

	vTaskPrioritySet(xPlayerTaskHandle, PLAYER_TASK_PRIORITY);
	vTaskPrioritySet(xEthernetTaskHandle, tskIDLE_PRIORITY + 2);

	AUDIO_Playback_Stop();
	//Release mp3 decoder
	MP3FreeDecoder(hMP3Decoder);

	PLAYERDEBUG("Exit radio player\r\n");
}

void PlayAudioAlarm(void)
{
	//Reset counters
	bytesLeft    = 0;
	outOfData    = 0;
  	readPtr      = readBuf;
  	DecodeErrors = 0;
  	hMP3Decoder  = MP3InitDecoder();

    if(hMP3Decoder == 0)
        return;

  	PLAYERDEBUG("Read header\r\n");
	//Read the first data to get info about the MP3 File
	while(FillBufferFromFlash(1)); 	 //Must Read MP3 file header information
  	PLAYERDEBUG("Header OK get length of decoded data\r\n");
	//Get the length of the decoded data, so we can set correct play length
	MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);
	PLAYERDEBUG("Data OK");
	//Select the correct samplerate and Mono/Stereo
	AUDIO_Init_audio_mode( ( AUDIO_Length_enum )mp3FrameInfo.bitsPerSample,  \
						((MP3DecInfo *)hMP3Decoder)->samprate,  \
						(((MP3DecInfo *)hMP3Decoder)->nChans==1) ? MONO : STEREO);

	PLAYERDEBUG("Settings OK\r\n");
	//Start the playback
	AUDIO_Playback_status = IS_PLAYING;
	DrawFullMenu_bb = 1;

	if(!vhUDA1380GetMuteStatus())
		vhUDA1380Mute(UDA1380_MUTE_DISABLE);

    vTaskPrioritySet(xPlayerTaskHandle, PLAYER_TASK_PRIORITY+2);

    PLAYERDEBUG("Play alarm sound\r\n");
	while((AUDIO_Playback_status == IS_PLAYING))
	{
		//Synchronization with IRQ
		if(xSemaphoreTake(xPlayerSemaphore, PLAYER_SEMPHR_WAITTIME) == pdTRUE)
		{
            FillBufferFromFlash(0);
		}
	}

	vTaskPrioritySet(xPlayerTaskHandle, PLAYER_TASK_PRIORITY);

	AUDIO_Playback_Stop();
	//Release mp3 decoder
	MP3FreeDecoder(hMP3Decoder);

	PLAYERDEBUG("Exit alarm player\r\n");
}

FRESULT ScanSDCard(char *path)
{
	res = f_opendir(&currentDir, path);

	if(res == FR_OK)
	{
        int i;
        int len;

		//Count music files in current folder
		NumberOfMusicFilesInFolder = 0;
		CurrentMusicFileInFolder   = -1;

        //Draw --:-- in time
		mp3_info.position = -1;
		mp3_info.duration = -1;

		DrawParameter29_bb = 1;
		DrawParameter30_bb = 1;
		DrawParameter31_bb = 1;

        PLAYERDEBUG("Player path: %s\r\n", PlayerPath);
        PLAYERDEBUG("List path: %s\r\n", FilePath);

        //if(AUDIO_Playback_navigation == FILELIST)
        //PLAYERDEBUG("FileLIST\r\n");

        if((AUDIO_Playback_navigation != FILELIST) || ((AUDIO_Playback_navigation == FILELIST) && (strcmp(PlayerPath,FilePath) == 0)))
        {
            //AUDIO_Playback_navigation = NONE;
        ListSortInit(0);

        //Scan SD card and add music files to list
        while ((f_readdir(&currentDir, &info) == FR_OK) && info.fname[0])
        {
            //PLAYERDEBUG("%s\r\n", info.fname);

            if(CheckFileType(info.fname) < SUPPORTED_MUSIC_FILE)
            {
                ++NumberOfMusicFilesInFolder;

                if(*info.lfname != 0)
                    ListSortAddEntry(info.lfname, strlen(info.lfname), 0);
                else
                    ListSortAddEntry(info.fname, strlen(info.fname), 0);
            }
        }

        //PLAYERDEBUG("Entries in list: %d\r\n", ListSortReturnNumOfEntries());
        ListSortSortData(0);

        for(i=1;i<=ListSortReturnNumOfEntries(0);++i)
        {
            ListSortReturnEntry(i, lfilename, &len, 0);
            lfilename[len] = 0;
            //PLAYERDEBUG("%d. %s\r\n", i, lfilename);
        }

        //If we have some songs...
        if(NumberOfMusicFilesInFolder > 0)
        {
            do{
                if(AlarmFile == 1)
                    CurrentMusicFileInFolder = AlarmMusicFile;
                else if(AUDIO_Playback_navigation == FILELIST)
                    CurrentMusicFileInFolder = FileListReturnCurrentMusicFile();
                else
                    CurrentMusicFileInFolder = 1;

                //AlarmFile                 = 0;
                //AUDIO_Playback_navigation = NONE;

                PLAYERDEBUG("Num of entries: %d\r\n", ListSortReturnNumOfEntries(0));

                while(CurrentMusicFileInFolder <= (unsigned int)ListSortReturnNumOfEntries(0))
                {
                    do{
                        char *ch;

                        strcat(path, "/");
                        i = strlen(path);
                        PLAYERDEBUG("Path to play before add filename: %s\r\n", path);
                        ListSortReturnEntry(CurrentMusicFileInFolder, &path[i], &len, 0);
                        path[i+len] = 0;
                        PLAYERDEBUG("Path to play: %s\r\n", path);
                        AUDIO_Playback_navigation = NEXT;
                        PlayAudioFile(&fileR, path);//, &info);

                        if(AlarmFile)
                        {
                            AlarmFile = 0;
                            AUDIO_Player_status = PLAYER_SUSPEND;
                            break;
                        }

                        //Remove file name from path
                        ch = strrchr(path, '/');

                        if(ch != NULL)
                            *ch = 0;

                        if(AUDIO_Player_status != PLAYER_MUSIC)
                            break;
                    }while(AUDIO_Playback_mode == REPEAT_ONE && AUDIO_Playback_navigation != FILELIST);// && AUDIO_Player_status != PLAYER_MUSIC);
                    //}while(AUDIO_Playback_mode == REPEAT_ONE && AUDIO_Playback_navigation == NONE && AUDIO_Player_status != PLAYER_SAVE);

                    switch(AUDIO_Playback_navigation)
                    {
                        case PREV:
                            if(CurrentMusicFileInFolder > 1)
                                --CurrentMusicFileInFolder;
                            else
                            {
                                if(FileStructure[CurrentFileStructureLevel] > 1)
                                    FileStructure[CurrentFileStructureLevel] -= 1;
                            }
                        break;

                        case FILELIST:
                            //After receive FILELIST navigation status we should back to the ROOT
                            //because current function must take DIR variable from stack.
                            //If we in ROOT we copy path from file manager and then find file.
                            return FR_NO_FILE;
                        break;

                        //case NEXT:
                        default:
                            //If we quit from player option we don't increase file counter to save current playing song
                            if(AUDIO_Player_status != PLAYER_SAVE)
                                ++CurrentMusicFileInFolder;
                        break;
                    }

                    if(AUDIO_Player_status == PLAYER_SAVE || AUDIO_Player_status == PLAYER_SUSPEND)
                        break;
                }

                if(AUDIO_Player_status == PLAYER_SAVE || AUDIO_Player_status == PLAYER_SUSPEND)
                    return FR_NO_FILE;
            }while(AUDIO_Playback_mode == REPEAT_FOLDER && (AUDIO_Playback_navigation == NONE || AUDIO_Playback_navigation == NEXT));
        }
        }

        //Perform folders sorting
        do{
            int rel;

            res = f_opendir(&currentDir, path);

            if(res != FR_OK)
                return res;

            ListSortInit(0);

            while((f_readdir(&currentDir, &info) == FR_OK) && info.fname[0])
            {
                if(info.fattrib & AM_DIR)
                {
                    ListSortAddEntry(info.fname, strlen(info.fname), 0);
                }
            }

            ListSortSortData(0);
            MaxFileStructure[CurrentFileStructureLevel] = ListSortReturnNumOfEntries(0);

            //Save state before compare
            rel = PathCmp(FilePath, path);

            LoadNextDirectory:

            //If we don't have informations about entry save it
            if(FileStructure[CurrentFileStructureLevel] == 0)
                FileStructure[CurrentFileStructureLevel] = 1;
            else
                FileStructure[CurrentFileStructureLevel] += 1;

            if(FileStructure[CurrentFileStructureLevel] <= MaxFileStructure[CurrentFileStructureLevel])
            {
                ListSortReturnEntry(FileStructure[CurrentFileStructureLevel], info.fname, &len, 0);

                strcat(path, "/");
                i = strlen(path);
                strcat(path, info.fname);
                path[i+len] = 0;

                PLAYERDEBUG("PATH: %s\r\n", path);

                if(AUDIO_Playback_navigation == FILELIST)
                {
                    if(PathCmp(FilePath, path) <= rel)
                    {
                        char *ch = strrchr(path, '/');

                        if(ch != NULL)
                            *ch = 0;

                        goto LoadNextDirectory;
                    }
                }

                ++CurrentFileStructureLevel;
                ScanSDCard(path);
                --CurrentFileStructureLevel;

                mp3_info.position  = -1;
                DrawParameter31_bb = 1;

                if(AUDIO_Player_status != PLAYER_SAVE)
                {
                    char *ch = strrchr(path, '/');

                    if(ch != NULL)
                        *ch = 0;
                }
                else
                    return FR_NO_FILE;
            }
            else
            {
                FileStructure[CurrentFileStructureLevel] = 0;
                return (~FR_OK);
            }
        }while(FileStructure[CurrentFileStructureLevel] <= MaxFileStructure[CurrentFileStructureLevel]);

		return FR_NO_FILE;
	}

	return res;
}

void ReduceUnusedSpaces(char *str)
{
	if(*str != 0)
	{
		char *ptr = str;
		str = str + strlen(str)-1;

		while(str > ptr)
		{
			//if last character is space delete it
			if(*str == 32)
				*(str--) = 0;
			else
				break;
		}
	}
}

unsigned int ReturnNumOfFilesInFolder(void)
{
	return NumberOfMusicFilesInFolder;
}

unsigned int ReturnCurrentFileNumber(void)
{
	return CurrentMusicFileInFolder;
}

void SetFileNumberToPlay(unsigned int filenumber)
{
	NumberOfMusicFileToPlay = filenumber-1;
}

//1-supported file
//0-unsupported file
int CheckFileType(char *filename)
{
	if((strstr(filename,".MP3") != NULL) || (strstr(filename,".mp3") != NULL))
    {
        FileType = FILE_TYPE_MP3;
		return FILE_TYPE_MP3;
    }
	else if((strstr(filename,".PLS") != NULL) || (strstr(filename,".pls") != NULL))
    {
        FileType = FILE_TYPE_PLS;
		return FILE_TYPE_PLS;
    }

    FileType = FILE_TYPE_UNKNOWN;
	return 200;
}

int ReturnCurrentTime(void)
{
    switch(FileType)
    {
        case FILE_TYPE_MP3:
            mp3_info.position = (uint32_t)(((long long)MP3_Data_Index*mp3_info.duration)/(mp3FileObject.fsize - mp3_info.data_start));
            return (int)mp3_info.position;
        break;

        default:
            return -1;
        break;
    }
}

int PathCmp(char *src, char *dst)
{
    int i = 0;
    char *tmp;

    tmp = strchr(src, '/');

    if(tmp != NULL)
    {
        ++tmp;

        do{
            tmp = strchr(tmp, '/');

            if(tmp == NULL)
            {
                if(!strcmp(src, dst))
                    ++i;
                break;
            }

            if(strncmp (src, dst, tmp-src) != 0)
                break;
            ++tmp;
        }while(++i);
    }

    return i;
}

void DMA2_Channel2_IRQHandler(void)
{
	portBASE_TYPE xTaskWoken = pdFALSE;

	if(DMA2_ISR_HTIF2_bb)
	{
		DMA2_IFCR_CHTIF2_bb = 1;
		audio_buffer_fill  |= LOW_EMPTY;
	}

	if(DMA2_ISR_TCIF2_bb)
	{
		DMA2_IFCR_CTCIF2_bb = 1;
		audio_buffer_fill  |= HIGH_EMPTY;
	}

	xSemaphoreGiveFromISR( xPlayerSemaphore, &xTaskWoken);

	if(xTaskWoken == pdTRUE)
		vPortYieldFromISR();
}
