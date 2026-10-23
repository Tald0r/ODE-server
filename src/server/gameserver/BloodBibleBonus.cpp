//////////////////////////////////////////////////////////////////////////////
// Filename    : BloodBibleBonus.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "BloodBibleBonus.h"

string BloodBibleBonus::toString() const

{
    StringStream msg;

    msg << "BloodBibleBonus(\n";

    OptionTypeListConstItor itr = m_OptionTypeList.begin();
    for (; itr != m_OptionTypeList.end(); itr++) {
        msg << (int)(*itr) << ",";
    }

    msg << ")\n";

    return msg.toString();
}
