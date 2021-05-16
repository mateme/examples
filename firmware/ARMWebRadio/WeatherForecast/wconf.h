#ifndef WEATHER_CONF_H_INCLUDED
    #define WEATHER_CONF_H_INCLUDED

    //#define DEFAULT_WOEID "523920" //Warsaw
    #define DEFAULT_WOEID "514254" //Pruszcz Gdanski Poland
    #define WEATHERCONF_SEMAPHORE_WAITTIME 200

    void WOEIDRead(char *woeid);
    void WOEIDWrite(char *woeid);
#endif
