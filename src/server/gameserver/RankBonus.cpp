//////////////////////////////////////////////////////////////////////////////
// Filename    : RankBonus.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////


#include "RankBonus.h"

#include "Exception.h"


RankBonus::RankBonus()

{}

RankBonus::RankBonus(DWORD type, DWORD point, Rank_t rank)

{
    m_Type = type;
    m_Point = point;
    m_Rank = rank;
}

RankBonus::~RankBonus()

{
    __BEGIN_TRY

    __END_CATCH_NO_RETHROW
}
