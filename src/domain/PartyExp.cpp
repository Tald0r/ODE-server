//////////////////////////////////////////////////////////////////////////////
// Filename    : PartyExp.cpp
// Description :
// The experience bonus a party shares. See PartyExp.h. The switch is the
// one the five sharing functions of the server's Party.cpp each carried.
//////////////////////////////////////////////////////////////////////////////

#include "domain/PartyExp.h"

#include "domain/Formulas.h"

namespace decore {

int partyExpPool(int amount, int memberCount) {
    switch (memberCount) {
    case 2:
        amount = percentValue(amount, 150);
        break;
    case 3:
        amount = percentValue(amount, 195);
        break;
    case 4:
        amount = percentValue(amount, 225);
        break;
    case 5:
        amount = percentValue(amount, 250);
        break;
    case 6:
        amount = percentValue(amount, 270);
        break;
    default:
        break;
    }

    return amount;
}

} // namespace decore
