#include "player.h"
#include "printf.h"
#include "stm32f4xx_hal_hcd.h"
#include "fatfs.h"
#include "vs1053.h"
#include "usb_host.h"
#include "systemcfg.h"
#include "communication.h"
#include "hdr_tim.h"
#include <string.h>

extern ApplicationTypeDef Appli_state;

#define MAX_NUM_OF_SONGS_IN_DIR 99

const char *CDsStr[6] = {"/CD1","/CD2","/CD3","/CD4","/CD5","/CD6"};

typedef struct
{
    char filename[13];
    unsigned char songplayed;
} SongsListEntry;

SongsListEntry PlayerList[MAX_NUM_OF_SONGS_IN_DIR];
unsigned char PlayerNumberOfSongs;

enum PlayerModuleState
{
    PLAYER_IDLE,
    PLAYER_STARTUP,
    PLAYER_PLAYING_MUSIC,
    PLAYER_GOSLEEP,
} PlayerState = PLAYER_STARTUP;

extern void ChangeAudioSourceToBluetooth(void);

int CheckSupportedFileType(char *name)
{
    char *ptr;

    ptr = strrchr(name, '.');

    if(ptr != NULL)
    {
        if(!strcmp(ptr, ".MP3"))
            return 1;
        else if(!strcmp(ptr, ".WAV"))
            return 2;
        else if(!strcmp(ptr, ".FLAC"))
            return 2;
    }

    return 0;
}

FRESULT scan_dir(int dirlp)// (char* path)
{
    FRESULT res;
    DIR dir;
    FILINFO fno;
    int i = 0;
    char pathbuf[8];

    SongsListEntry tmp;
    int changes = 0;

    if(dirlp >= 0 && dirlp <=5)
        strcpy(pathbuf, CDsStr[dirlp]);

    memset(PlayerList, 0,sizeof(SongsListEntry)*MAX_NUM_OF_SONGS_IN_DIR);
    PlayerNumberOfSongs = 0;

    printf_("Dir path %s\r\n", pathbuf);

    res = f_opendir(&dir, pathbuf);

    if (res == FR_OK)
    {
        for(;;)
        {
            res = f_readdir(&dir, &fno);

            if (res != FR_OK || fno.fname[0] == 0)
                break;
            else
            {
                printf_("%s\r\n", fno.fname);
                //vTaskDelay(10);

                if(CheckSupportedFileType(fno.fname))
                {
                    ++PlayerNumberOfSongs;
                    strcpy(PlayerList[i++].filename, fno.fname);
                }

                if(i >= MAX_NUM_OF_SONGS_IN_DIR)
                    break;
            }
        }

        f_closedir(&dir);
    }


    do{
        changes = 0;

        for(i=0;i<(PlayerNumberOfSongs-1);++i)
        {
            if(strcmp(PlayerList[i].filename,PlayerList[i+1].filename) > 0)
            {
                tmp = PlayerList[i+1];
                PlayerList[i+1] = PlayerList[i];
                PlayerList[i] = tmp;

                ++changes;
            }
        }
    }while(changes > 0);

    return res;
}

int CountMusicFilesInDir(int dirlp)
{
    FRESULT res;
    DIR dir;
    FILINFO fno;

    int i = 0;
    char pathbuf[8];

    if(dirlp >= 0 && dirlp <=5)
        strcpy(pathbuf, CDsStr[dirlp]);

    res = f_opendir(&dir, pathbuf);

    if (res == FR_OK)
    {
        for(;;)
        {
            res = f_readdir(&dir, &fno);

            if (res != FR_OK || fno.fname[0] == 0)
                break;
            else
            {
                if(CheckSupportedFileType(fno.fname))
                    ++i;

                if(i >= MAX_NUM_OF_SONGS_IN_DIR)
                    break;
            }
        }

        f_closedir(&dir);
    }

    return i;
}


void ResetTrackTimeCounter(void)
{
    //Takie sobie te rozwiazanie
    TIM4->CNT = 0;
}

void ChangeAudioSourceToVS1053(void)
{
    PlayerState = PLAYER_STARTUP;
    vTaskResume(xPlayerTaskHandle);
}

void vPlayerTask(void * pvArg)
{
    pvArg = pvArg;

    int i;

    int dirlp;
    int songlp;

    int dirlp_next  = -1;
    int songlp_next = -1;

    char playerpath[22];
    unsigned char command;

    BoardInit();
    VS1053Init();
    VS1053VolumeSet(254);

    Display.disc     = 0;
    Display.track    = 0;
    Display.time_min = 0;
    Display.time_sec = 0;
    Display.mode     = 0;

    //We set base position here to save current track&cd between audio source switching TODO: repair this
    /*dirlp  = 0;
    songlp = 0;*/

    for(;;)
    {
        switch(PlayerState)
        {
        case PLAYER_IDLE:
            vTaskDelay(50);
            vTaskSuspend(NULL);
            break;

        case PLAYER_STARTUP:
            SetSystemLED(LED_STATUS_PLAYER_STREND);

            EnableMuteAudio();
            vTaskDelay(5);

            EnableVS1053Audio();
            DisableBluetoothAudio();

            vTaskDelay(5);
            DisableMuteAudio();

            for(i=254 ; i >=0 ; --i )
            {
                vTaskDelay(5);
                VS1053VolumeSet(i);
            }

            vTaskDelay(4);
            PlayerState = PLAYER_PLAYING_MUSIC;

            Display.disc     = 0;
            Display.track    = 0;
            Display.time_min = 0;
            Display.time_sec = 0;
            Display.mode     = 0;

            break;

        case PLAYER_PLAYING_MUSIC:
            if(Appli_state == APPLICATION_READY)
            {
                SetSystemLED(LED_STATUS_PLAYER);

                CDsInSystem[0] = CountMusicFilesInDir(0);
                CDsInSystem[1] = CountMusicFilesInDir(1);
                CDsInSystem[2] = CountMusicFilesInDir(2);
                CDsInSystem[3] = CountMusicFilesInDir(3);
                CDsInSystem[4] = CountMusicFilesInDir(4);
                CDsInSystem[5] = CountMusicFilesInDir(5);

                for(dirlp = 0; dirlp <= 5; ++dirlp)
                {
                    char * pch;

                    if(dirlp_next >= 0)
                    {
                        dirlp      = dirlp_next;
                        dirlp_next = -1;
                    }

                    Display.disc = dirlp;

                    scan_dir(dirlp);
                    strcpy(playerpath, CDsStr[dirlp]);

                    //TODO: Napisz funkcje k tora zapisze liste odwtarzanych plikow i te ktore nie zostaly odtworzone

                    for(songlp = 0; songlp < PlayerNumberOfSongs; ++songlp)
                    {
                        if(songlp_next >= 0)
                        {
                            if(songlp_next > 99)
                            {
                                songlp = PlayerNumberOfSongs - 1;
                            }
                            else
                            {
                                songlp = songlp_next;
                            }

                            songlp_next = -1;
                        }

                        //TODO: Tutaj zapisuj pliki odtworzone ju¿ i jeœli jest jakiœ z "polecenia to zapisz go równie¿" jak poŸniej wejdzie mix to mixuj te co zostaly
                        PlayerList[songlp].songplayed = 1;

                        //Save current track number to Display struct
                        ResetTrackTimeCounter();
                        Display.track    = songlp;
                        Display.time_min = 0;
                        Display.time_sec = 0;

                        strcat(playerpath,"/");
                        strcat(playerpath,PlayerList[songlp].filename);

                        VS1053PlayerOpenFile(playerpath);

                        //Tell to communication task that we have loaded new track
                        xSemaphoreGive(xNewTrackLoaded);

                        while(playerState != psStopped)
                        {
                            VS1053PlayerFeedBuffer();

                            if(xQueueReceive( SerialRXPlayerHandler, &command, 0))
                            {
                                switch(command)
                                {
                                case CMD_PLAYER_PLAY:
                                    playerState = psPlayback;
                                    break;

                                case CMD_PLAYER_PAUSE:
                                    break;

                                case CMD_PLAYER_NEXT:
                                    playerState = psUserRequestedCancel;

                                    if(songlp < PlayerNumberOfSongs)
                                    {
                                        if((songlp +1) == PlayerNumberOfSongs)
                                        {
                                            dirlp_next = dirlp + 1;

                                            if(dirlp_next > 5)
                                                dirlp_next = 0;

                                            songlp_next = 0;
                                        }
                                        else
                                        {
                                            songlp_next = songlp + 1;
                                        }
                                    }
                                    else
                                    {
                                        dirlp_next = dirlp + 1;

                                        if(dirlp_next > 5)
                                            dirlp_next = 0;

                                        songlp_next = 0;
                                    }
                                    break;

                                case CMD_PLAYER_PREV:
                                    playerState = psUserRequestedCancel;

                                    if(songlp == 0)
                                    {
                                        if(dirlp == 0)
                                        {
                                            dirlp_next  = 5;
                                            songlp_next = 1000;//Select last track in previous CD
                                        }
                                        else
                                        {
                                            dirlp_next  = dirlp - 1;
                                            songlp_next = 1000;//Select last track in previous CD
                                        }
                                    }
                                    else
                                    {
                                        songlp_next = songlp - 1;
                                    }
                                    break;

                                case CMD_PLAYER_SEEK_NEXT:
                                    break;

                                case CMD_PLAYER_SEEK_PREV:
                                    break;

                                case CMD_PLAYER_CD1:
                                    playerState = psUserRequestedCancel;
                                    dirlp_next  = 0;
                                    songlp_next = 0;
                                    break;

                                case CMD_PLAYER_CD2:
                                    playerState = psUserRequestedCancel;
                                    dirlp_next  = 1;
                                    songlp_next = 0;
                                    break;

                                case CMD_PLAYER_CD3:
                                    playerState = psUserRequestedCancel;
                                    dirlp_next  = 2;
                                    songlp_next = 0;
                                    break;

                                case CMD_PLAYER_CD4:
                                    playerState = psUserRequestedCancel;
                                    dirlp_next  = 3;
                                    songlp_next = 0;
                                    break;

                                case CMD_PLAYER_CD5:
                                    playerState = psUserRequestedCancel;
                                    dirlp_next  = 4;
                                    songlp_next = 0;
                                    break;

                                case CMD_PLAYER_CD6:
                                    playerState = psUserRequestedCancel;
                                    dirlp_next  = 5;
                                    songlp_next = 0;
                                    break;

                                case CMD_PLAYER_BUTTON:
                                    //TODO: player multiswitch
                                    playerState = psUserRequestedCancel;

                                    PlayerState = PLAYER_GOSLEEP;

                                    printf_("PLAYER->System switch: SHORT\r\n");
                                    break;

                                case CMD_PLAYER_BUTTON_LONG:
                                    //TODO: player multiswitch
                                    printf_("PLAYER->System switch: LONG\r\n");

                                    //
                                    playerState = psUserRequestedCancel;

                                    if(songlp < PlayerNumberOfSongs)
                                    {
                                        if((songlp +1) == PlayerNumberOfSongs)
                                        {
                                            dirlp_next = dirlp + 1;

                                            if(dirlp_next > 5)
                                                dirlp_next = 0;

                                            songlp_next = 0;
                                        }
                                        else
                                        {
                                            songlp_next = songlp + 1;
                                        }
                                    }
                                    else
                                    {
                                        dirlp_next = dirlp + 1;

                                        if(dirlp_next > 5)
                                            dirlp_next = 0;

                                        songlp_next = 0;
                                    }
                                    //
                                    break;
                                }
                            }
                        }

                        VS1053PlayerCloseFile();

                        pch = strrchr(playerpath,'/');
                        *pch = 0;

                        if(PlayerState != PLAYER_PLAYING_MUSIC)
                            break;
                    }

                    if(PlayerState != PLAYER_PLAYING_MUSIC)
                        break;
                }
            }
            else
            {
                vTaskDelay(100);
            }
            break;

        case PLAYER_GOSLEEP:
            SetSystemLED(LED_STATUS_PLAYER_STREND);

            for(i=0 ; i <= 254 ; ++i )
            {
                vTaskDelay(5);
                VS1053VolumeSet(i);
            }

            vTaskDelay(4);
            PlayerState = PLAYER_IDLE;

            EnableMuteAudio();
            vTaskDelay(5);

            DisableVS1053Audio();
            EnableBluetoothAudio();

            vTaskDelay(5);
            DisableMuteAudio();

            ChangeAudioSourceToBluetooth();

            break;
        }


    }
}

void TIM4_IRQHandler(void)
{
    DisplayPacket tmp;
    tmp = Display;

    if(TIM4_SR_UIF_bb)
    {
        ++tmp.time_sec;

        if(tmp.time_sec > 59)
        {
            tmp.time_sec = 0;
            ++tmp.time_min;
        }

        Display = tmp;

        TIM4_SR_UIF_bb = 0;
    }
}

