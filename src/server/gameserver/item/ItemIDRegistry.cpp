//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemIDRegistry.cpp
// Written by  : excel96
// Description :
// The initItemIDRegistry member function of the 87 item classes that have
// one, pulled out of the item implementation files and implemented here.
//////////////////////////////////////////////////////////////////////////////

#include "AR.h"
#include "Belt.h"
#include "Blade.h"
#include "BloodBible.h"
#include "Bomb.h"
#include "BombMaterial.h"
#include "Bracelet.h"
#include "CarryingReceiver.h"
#include "CastleSymbol.h"
#include "Coat.h"
#include "CodeSheet.h"
#include "ComposMei.h"
#include "CoreZap.h"
#include "CoupleRing.h"
#include "Cross.h"
#include "Dermis.h"
#include "DyePotion.h"
#include "ETC.h"
#include "EffectItem.h"
#include "EventETC.h"
#include "EventGiftBox.h"
#include "EventItem.h"
#include "EventStar.h"
#include "EventTree.h"
#include "Fascia.h"
#include "GameContext.h"
#include "Glove.h"
#include "Helm.h"
#include "HolyWater.h"
#include "ItemInfoManager.h"
#include "Key.h"
#include "Larva.h"
#include "LearningItem.h"
#include "LuckyBag.h"
#include "Mace.h"
#include "Magazine.h"
#include "Mine.h"
#include "Mitten.h"
#include "MixingItem.h"
#include "Money.h"
#include "MoonCard.h"
#include "Motorcycle.h"
#include "Necklace.h"
#include "OustersArmsband.h"
#include "OustersBoots.h"
#include "OustersChakram.h"
#include "OustersCirclet.h"
#include "OustersCoat.h"
#include "OustersPendent.h"
#include "OustersRing.h"
#include "OustersStone.h"
#include "OustersSummonItem.h"
#include "OustersWristlet.h"
#include "Persona.h"
#include "PetEnchantItem.h"
#include "PetFood.h"
#include "PetItem.h"
#include "Potion.h"
#include "Pupa.h"
#include "QuestItem.h"
#include "Relic.h"
#include "ResurrectItem.h"
#include "Ring.h"
#include "SG.h"
#include "SMG.h"
#include "SMSItem.h"
#include "SR.h"
#include "Serum.h"
#include "Shield.h"
#include "Shoes.h"
#include "ShoulderArmor.h"
#include "Skull.h"
#include "SlayerPortalItem.h"
#include "Sweeper.h"
#include "Sword.h"
#include "TrapItem.h"
#include "Trouser.h"
#include "VampireAmulet.h"
#include "VampireBracelet.h"
#include "VampireCoat.h"
#include "VampireCoupleRing.h"
#include "VampireETC.h"
#include "VampireEarring.h"
#include "VampireNecklace.h"
#include "VampirePortalItem.h"
#include "VampireRing.h"
#include "VampireWeapon.h"
#include "WarItem.h"
#include "Water.h"
#include "repository/ItemRepository.h"

namespace {

// The body every initItemIDRegistry shares: the class's registry starts at
// the highest ItemID already in its object table (an empty table assigns
// nothing and leaves the static's zero), rounded up to the next
// ItemIDSuccessor multiple plus the ItemIDBase. `label` is what the boot log prints - the class name for
// most classes, the table name or a shortened name for a few, carried over
// verbatim from the hand-expanded originals.
void initItemIDRegistryFromTable(Mutex& mutex, ItemID_t& registry, const char* table, const char* label) {
    __ENTER_CRITICAL_SECTION(mutex)

    int count = defaultItemRepository().countItemRows(table);

    if (count != 0) {
        registry = defaultItemRepository().loadMaxItemID(table);
    }

    registry += (de::gameContext().itemInfos().getItemIDSuccessor() -
                 (registry % de::gameContext().itemInfos().getItemIDSuccessor())) +
                de::gameContext().itemInfos().getItemIDBase();

    __LEAVE_CRITICAL_SECTION(mutex)

    cout << label << "...ItemIDRegistry:" << registry << endl;
}

} // namespace

#define ITEMIDREGISTRY(CLASS, TABLE, LABEL)                                   \
    void CLASS::initItemIDRegistry(void) {                                    \
        initItemIDRegistryFromTable(m_Mutex, m_ItemIDRegistry, TABLE, LABEL); \
    }

ITEMIDREGISTRY(CarryingReceiver, "CarryingReceiverObject", "CarryingReceiverObject")
ITEMIDREGISTRY(ShoulderArmor, "ShoulderArmorObject", "ShoulderArmorObject")
ITEMIDREGISTRY(Dermis, "DermisObject", "DermisObject")
ITEMIDREGISTRY(Persona, "PersonaObject", "PersonaObject")
ITEMIDREGISTRY(Fascia, "FasciaObject", "FasciaObject")
ITEMIDREGISTRY(Mitten, "MittenObject", "MittenObject")
ITEMIDREGISTRY(Motorcycle, "MotorcycleObject", "Motorcycle")
ITEMIDREGISTRY(Potion, "PotionObject", "Potion")
ITEMIDREGISTRY(Water, "WaterObject", "Water")
ITEMIDREGISTRY(HolyWater, "HolyWaterObject", "HolyWater")
ITEMIDREGISTRY(Magazine, "MagazineObject", "Magazine")
ITEMIDREGISTRY(BombMaterial, "BombMaterialObject", "BombMaterial")
ITEMIDREGISTRY(ETC, "ETCObject", "ETC")
ITEMIDREGISTRY(Key, "KeyObject", "Key")
ITEMIDREGISTRY(Ring, "RingObject", "Ring")
ITEMIDREGISTRY(Bracelet, "BraceletObject", "Bracelet")
ITEMIDREGISTRY(Necklace, "NecklaceObject", "Necklace")
ITEMIDREGISTRY(Coat, "CoatObject", "Coat")
ITEMIDREGISTRY(Trouser, "TrouserObject", "Trouser")
ITEMIDREGISTRY(Shoes, "ShoesObject", "Shoes")
ITEMIDREGISTRY(Sword, "SwordObject", "Sword")
ITEMIDREGISTRY(Blade, "BladeObject", "Blade")
ITEMIDREGISTRY(Shield, "ShieldObject", "Shield")
ITEMIDREGISTRY(Cross, "CrossObject", "Cross")
ITEMIDREGISTRY(Mace, "MaceObject", "Mace")
ITEMIDREGISTRY(Glove, "GloveObject", "Glove")
ITEMIDREGISTRY(Helm, "HelmObject", "Helm")
ITEMIDREGISTRY(SG, "SGObject", "SG")
ITEMIDREGISTRY(SMG, "SMGObject", "SMG")
ITEMIDREGISTRY(AR, "ARObject", "AR")
ITEMIDREGISTRY(SR, "SRObject", "SR")
ITEMIDREGISTRY(Bomb, "BombObject", "Bomb")
ITEMIDREGISTRY(Mine, "MineObject", "Mine")
ITEMIDREGISTRY(Belt, "BeltObject", "Belt")
ITEMIDREGISTRY(LearningItem, "LearningItemObject", "LearningItem")
ITEMIDREGISTRY(Money, "MoneyObject", "Money")
ITEMIDREGISTRY(VampireRing, "VampireRingObject", "VampireRing")
ITEMIDREGISTRY(VampireBracelet, "VampireBraceletObject", "VampireBracelet")
ITEMIDREGISTRY(VampireNecklace, "VampireNecklaceObject", "VampireNecklace")
ITEMIDREGISTRY(VampireCoat, "VampireCoatObject", "VampireCoat")
ITEMIDREGISTRY(Skull, "SkullObject", "Skull")
ITEMIDREGISTRY(Serum, "SerumObject", "Serum")
ITEMIDREGISTRY(VampireETC, "VampireETCObject", "VampireETC")
ITEMIDREGISTRY(SlayerPortalItem, "SlayerPortalItemObject", "SlayerPortalItem")
ITEMIDREGISTRY(VampirePortalItem, "VampirePortalItemObject", "VampirePortalItem")
ITEMIDREGISTRY(EventGiftBox, "EventGiftBoxObject", "GiftBox")
ITEMIDREGISTRY(EventStar, "EventStarObject", "EventStar")
ITEMIDREGISTRY(VampireEarring, "VampireEarringObject", "VampireEarring")
ITEMIDREGISTRY(Relic, "RelicObject", "RelicObject")
ITEMIDREGISTRY(VampireWeapon, "VampireWeaponObject", "VampireWeaponObject")
ITEMIDREGISTRY(VampireAmulet, "VampireAmuletObject", "VampireAmuletObject")
ITEMIDREGISTRY(QuestItem, "QuestItemObject", "QuestItem")
ITEMIDREGISTRY(EventTree, "EventTreeObject", "EventTree")
ITEMIDREGISTRY(EventETC, "EventETCObject", "EventETC")
ITEMIDREGISTRY(BloodBible, "BloodBibleObject", "BloodBible")
ITEMIDREGISTRY(CastleSymbol, "CastleSymbolObject", "CastleSymbol")
ITEMIDREGISTRY(CoupleRing, "CoupleRingObject", "CoupleRing")
ITEMIDREGISTRY(VampireCoupleRing, "VampireCoupleRingObject", "CoupleRing")
ITEMIDREGISTRY(EventItem, "EventItemObject", "EventItem")
ITEMIDREGISTRY(DyePotion, "DyePotionObject", "DyePotion")
ITEMIDREGISTRY(ResurrectItem, "ResurrectItemObject", "ResurrectItem")
ITEMIDREGISTRY(MixingItem, "MixingItemObject", "MixingItem")
ITEMIDREGISTRY(OustersArmsband, "OustersArmsbandObject", "OustersArmsband")
ITEMIDREGISTRY(OustersBoots, "OustersBootsObject", "OustersBoots")
ITEMIDREGISTRY(OustersChakram, "OustersChakramObject", "OustersChakram")
ITEMIDREGISTRY(OustersCirclet, "OustersCircletObject", "OustersCirclet")
ITEMIDREGISTRY(OustersCoat, "OustersCoatObject", "OustersCoat")
ITEMIDREGISTRY(OustersPendent, "OustersPendentObject", "OustersPendent")
ITEMIDREGISTRY(OustersRing, "OustersRingObject", "OustersRing")
ITEMIDREGISTRY(OustersStone, "OustersStoneObject", "OustersStone")
ITEMIDREGISTRY(OustersWristlet, "OustersWristletObject", "OustersWristlet")
ITEMIDREGISTRY(Larva, "LarvaObject", "Larva")
ITEMIDREGISTRY(Pupa, "PupaObject", "Pupa")
ITEMIDREGISTRY(ComposMei, "ComposMeiObject", "ComposMei")
ITEMIDREGISTRY(OustersSummonItem, "OustersSummonItemObject", "OustersSummonItem")
ITEMIDREGISTRY(EffectItem, "EffectItemObject", "EffectItem")
ITEMIDREGISTRY(CodeSheet, "CodeSheetObject", "CodeSheet")
ITEMIDREGISTRY(MoonCard, "MoonCardObject", "MoonCard")
ITEMIDREGISTRY(Sweeper, "SweeperObject", "Sweeper")
ITEMIDREGISTRY(PetItem, "PetItemObject", "PetItem")
ITEMIDREGISTRY(PetFood, "PetFoodObject", "PetFood")
ITEMIDREGISTRY(PetEnchantItem, "PetEnchantItemObject", "PetEnchantItem")
ITEMIDREGISTRY(LuckyBag, "LuckyBagObject", "LuckyBag")
ITEMIDREGISTRY(SMSItem, "SMSItemObject", "SMSItem")
ITEMIDREGISTRY(CoreZap, "CoreZapObject", "CoreZap")
ITEMIDREGISTRY(TrapItem, "TrapItemObject", "TrapItem")
ITEMIDREGISTRY(WarItem, "WarItemObject", "WarItem")
