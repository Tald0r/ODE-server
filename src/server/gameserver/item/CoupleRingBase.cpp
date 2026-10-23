//////////////////////////////////////////////////////////////////////////////
// Filename    : CoupleRingBase.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CoupleRingBase.h"

#include "CoupleRing.h"
#include "GameContext.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "StringStream.h"
#include "VampireCoupleRing.h"

//////////////////////////////////////////////////////////////////////////////
// class CoupleRingBase member methods
//////////////////////////////////////////////////////////////////////////////

CoupleRingBase::CoupleRingBase()

{
    m_PartnerItemID = 0;
}

void CoupleRingBase::setPartnerItemID(ItemID_t partnerItemID)

{
    m_PartnerItemID = partnerItemID;
    StringStream msg;
    msg << "PartnerItemID=" << (uint)partnerItemID;

    tinysave(msg.toString().c_str());
}

ItemInfo* CoupleRingBase::getItemInfo(ItemType_t itemType, Race_t race)

{
    switch (race) {
    case RACE_SLAYER:
        return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_COUPLE_RING, itemType);
        break;
    case RACE_VAMPIRE:
        return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_VAMPIRE_COUPLE_RING, itemType);
        break;
    default:
        Assert(false);
    }
}
