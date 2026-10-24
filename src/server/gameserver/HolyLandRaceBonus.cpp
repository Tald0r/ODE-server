#include "HolyLandRaceBonus.h"

#include "CastleInfoManager.h"
#include "GameContext.h"

HolyLandRaceBonus::HolyLandRaceBonus() {
    refresh();
}

HolyLandRaceBonus::~HolyLandRaceBonus() {}

void HolyLandRaceBonus::refresh()

{
    // Clear the previous ones.
    clear();

    const unordered_map<ZoneID_t, CastleInfo*>& castleInfos = de::gameContext().castleInfos().getCastleInfos();
    unordered_map<ZoneID_t, CastleInfo*>::const_iterator itr = castleInfos.begin();


    // Set the bonus according to the race that currently owns each castle.
    for (; itr != castleInfos.end(); itr++) {
        CastleInfo* pCastleInfo = itr->second;

        if (pCastleInfo->getRace() == RACE_SLAYER) {
            const list<OptionType_t>& optionTypes = pCastleInfo->getOptionTypeList();
            m_SlayerOptionTypes.insert(m_SlayerOptionTypes.begin(), optionTypes.begin(), optionTypes.end());
        } else if (pCastleInfo->getRace() == RACE_VAMPIRE) {
            const list<OptionType_t>& optionTypes = pCastleInfo->getOptionTypeList();
            m_VampireOptionTypes.insert(m_VampireOptionTypes.begin(), optionTypes.begin(), optionTypes.end());
        } else {
            // Ignore
        }
    }
}
