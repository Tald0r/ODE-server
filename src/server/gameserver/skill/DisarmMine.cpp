//////////////////////////////////////////////////////////////////////////////
// Filename    : DisarmMine.cpp
// Written by  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "DisarmMine.h"

#include "Assert.h"
#include "EffectDarkness.h"
#include "GCDeleteEffectFromTile.h"
#include "GCDeleteObject.h"
#include "GCSkillFailed1.h"
#include "GCSkillFailed2.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK5.h"
#include "SkillInfo.h"
#include "item/Mine.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void DisarmMine::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);
}


DisarmMine g_DisarmMine;
