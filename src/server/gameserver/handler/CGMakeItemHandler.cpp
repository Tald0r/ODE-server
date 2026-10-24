//////////////////////////////////////////////////////////////////////////////
// Filename    : CGMakeItemHandler.cc
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGMakeItem.h"

#ifdef __GAME_SERVER__
#include "GCDeleteObject.h"
#include "GCDeleteandPickUpOK.h"
#include "GCSkillFailed1.h"
#include "GamePlayer.h"
#include "Item.h"
#include "SkillHandler.h"
#include "SkillHandlerManager.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "item/Money.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGMakeItemHandler::execute(CGMakeItem* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX __BEGIN_DEBUG

#ifdef __GAME_SERVER__


#endif // __GAME_SERVER__

        __END_DEBUG __END_DEBUG_EX
}
