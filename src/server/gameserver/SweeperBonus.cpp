//////////////////////////////////////////////////////////////////////////////
// Filename    : SweeperBonus.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SweeperBonus.h"

#include "repository/WarInfoRepository.h"

void SweeperBonus::setRace(Race_t race) {
    if (m_Race != race) {
        m_Race = race;
        defaultWarInfoRepository().saveSweeperBonusOwner(m_Race, m_Type);
    }
}

string SweeperBonus::toString() const

{
    StringStream msg;

    msg << "SweeperBonus(\n";

    OptionTypeListConstItor itr = m_OptionTypeList.begin();
    for (; itr != m_OptionTypeList.end(); itr++) {
        msg << (int)(*itr) << ",";
    }

    msg << ")\n";

    return msg.toString();
}
