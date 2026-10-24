////////////////////////////////////////////////////////////////////////////////
// Filename    : WeatherInfo.cpp
// Written By  : Reiot
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "WeatherInfo.h"

#include "Assert.h"
#include "repository/GameInfoRepository.h"

////////////////////////////////////////////////////////////////////////////////
// class WeatherInfo member methods
////////////////////////////////////////////////////////////////////////////////

Weather WeatherInfo::getWeather(uint probability) const

{
    Assert(m_Probabilities[WEATHER_CLEAR] + m_Probabilities[WEATHER_RAINY] + m_Probabilities[WEATHER_SNOWY] == 100);
    Assert(probability <= 100);

    if (probability < m_Probabilities[WEATHER_CLEAR])
        return WEATHER_CLEAR;
    else if (probability < +m_Probabilities[WEATHER_CLEAR] + m_Probabilities[WEATHER_RAINY])
        return WEATHER_RAINY;
    else
        return WEATHER_SNOWY;
}

string WeatherInfo::toString() const

{
    StringStream msg;
    msg << "WeatherInfo(" << "Clear:" << m_Probabilities[WEATHER_CLEAR] << "%"
        << ",Rainy:" << m_Probabilities[WEATHER_RAINY] << "%" << ",Snowy:" << m_Probabilities[WEATHER_SNOWY] << "%"
        << ")";
    return msg.toString();
}

////////////////////////////////////////////////////////////////////////////////
// class WeatherInfoManager member methods
////////////////////////////////////////////////////////////////////////////////

void WeatherInfoManager::load()

{
    vector<WeatherRow> rows = defaultGameInfoRepository().loadWeather();

    Assert(rows.size() == 12);

    for (size_t r = 0; r < rows.size(); r++) {
        int month = rows[r].month;
        m_WeatherInfos[month - 1].setProbability(WEATHER_CLEAR, rows[r].clear);
        m_WeatherInfos[month - 1].setProbability(WEATHER_RAINY, rows[r].rainy);
        m_WeatherInfos[month - 1].setProbability(WEATHER_SNOWY, rows[r].snowy);
    }
}

string WeatherInfoManager::toString() const

{
    StringStream msg;

    msg << "WeatherInfoManager(";

    for (int i = 0; i < 12; i++) {
        msg << (int)(i + 1) << " month : " << m_WeatherInfos[i].toString();
    }

    msg << ")";

    return msg.toString();
}
