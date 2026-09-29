//--------------------------------------------------------------------------------
//
// Filename   : WeatherManager.cpp
// Written By : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "WeatherManager.h"

#include "DarkLightInfo.h"
#include "GCChangeDarkLight.h"
#include "GCChangeWeather.h"
#include "GCLightning.h"
#include "GameContext.h"
#include "GameTime.h"
#include "PKZoneInfoManager.h"
#include "TimeManager.h"
#include "WeatherInfo.h"
#include "Zone.h"
#include "domain/DarkLight.h"

//--------------------------------------------------------------------------------
//
// destructor
//
// Save every object in the container to the DB, then delete them.
//
//--------------------------------------------------------------------------------
WeatherManager::~WeatherManager()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}


//--------------------------------------------------------------------------------
// initialize current zone's weather, darklevel, lightlevel
//--------------------------------------------------------------------------------
void WeatherManager::init()

{
    __BEGIN_TRY

    //--------------------------------------------------------------------------------
    // Set today's weather.
    //--------------------------------------------------------------------------------

    // Fetch this month's weather information.
    // const WeatherInfo & weatherInfo = de::gameContext().weatherInfos().getWeatherInfo(gametime.getMonth());

    // Roll the dice to pick today's weather.
    // m_TodayWeather = weatherInfo.getWeather(Dice(1,100));

    //--------------------------------------------------------------------------------
    // Set tomorrow's date.
    //--------------------------------------------------------------------------------

    // Get the game time as a time_t.
    time_t gmtime = de::gameContext().worldTime().getgametime();

    // Convert to a tm structure to read the hour, minute and second.
    tm ltm;
    localtime_r(&gmtime, &ltm);
    // struct tm* ptm = localtime(&gmtime);

    // Compute the game-time seconds left until tomorrow.
    int dSec = (23 - ltm.tm_hour) * 3600 + (59 - ltm.tm_min) * 60 + (60 - ltm.tm_sec);

    // Adding (remaining game time / 5) to the current real time gives tomorrow's real time.
    time_t currentTime = time(0);
    m_Tomorrow = currentTime + dSec / 5;

    //--------------------------------------------------------------------------------
    // Set the current weather, the weather level and the next weather change time.
    //--------------------------------------------------------------------------------
    // Decide the chance of rain or snow and the current weather.

    m_TodayWeather = WEATHER_CLEAR;
    m_Probability = 0;
    m_CurrentWeather = WEATHER_CLEAR;
    m_WeatherLevel = 0;

    m_NextWeatherChangingTime = m_Tomorrow;
    m_NextLightning = m_Tomorrow;

    //--------------------------------------------------------------------------------
    // Set the zone's current light and dark levels and the next change time.
    //--------------------------------------------------------------------------------
    DarkLightInfo* pDIInfo = de::gameContext().darkLights().getCurrentDarkLightInfo(m_pZone);
    m_pZone->setDarkLevel(pDIInfo->getDarkLevel());
    m_pZone->setLightLevel(pDIInfo->getLightLevel());

    // Compute how many game-time seconds remain until the next ten-minute mark.
    dSec = (9 - ltm.tm_min / 10) * 60 + (60 - ltm.tm_sec);
    m_Next10Min = currentTime + dSec / 5;

    __END_CATCH
}


//--------------------------------------------------------------------------------
// Changes the weather by itself once the set time arrives. Must be called from the zone's heartbeat.
//--------------------------------------------------------------------------------
void WeatherManager::heartbeat()

{
    // Nothing to do outside a normal field.
    if (m_pZone->getZoneType() != ZONE_NORMAL_FIELD)
        return;

    // Nothing to do in a PK zone either.
    if (de::gameContext().pkZoneInfos().isPKZone(m_pZone->getZoneID()))
        return;

    time_t currentTime = time(0);

    //--------------------------------------------------------------------------------
    // Once a day has passed, change today's weather and compute the weather chance.
    // Note that even when today's weather changes, the weather does not change
    // until m_NextWeatherChangingTime has passed.
    //--------------------------------------------------------------------------------
    if (currentTime > m_Tomorrow) {
        // Fetch the GameTime object to find out which month it is.
        GameTime gametime = de::gameContext().worldTime().getGameTime();

        // Fetch this month's weather information.
        const WeatherInfo& weatherInfo = de::gameContext().weatherInfos().getWeatherInfo(gametime.getMonth());

        // Roll the dice to pick today's weather.
        m_TodayWeather = weatherInfo.getWeather(Dice(1, 100));

        // Decide the chance of rain or snow.
        // The current weather only changes once the next weather change time passes,
        // so there is no need to set it here.
        if (m_TodayWeather == WEATHER_CLEAR) {
            // A clear day stays clear.
            m_Probability = 0;
        } else {
            m_Probability = Dice(3, 100) / 3;
        }

        // Get the game time as a time_t.
        time_t gmtime = de::gameContext().worldTime().getgametime();

        // Convert to a tm structure to read the hour, minute and second.
        tm ltm;
        localtime_r(&gmtime, &ltm);
        // struct tm* ptm = localtime(&gmtime);

        // Compute the game-time seconds left until tomorrow.
        int dSec = (23 - ltm.tm_hour) * 3600 + (59 - ltm.tm_min) * 60 + (59 - ltm.tm_sec);

        // Adding (remaining game time / 5) to the current real time gives tomorrow's real time.
        m_Tomorrow = currentTime + dSec / 5;
    }

    //--------------------------------------------------------------------------------
    // The weather has to be changed once the set time has passed.
    //--------------------------------------------------------------------------------
    if (currentTime > m_NextWeatherChangingTime) {
        if (m_TodayWeather == WEATHER_CLEAR) {
            // On a clear day no weather change happens until tomorrow.
            m_CurrentWeather = WEATHER_CLEAR;
            m_WeatherLevel = 0;
            m_NextWeatherChangingTime = m_Tomorrow;
            m_NextLightning = m_Tomorrow / 2;
        } else {
            m_CurrentWeather = (Dice(1, 100) < m_Probability) ? m_TodayWeather : WEATHER_CLEAR;
            m_WeatherLevel = (m_CurrentWeather != WEATHER_CLEAR) ? Dice(3, 20) / 3 : 0;

            // The period is kept short for testing.
            // m_NextWeatherChangingTime = time(0) + Dice(1,20)* 60;
            m_NextWeatherChangingTime = time(0) + 60;

            // The period is kept short for testing.
            // m_NextLightning = time(0) + 60;
            m_NextLightning = time(0) + 20;
        }

        GCChangeWeather gcChangeWeather;
        gcChangeWeather.setWeather(m_CurrentWeather);
        gcChangeWeather.setWeatherLevel(m_WeatherLevel);

        m_pZone->broadcastPacket(&gcChangeWeather, NULL);
    }

    //--------------------------------------------------------------------------------
    // When the weather is rain and the set time has passed, check for lightning.
    //--------------------------------------------------------------------------------
    if (m_CurrentWeather == WEATHER_RAINY && currentTime > m_NextLightning) {
        // Roll 1d100; a result below (rain level * 5 - 30) counts as
        // a lightning strike. The weather level tops out at 20, so
        // the chance reaches at most 70%.
        if (Dice(1, 100) < (uint)max(0, m_WeatherLevel * 5 - 30)) {
            GCLightning gcLightning;
            gcLightning.setDelay(Dice(1, 5));
            m_pZone->broadcastPacket(&gcLightning, NULL);
        }

        // Set the next lightning check time.
        // (When the weather is clear it does not even need to advance.)
        m_NextLightning += 60;
    }

    //--------------------------------------------------------------------------------
    // Update the zone's light and dark information every ten game-time minutes.
    //--------------------------------------------------------------------------------
    if (currentTime > m_Next10Min) {
        DarkLightInfo* pDIInfo = de::gameContext().darkLights().getCurrentDarkLightInfo(m_pZone);

        DarkLevel_t darkLevel = pDIInfo->getDarkLevel();
        LightLevel_t lightLevel = pDIInfo->getLightLevel();

        // Broadcast if either the dark level or the light level changed.
        if (darkLevel != m_pZone->getDarkLevel() || lightLevel != m_pZone->getLightLevel()) {
            m_pZone->setDarkLevel(darkLevel);
            m_pZone->setLightLevel(lightLevel);

            // The broadcast sends the first packet to each slayer without
            // Lightness or Yellow Poison and the second to each vampire, and
            // sends an ousters nothing. It does not consult the zone's type:
            // the levels are the ones a slayer and a vampire see outside a
            // castle and a PK zone.
            decore::DarkLightViewer slayer = {};
            slayer.race = decore::DarkLightRace::Slayer;
            const decore::DarkLight slayerLevels = decore::darkLightForViewer(slayer, darkLevel, lightLevel);

            decore::DarkLightViewer vampire = {};
            vampire.race = decore::DarkLightRace::Vampire;
            const decore::DarkLight vampireLevels = decore::darkLightForViewer(vampire, darkLevel, lightLevel);

            GCChangeDarkLight gcChangeDarkLight;
            gcChangeDarkLight.setDarkLevel(slayerLevels.darkLevel);
            gcChangeDarkLight.setLightLevel(slayerLevels.lightLevel);

            GCChangeDarkLight gcChangeDarkLight2;
            gcChangeDarkLight2.setDarkLevel(vampireLevels.darkLevel);
            gcChangeDarkLight2.setLightLevel(vampireLevels.lightLevel);

            m_pZone->broadcastDarkLightPacket(&gcChangeDarkLight, &gcChangeDarkLight2, NULL);

            // cout << "(DarkLevel/LightLevel) : (" << (int)darkLevel << "," << (int)lightLevel << ") at " <<
            // g_pTimeManager->getGameTime().toString() << endl;
        } else {
            // cout << "(DarkLevel/LightLevel) : (" << (int)darkLevel << "," << (int)lightLevel << ") at " <<
            // g_pTimeManager->getGameTime().toString() << endl;
        }

        m_Next10Min += 120;
    }
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string WeatherManager::toString() const

{
    StringStream msg;

    msg << "WeatherManager(";
    msg << "     Today's weather: " << Weather2String[m_TodayWeather];
    msg << "     Current weather: " << Weather2String[m_CurrentWeather];
    msg << "    Rain/snow chance: " << (int)m_Probability << "%";
    msg << "       Weather level: " << (int)m_WeatherLevel;

    time_t currentTime = time(0);

    msg << "        Current time: " << ctime(&currentTime);
    msg << "       Game tomorrow: " << ctime(&m_Tomorrow);
    msg << " Next weather change: " << ctime(&m_NextWeatherChangingTime);
    msg << "Next lightning check: " << ctime(&m_NextLightning);

    return msg.toString();
}
