#ifndef RECIPES_H_INCLUDED
    #define RECIPES_H_INCLUDED

    #define RECIPES_SEMAPHORE_WAITTIME 500
    #define MAX_RECIPES                400

    #define RECIPE_MIN(x) (x*60)

    #define RECIPE_TEMPERATURE_MAX 800
    #define RECIPE_TEMPERATURE_MIN 300

    #define RECIPE_TIME_MAX     RECIPE_MIN(999)
    #define RECIPE_TIME_MIN     RECIPE_MIN(0)

    #define BOILING_TIME_MAX    RECIPE_MIN(240)
    #define BOILING_TIME_MIN    RECIPE_MIN(60)

    #define RECIPE_SIZE               1024
    #define RECIPE_NAME_LEN           16
    #define RECIPE_NUM_OF_PHASES      8
    #define RECIPE_NUM_OF_STEPS       4
    #define BOILING_NUM_OF_PHASES     4
    #define RECIPE_LINK_SIZE          2
    #define RECIPE_NUM_OF_MALTS_SLOTS 2
    #define RECIPE_COMMENT_LEN        (RECIPE_SIZE - RECIPE_NAME_LEN - 2 - RECIPE_NUM_OF_PHASES*4 - 2 - BOILING_NUM_OF_PHASES*1 - 1 - RECIPE_LINK_SIZE*(RECIPE_NUM_OF_MALTS_SLOTS+BOILING_NUM_OF_PHASES) - 2)

    #define MEM_HOPS_BASE  0x70000
    #define MEM_MALTS_BASE 0x80000

    #define GET_HOP_NAME   0
    #define GET_MALT_NAME  1

    typedef struct __attribute__((packed))
    {
        unsigned short Temperature;
        unsigned short Time;
    } ProcessPhase;

    typedef struct __attribute__((packed))
    {
        char RecipeName[RECIPE_NAME_LEN];

        unsigned char NumOfSteps;
        unsigned char NumOfPhases;
        ProcessPhase Phases[RECIPE_NUM_OF_PHASES];

        unsigned short BoilingTime;
        unsigned char  BoilingNumOfPhases;
        unsigned char  BoilingPhasesTime[BOILING_NUM_OF_PHASES];

        unsigned short MaltsAddress[RECIPE_NUM_OF_MALTS_SLOTS];
        unsigned short HopsAddress[BOILING_NUM_OF_PHASES];

        char Comment[RECIPE_COMMENT_LEN];
        unsigned short RecipeCRC;
    } SystemRecipe;

    int GetNumberOfRecipes(void);
    void DeleteRecipe(int RecipeAddress);
    int AddRecipe(SystemRecipe *stat);
    int ChangeRecipe(SystemRecipe *stat, int RecipeAddress);
    int GetRecipe(SystemRecipe *stat, int RecipeAddress);

    void InitRecipesList(void);
    void NextRecipe(void);
    void PreviousRecipe(void);
    unsigned short ReturnCurrentRecipe(void);
    unsigned short ReturnCurrentRecipeAddress(void);
    int ListRecipes(char *buf);
    void InitRecipe(SystemRecipe *stat, int steps);
    int GetMaltHopQuantity(int hopmalt);
    int GetMaltHopName(int hopmalt, char *str, int position);
    int AddHopToRecipe(SystemRecipe *stat, int hop, int weight, int unit, int step);
    int AddMaltToRecipe(SystemRecipe *stat, int malt, int weight, int unit, int step);
    void DebugPrintRecipe(int i);
    void DebugPrintRecipeRAM(SystemRecipe *p);
#endif
