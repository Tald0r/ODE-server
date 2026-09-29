//////////////////////////////////////////////////////////////////////////////
// Filename    : PartyExp.h
// Description :
// de-core: the experience bonus a party shares.
//
// When a party member earns experience with other members nearby, the
// server raises the amount by a percentage that grows with the number of
// members within reach, then splits the raised pool among them in
// proportion to their levels. This file holds the first step, which the
// five sharing functions of the server's Party (attribute experience,
// vampire and ousters experience, rank experience and advancement
// experience) apply alike. The members within reach, their levels and the
// split stay in Party.cpp, whose five splits differ (in float for three,
// in int for two).
//
// The skill experience and the fame a party member gains are raised by
// other tables (skill/SkillExperience.cpp), not by this one.
//
// Only the server computes it; the client is sent the experience. This
// file is not in the vendored subset (DECORE_VENDORED_SOURCES), and its
// rows are in vectors/server/, which the client does not copy.
//////////////////////////////////////////////////////////////////////////////

#ifndef DECORE_PARTY_EXP_H
#define DECORE_PARTY_EXP_H

namespace decore {

// The experience `amount` becomes when it is shared by memberCount members:
// 150% of it for 2, 195% for 3, 225% for 4, 250% for 5 and 270% for 6,
// truncated toward zero through percentValue; any other count, 1 or more
// than a party holds included, leaves it as it is.
int partyExpPool(int amount, int memberCount);

} // namespace decore

#endif
