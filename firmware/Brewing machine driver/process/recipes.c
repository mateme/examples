#include "recipes.h"
#include "m25pe80.h"
#include "printf.h"
#include <string.h>
#include "FreeRTOS.h"
#include "crc.h"

static unsigned short CurrentRecipe = 1;
static unsigned short CurrentRecipeAddress = 1;
static unsigned short NumOfRecipes = 0;

int GetNumberOfRecipes(void)
{
    int i;
    int num = 0;
    unsigned char buff;

    for(i=1; i<=MAX_RECIPES; ++i)
    {
        M25PE80ReadData(i<<10, &buff, 1);

        if(buff != 0xFF)
            ++num;
    }

    return num;
}

void DeleteRecipe(int RecipeAddress)
{
    if(RecipeAddress > MAX_RECIPES || RecipeAddress < 1)
        return;

    M25PE80WriteEnable();
    M25PE80PageErase((RecipeAddress<<10) | (0<<8));
    while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

    M25PE80WriteEnable();
    M25PE80PageErase((RecipeAddress<<10) | (1<<8));
    while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

    M25PE80WriteEnable();
    M25PE80PageErase((RecipeAddress<<10) | (2<<8));
    while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

    M25PE80WriteEnable();
    M25PE80PageErase((RecipeAddress<<10) | (3<<8));
    while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

    M25PE80WriteDisable();
}

int AddRecipe(SystemRecipe *stat)
{
    int i, j;
    unsigned char buff;
    unsigned short crctmp;
    unsigned char *tmp = (unsigned char*)stat;

    //Check free space for new Recipe
    j = GetNumberOfRecipes();
    if(j > MAX_RECIPES)
        return -1;

    //Calculate CRC
    stat->RecipeCRC = 0;
    crctmp = (unsigned short)(CRCCalculate(stat, RECIPE_SIZE) & 0xFFFF);
    stat->RecipeCRC = crctmp;

    if(j==0)
    {
        M25PE80WriteEnable();
        M25PE80PageProgram((1<<10) | (0<<8), tmp, 256);
        while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

        M25PE80WriteEnable();
        M25PE80PageProgram((1<<10) | (1<<8), tmp+256, 256);
        while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

        M25PE80WriteEnable();
        M25PE80PageProgram((1<<10) | (2<<8), tmp+512, 256);
        while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

        M25PE80WriteEnable();
        M25PE80PageProgram((1<<10) | (3<<8), tmp+768, 256);
        while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

        M25PE80WriteDisable();

        return 1;
    }

    //Find first free sector to store new Recipe
    for(i=1; i<=MAX_RECIPES; ++i)
    {
        M25PE80ReadData(i<<10, &buff, 1);

        if(buff == 0xFF)
        {
            j = i;
            break;
        }
    }

    M25PE80WriteEnable();
    M25PE80PageProgram((j<<10) | (0<<8), tmp, 256);
    while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

    M25PE80WriteEnable();
    M25PE80PageProgram((j<<10) | (1<<8), tmp+256, 256);
    while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

    M25PE80WriteEnable();
    M25PE80PageProgram((j<<10) | (2<<8), tmp+512, 256);
    while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

    M25PE80WriteEnable();
    M25PE80PageProgram((j<<10) | (3<<8), tmp+768, 256);
    while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

    M25PE80WriteDisable();

    return j;
}

int ChangeRecipe(SystemRecipe *stat, int RecipeAddress)
{
    unsigned char *tmp = (unsigned char*)stat;

    if(RecipeAddress > MAX_RECIPES || RecipeAddress < 1)
        return -1;

    DeleteRecipe(RecipeAddress);

    //Calculate CRC
    stat->RecipeCRC = 0;
    stat->RecipeCRC = (unsigned short)(CRCCalculate(stat, RECIPE_SIZE) & 0xFFFF);

    M25PE80WriteEnable();
    M25PE80PageProgram((RecipeAddress<<10) | (0<<8), tmp, 256);
    while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

    M25PE80WriteEnable();
    M25PE80PageProgram((RecipeAddress<<10) | (1<<8), tmp+256, 256);
    while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

    M25PE80WriteEnable();
    M25PE80PageProgram((RecipeAddress<<10) | (2<<8), tmp+512, 256);
    while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

    M25PE80WriteEnable();
    M25PE80PageProgram((RecipeAddress<<10) | (3<<8), tmp+768, 256);
    while(M25PE80ReadStatus() & MEM_STATUS_WIP) {};

    M25PE80WriteDisable();

    return 0;
}

int GetRecipe(SystemRecipe *stat, int RecipeAddress)
{
    unsigned short recipeCRC;

    if(RecipeAddress > MAX_RECIPES || RecipeAddress < 1)
        return -1;

    M25PE80ReadData(RecipeAddress<<10, stat, 1024);

    recipeCRC = stat->RecipeCRC;
    stat->RecipeCRC = 0;
    stat->RecipeCRC = (unsigned short)(CRCCalculate(stat, RECIPE_SIZE) & 0xFFFF);

    if(recipeCRC != stat->RecipeCRC)
        return -2;

    return 0;
}

void InitRecipesList(void)
{
    int i;
    unsigned char buff;

    NumOfRecipes  = (unsigned short)GetNumberOfRecipes();
    CurrentRecipe = 1;
    CurrentRecipeAddress = 0;

    if(NumOfRecipes != 0)
    {
        for(i=1; i<=MAX_RECIPES; ++i)
        {
            M25PE80ReadData(i<<10, &buff, 1);

            if(buff != 0xFF)
            {
                CurrentRecipeAddress = i;
                break;
            }

            if(buff == 0xFF && i == MAX_RECIPES)
            {
                CurrentRecipeAddress = 1;
                break;
            }
        }
    }
}

void NextRecipe(void)
{
    if(NumOfRecipes > 1)
    {
        if(CurrentRecipe < NumOfRecipes)
        {
            int i;
            unsigned char buff;
            ++CurrentRecipe;

            for(i=(CurrentRecipeAddress+1); i<=MAX_RECIPES; ++i)
            {
                M25PE80ReadData(i<<10, &buff, 1);

                if(buff != 0xFF)
                {
                    CurrentRecipeAddress = i;
                    break;
                }
            }
        }
        else
        {
            int i;
            unsigned char buff;
            CurrentRecipe = 1;

            for(i=1; i<=MAX_RECIPES; ++i)
            {
                M25PE80ReadData(i<<10, &buff, 1);

                if(buff != 0xFF)
                {
                    CurrentRecipeAddress = i;
                    break;
                }
            }
        }
    }
}

void PreviousRecipe(void)
{
    if(NumOfRecipes > 1)
    {
        if(CurrentRecipe > 1)
        {
            int i;
            unsigned char buff;
            --CurrentRecipe;

            for(i=(CurrentRecipeAddress-1); i>0; --i)
            {
                M25PE80ReadData(i<<10, &buff, 1);

                if(buff != 0xFF)
                {
                    CurrentRecipeAddress = i;
                    break;
                }
            }
        }
        else
        {
            int i;
            unsigned char buff;
            CurrentRecipe = NumOfRecipes;

            for(i=MAX_RECIPES; i>0; --i)
            {
                M25PE80ReadData(i<<10, &buff, 1);

                if(buff != 0xFF)
                {
                    CurrentRecipeAddress = i;
                    break;
                }
            }
        }
    }
}

unsigned short ReturnCurrentRecipe(void)
{
    return CurrentRecipe;
}

unsigned short ReturnCurrentRecipeAddress(void)
{
    return CurrentRecipeAddress;
}

int ListRecipes(char *buf)
{
    unsigned short i;
    unsigned char buff;

    if(NumOfRecipes == 0)
        return 0;

    memset(buf+1, ' ', 31);
    *buf = 0x7E;

    if(NumOfRecipes == 1)
    {
        M25PE80ReadData(((int)CurrentRecipeAddress<<10), buf+1, 15);
        return 1;
    }
    else
    {
        M25PE80ReadData(((int)CurrentRecipeAddress<<10), buf+1, 15);

        if((CurrentRecipe+1) > NumOfRecipes)
        {
            i = 1;
        }
        else
        {
            i = CurrentRecipeAddress+1;
        }

        for(; i<=MAX_RECIPES; ++i)
        {
            M25PE80ReadData(i<<10, &buff, 1);

            if(buff != 0xFF)
            {
                M25PE80ReadData(((int)i<<10), buf+17, 15);
                return NumOfRecipes;
            }
        }
    }

    return 0;
}

void InitRecipe(SystemRecipe *stat, int steps)
{
    int i, j;

    switch(steps)
    {
    case 1:
        stat->Phases[0].Temperature = 730;
        stat->Phases[0].Time        = 0;
        stat->Phases[1].Temperature = 670;
        stat->Phases[1].Time        = RECIPE_MIN(60);
        break;

    case 2:
        stat->Phases[0].Temperature = 730;
        stat->Phases[0].Time        = 0;
        stat->Phases[1].Temperature = 670;
        stat->Phases[1].Time        = RECIPE_MIN(60);
        stat->Phases[2].Temperature = 780;
        stat->Phases[2].Time        = 0;
        stat->Phases[3].Temperature = 780;
        stat->Phases[3].Time        = RECIPE_MIN(10);
        break;

    case 3:
        stat->Phases[0].Temperature = 680;
        stat->Phases[0].Time        = 0;
        stat->Phases[1].Temperature = 620;
        stat->Phases[1].Time        = RECIPE_MIN(30);
        stat->Phases[2].Temperature = 720;
        stat->Phases[2].Time        = 0;
        stat->Phases[3].Temperature = 720;
        stat->Phases[3].Time        = RECIPE_MIN(30);
        stat->Phases[4].Temperature = 780;
        stat->Phases[4].Time        = 0;
        stat->Phases[5].Temperature = 780;
        stat->Phases[5].Time        = RECIPE_MIN(10);
        break;

    case 4:
        stat->Phases[0].Temperature = 460;
        stat->Phases[0].Time        = 0;
        stat->Phases[1].Temperature = 440;
        stat->Phases[1].Time        = RECIPE_MIN(15);
        stat->Phases[2].Temperature = 620;
        stat->Phases[2].Time        = 0;
        stat->Phases[3].Temperature = 620;
        stat->Phases[3].Time        = RECIPE_MIN(40);
        stat->Phases[4].Temperature = 720;
        stat->Phases[4].Time        = 0;
        stat->Phases[5].Temperature = 720;
        stat->Phases[5].Time        = RECIPE_MIN(20);
        stat->Phases[6].Temperature = 780;
        stat->Phases[6].Time        = 0;
        stat->Phases[7].Temperature = 780;
        stat->Phases[7].Time        = RECIPE_MIN(10);
        break;
    }

    stat->BoilingTime = RECIPE_MIN(60);

    for(i=0; i < RECIPE_NUM_OF_MALTS_SLOTS; ++i)
        stat->MaltsAddress[i] = i;

    j = i;

    for(i =0; i < BOILING_NUM_OF_PHASES; ++i)
    {
        stat->BoilingPhasesTime[i] = 0;
        stat->HopsAddress[i]  = ++j;
    }
}

int GetMaltHopQuantity(int hopmalt)
{
    int quantity = 0;
    char *p = NULL;
    p = pvPortMalloc(513);

    if(p != NULL)
    {
        int i, j, check = 0;
        int address;
        char *pch;

        address = (hopmalt > 0) ? MEM_MALTS_BASE : MEM_HOPS_BASE;

        for(i=0; i<64; ++i)
        {
            M25PE80ReadData(address, p, 512);
            address += 512;

            //if(*(p+512) == 0x0D)
            //    ++quantity;

            *(p+512) = 0;

            pch = p;

            for(j=0; j<70; ++j)
            {
                if(check)
                {
                    pch = strchr(pch, 0x0D);

                    if(pch != NULL)
                    {
                        ++quantity;
                        ++pch;
                    }

                    check = 0;
                }

                pch = strchr(pch, 0x09);

                if(pch != NULL)
                {
                    ++pch;

                    pch = strchr(pch, 0x0D);

                    if(pch != NULL)
                    {
                        ++quantity;
                        ++pch;
                    }
                    else
                        check = 1;
                }
                else
                {
                    break;
                }
            }
        }

        vPortFree(p);
        return quantity;
    }
    else
    {
        printf_("Allocation error\r\n");
        return 0;
    }
}

int GetMaltHopName(int hopmalt, char *str, int position)
{
    char *p = NULL;
    p = pvPortMalloc(576);//512(search buffer)+64(name buffer)

    if(p != NULL)
    {
        int i, j;
        int address;
        char *fch, *sch;
        int cnt = 0;

        *str = 0;

        address  = (hopmalt > 0) ? MEM_MALTS_BASE : MEM_HOPS_BASE;
        *(p+512) = 0;

        for(i=0; i<64; ++i)
        {
            M25PE80ReadData(address, p, 512);
            address += 512;
            fch = p;

            for(j=0; j<256; ++j)
            {
                fch = strchr(fch, 0x09);

                if(fch != NULL)
                {
                    if(cnt == position)
                    {
                        sch = strchr(fch+1, 0x0D);

                        if(sch != NULL)
                        {
                            *sch = 0;
                            strcpy(str, fch+1);

                            vPortFree(p);
                            return cnt;
                        }
                        else
                        {
                            M25PE80ReadData(address, p+512, 63);
                            *(p+575) = 0;

                            sch = strchr(fch+1, 0x0D);

                            if(sch != NULL)
                            {
                                *sch = 0;
                                strcpy(str, fch+1);

                                vPortFree(p);
                                return cnt;
                            }
                            else
                            {
                                vPortFree(p);
                                return -2;
                            }
                        }
                    }
                    else
                    {
                        ++cnt;
                        ++fch;
                    }
                }
                else
                    break;
            }
        }

        vPortFree(p);
        return -3;
    }
    else
    {
        printf_("Allocation error\r\n");
        return -1;
    }
}

int AddHopToRecipe(SystemRecipe *stat, int hop, int weight, int unit, int step)
{
    //Obtain free memory in comment section
    int size = 0, i;

    for(i=0; i<RECIPE_NUM_OF_MALTS_SLOTS ; ++i)
        size += strlen(&stat->Comment[stat->MaltsAddress[i]]);

    for(i=0; i<BOILING_NUM_OF_PHASES ; ++i)
        size += strlen(&stat->Comment[stat->HopsAddress[i]]);

    size += BOILING_NUM_OF_PHASES + RECIPE_NUM_OF_MALTS_SLOTS;

    if(size < RECIPE_COMMENT_LEN && step < stat->BoilingNumOfPhases)
    {
        char buffer[40];

        GetMaltHopName(GET_HOP_NAME, buffer, hop);
        i = strlen(buffer);

        if(unit)
            sprintf_(&buffer[i], " %d,%.2d kg\r\n", weight/1000, (weight%1000)/10);
        else
            sprintf_(&buffer[i], " %d g\r\n", weight);


        if((strlen(buffer) + size) < RECIPE_COMMENT_LEN)
        {
            if(step == (stat->BoilingNumOfPhases-1))
            {
                strcat(&stat->Comment[stat->HopsAddress[step]], buffer);
            }
            else
            {
                int hopsSize = 0;
                int copySize;

                for(i=step+1; i<stat->BoilingNumOfPhases; ++i)
                    hopsSize += strlen(&stat->Comment[stat->HopsAddress[i]]);

                copySize = hopsSize + stat->BoilingNumOfPhases;

                char *b = NULL;
                b = pvPortMalloc(copySize);

                if(b != NULL)
                {
                    memcpy(b, &stat->Comment[stat->HopsAddress[step+1]], copySize);
                    strcat(&stat->Comment[stat->HopsAddress[step]], buffer);

                    for(i=step+1; i<stat->BoilingNumOfPhases; ++i)
                        stat->HopsAddress[i] += strlen(buffer);

                    memcpy(&stat->Comment[stat->HopsAddress[step+1]], b, copySize);

                    vPortFree(b);
                }
                else
                    return -3;
            }
        }
        else
            return -2;

        return 0;
    }
    else
        return -1;
}

int AddMaltToRecipe(SystemRecipe *stat, int malt, int weight, int unit, int step)
{
    //Obtain free memory in comment section
    int size = 0, i;

    for(i=0; i<RECIPE_NUM_OF_MALTS_SLOTS ; ++i)
        size += strlen(&stat->Comment[stat->MaltsAddress[i]]);

    for(i=0; i<BOILING_NUM_OF_PHASES ; ++i)
        size += strlen(&stat->Comment[stat->HopsAddress[i]]);

    size += BOILING_NUM_OF_PHASES + RECIPE_NUM_OF_MALTS_SLOTS;

    if(size < RECIPE_COMMENT_LEN && step < RECIPE_NUM_OF_MALTS_SLOTS)
    {
        char buffer[40];

        GetMaltHopName(GET_MALT_NAME, buffer, malt);
        i = strlen(buffer);

        if(unit)
            sprintf_(&buffer[i], " %d,%.2d kg\r\n", weight/1000, (weight%1000)/10);
        else
            sprintf_(&buffer[i], " %d g\r\n", weight);

        if((strlen(buffer) + size) < RECIPE_COMMENT_LEN)
        {
            if(step == (RECIPE_NUM_OF_MALTS_SLOTS - 1))
            {
                int hopsSize = 0;
                int copySize;

                for(i=0; i<stat->BoilingNumOfPhases; ++i)
                    hopsSize += strlen(&stat->Comment[stat->HopsAddress[i]]);

                copySize = hopsSize + stat->BoilingNumOfPhases;

                char *b = NULL;
                b = pvPortMalloc(copySize);

                if(b != NULL)
                {
                    memcpy(b, &stat->Comment[stat->HopsAddress[0]], copySize);
                    strcat(&stat->Comment[stat->MaltsAddress[1]], buffer);

                    for(i=0; i<stat->BoilingNumOfPhases; ++i)
                        stat->HopsAddress[i] += strlen(buffer);

                    memcpy(&stat->Comment[stat->HopsAddress[0]], b, copySize);

                    vPortFree(b);
                }
                else
                    return -3;
            }
            else
            {
                int hopsSize = 0;
                int copySize;

                for(i=0; i<stat->BoilingNumOfPhases; ++i)
                    hopsSize += strlen(&stat->Comment[stat->HopsAddress[i]]);

                hopsSize += strlen(&stat->Comment[stat->MaltsAddress[1]]);
                copySize = hopsSize + stat->BoilingNumOfPhases+1;

                char *b = NULL;
                b = pvPortMalloc(copySize);

                if(b != NULL)
                {
                    memcpy(b, &stat->Comment[stat->MaltsAddress[1]], copySize);
                    strcat(&stat->Comment[stat->MaltsAddress[0]], buffer);

                    for(i=0; i<stat->BoilingNumOfPhases; ++i)
                        stat->HopsAddress[i] += strlen(buffer);

                    stat->MaltsAddress[1] += strlen(buffer);

                    memcpy(&stat->Comment[stat->MaltsAddress[1]], b, copySize);

                    vPortFree(b);
                }
                else
                    return -3;
            }

        }
        else
            return -2;

        return 0;
    }
    else
        return -1;
}

void DebugPrintRecipe(int i)
{
    SystemRecipe *p = NULL;
    p = pvPortMalloc(RECIPE_SIZE);

    if(p != NULL)
    {
        GetRecipe(p, i);

        printf_("Size of recipe: %d\r\n", sizeof(SystemRecipe));
        printf_("Comment length: %d\r\n", RECIPE_COMMENT_LEN);

        p->RecipeName[RECIPE_NAME_LEN-1] = 0;
        p->Comment[RECIPE_COMMENT_LEN-1]   = 0;

        printf_("Recipe name: %s\r\n", p->RecipeName);
        printf_("Phases: %d\r\n", p->NumOfPhases);

        if(p->NumOfPhases > 6)
            p->NumOfPhases = 6;

        for(i=0; i < p->NumOfPhases; ++i)
            printf_("%d. Temp: %d oC Time: %d s\r\n", i, p->Phases[i].Temperature, p->Phases[i].Time);

        printf_("Malts address: %x\r\n", p->MaltsAddress);
        printf_("Hops address: %x\r\n", p->HopsAddress);


        printf_("Malts 1:\r\n%s\r\n", p->Comment[p->MaltsAddress[0]]);
        printf_("Malts 2:\r\n%s\r\n", p->Comment[p->MaltsAddress[1]]);

        printf_("Hops 1:\r\n%s\r\n", p->Comment[p->HopsAddress[0]]);
        printf_("Hops 2:\r\n%s\r\n", p->Comment[p->HopsAddress[1]]);
        printf_("Hops 3:\r\n%s\r\n", p->Comment[p->HopsAddress[2]]);
        printf_("Hops 4:\r\n%s\r\n", p->Comment[p->HopsAddress[3]]);

        printf_("CRC: %x\r\n", p->RecipeCRC);

        p->RecipeCRC = 0;
        p->RecipeCRC = (unsigned short)(CRCCalculate(p, RECIPE_SIZE) & 0xFFFF);

        printf_("CRC calculated: %x\r\n", p->RecipeCRC);


        vPortFree(p);
    }
    else
    {
        printf_("Allocation error\r\n");
    }
}

void DebugPrintRecipeRAM(SystemRecipe *p)
{
    int i;

    p->RecipeName[RECIPE_NAME_LEN-1] = 0;
    p->Comment[RECIPE_COMMENT_LEN-1]   = 0;

    printf_("Recipe name: %s\r\n", p->RecipeName);
    printf_("Phases: %d\r\n", p->NumOfPhases);

    if(p->NumOfPhases > 6)
        p->NumOfPhases = 6;

    for(i=0; i < p->NumOfPhases; ++i)
        printf_("%d. Temp: %d oC Time: %d s\r\n", i, p->Phases[i].Temperature, p->Phases[i].Time);

    printf_("Malts address: %x\r\n", p->MaltsAddress[0]);
    printf_("Malts address: %x\r\n", p->MaltsAddress[1]);
    printf_("Hops address: %x\r\n", p->HopsAddress[0]);
    printf_("Hops address: %x\r\n", p->HopsAddress[1]);
    printf_("Hops address: %x\r\n", p->HopsAddress[2]);
    printf_("Hops address: %x\r\n", p->HopsAddress[3]);


    printf_("Malts 1:\r\n%s\r\n", &(p->Comment[p->MaltsAddress[0]]));
    printf_("Malts 2:\r\n%s\r\n", &(p->Comment[p->MaltsAddress[1]]));

    printf_("Hops 1:\r\n%s\r\n", &(p->Comment[p->HopsAddress[0]]));
    printf_("Hops 2:\r\n%s\r\n", &(p->Comment[p->HopsAddress[1]]));
    printf_("Hops 3:\r\n%s\r\n", &(p->Comment[p->HopsAddress[2]]));
    printf_("Hops 4:\r\n%s\r\n", &(p->Comment[p->HopsAddress[3]]));

    printf_("CRC: %x\r\n", p->RecipeCRC);

    p->RecipeCRC = 0;
    p->RecipeCRC = (unsigned short)(CRCCalculate(p, RECIPE_SIZE) & 0xFFFF);

    printf_("CRC calculated: %x\r\n", p->RecipeCRC);
}

