//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemInfoManager.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ItemInfoManager.h"

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
#include "Exception.h"
#include "Fascia.h"
#include "GameContext.h"
#include "Glove.h"
#include "Helm.h"
#include "HolyWater.h"
#include "ItemUtil.h"
#include "KernelContext.h"
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
#include "Motorcycle.h"
#include "Necklace.h"
#include "OptionInfo.h"
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
#include "Properties.h"
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
#include "StringStream.h"
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
#include "item/MoonCard.h"


//////////////////////////////////////////////////////////////////////////////
// Constructor
//////////////////////////////////////////////////////////////////////////////
ItemInfoManager::ItemInfoManager()

{
    for (int i = 0; i < Item::ITEM_CLASS_MAX; i++)
        m_InfoClassManagers[i] = NULL;
}

//////////////////////////////////////////////////////////////////////////////
// Destructor
//////////////////////////////////////////////////////////////////////////////
ItemInfoManager::~ItemInfoManager()

{
    try {
        for (int i = 0; i < Item::ITEM_CLASS_MAX; i++) {
            SAFE_DELETE(m_InfoClassManagers[i]);
        }
    } catch (Throwable&) {
        // A Throwable from the try block is dropped here and goes no further.
    }
}

//////////////////////////////////////////////////////////////////////////////
// ItemInfoManager::init()
//////////////////////////////////////////////////////////////////////////////
void ItemInfoManager::init()

{
    // Set ItemIDSuccessor and Base.
    m_ItemIDSuccessor = de::kernelContext().config().getPropertyInt("ServerCount");
    m_ItemIDBase = de::kernelContext().config().getPropertyInt("ServerID");

    addInfoClassManager(Item::ITEM_CLASS_AR, new ARInfoManager());
    AR::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_BELT, new BeltInfoManager());
    Belt::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_CROSS, new CrossInfoManager());
    Cross::initItemIDRegistry();


    addInfoClassManager(Item::ITEM_CLASS_BLADE, new BladeInfoManager());
    Blade::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_BOMB, new BombInfoManager());
    Bomb::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_BOMB_MATERIAL, new BombMaterialInfoManager());
    BombMaterial::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_BRACELET, new BraceletInfoManager());
    Bracelet::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_COAT, new CoatInfoManager());
    Coat::initItemIDRegistry();


    addInfoClassManager(Item::ITEM_CLASS_ETC, new ETCInfoManager());
    ETC::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_GLOVE, new GloveInfoManager());
    Glove::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_HELM, new HelmInfoManager());
    Helm::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_HOLYWATER, new HolyWaterInfoManager());
    HolyWater::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_KEY, new KeyInfoManager());
    Key::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_LEARNINGITEM, new LearningItemInfoManager());
    LearningItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_MAGAZINE, new MagazineInfoManager());
    Magazine::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_MINE, new MineInfoManager());
    Mine::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_MONEY, new MoneyInfoManager());
    Money::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_MOTORCYCLE, new MotorcycleInfoManager());
    Motorcycle::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_NECKLACE, new NecklaceInfoManager());
    Necklace::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_POTION, new PotionInfoManager());
    Potion::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_RING, new RingInfoManager());
    Ring::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_SG, new SGInfoManager());
    SG::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_SMG, new SMGInfoManager());
    SMG::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_SHIELD, new ShieldInfoManager());
    Shield::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_SHOES, new ShoesInfoManager());
    Shoes::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_SWORD, new SwordInfoManager());
    Sword::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_SR, new SRInfoManager());
    SR::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_TROUSER, new TrouserInfoManager());
    Trouser::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_VAMPIRE_BRACELET, new VampireBraceletInfoManager());
    VampireBracelet::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_VAMPIRE_NECKLACE, new VampireNecklaceInfoManager());
    VampireNecklace::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_VAMPIRE_RING, new VampireRingInfoManager());
    VampireRing::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_WATER, new WaterInfoManager());
    Water::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_VAMPIRE_COAT, new VampireCoatInfoManager());
    VampireCoat::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_SKULL, new SkullInfoManager());
    Skull::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_MACE, new MaceInfoManager());
    Mace::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_SERUM, new SerumInfoManager());
    Serum::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_VAMPIRE_ETC, new VampireETCInfoManager());
    VampireETC::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_SLAYER_PORTAL_ITEM, new SlayerPortalItemInfoManager());
    SlayerPortalItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM, new VampirePortalItemInfoManager());
    VampirePortalItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_EVENT_GIFT_BOX, new EventGiftBoxInfoManager());
    EventGiftBox::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_EVENT_STAR, new EventStarInfoManager());
    EventStar::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_VAMPIRE_EARRING, new VampireEarringInfoManager());
    VampireEarring::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_RELIC, new RelicInfoManager());
    Relic::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_VAMPIRE_WEAPON, new VampireWeaponInfoManager());
    VampireWeapon::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_VAMPIRE_AMULET, new VampireAmuletInfoManager());
    VampireAmulet::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_QUEST_ITEM, new QuestItemInfoManager());
    QuestItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_EVENT_TREE, new EventTreeInfoManager());
    EventTree::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_EVENT_ETC, new EventETCInfoManager());
    EventETC::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_BLOOD_BIBLE, new BloodBibleInfoManager());
    BloodBible::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_CASTLE_SYMBOL, new CastleSymbolInfoManager());
    CastleSymbol::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_COUPLE_RING, new CoupleRingInfoManager());
    CoupleRing::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_VAMPIRE_COUPLE_RING, new VampireCoupleRingInfoManager());
    VampireCoupleRing::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_EVENT_ITEM, new EventItemInfoManager());
    EventItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_DYE_POTION, new DyePotionInfoManager());
    DyePotion::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_RESURRECT_ITEM, new ResurrectItemInfoManager());
    ResurrectItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_MIXING_ITEM, new MixingItemInfoManager());
    MixingItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_OUSTERS_ARMSBAND, new OustersArmsbandInfoManager());
    OustersArmsband::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_OUSTERS_BOOTS, new OustersBootsInfoManager());
    OustersBoots::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_OUSTERS_CHAKRAM, new OustersChakramInfoManager());
    OustersChakram::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_OUSTERS_CIRCLET, new OustersCircletInfoManager());
    OustersCirclet::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_OUSTERS_COAT, new OustersCoatInfoManager());
    OustersCoat::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_OUSTERS_PENDENT, new OustersPendentInfoManager());
    OustersPendent::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_OUSTERS_RING, new OustersRingInfoManager());
    OustersRing::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_OUSTERS_STONE, new OustersStoneInfoManager());
    OustersStone::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_OUSTERS_WRISTLET, new OustersWristletInfoManager());
    OustersWristlet::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_LARVA, new LarvaInfoManager());
    Larva::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_PUPA, new PupaInfoManager());
    Pupa::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_COMPOS_MEI, new ComposMeiInfoManager());
    ComposMei::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM, new OustersSummonItemInfoManager());
    OustersSummonItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_EFFECT_ITEM, new EffectItemInfoManager());
    EffectItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_CODE_SHEET, new CodeSheetInfoManager());
    CodeSheet::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_MOON_CARD, new MoonCardInfoManager());
    MoonCard::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_SWEEPER, new SweeperInfoManager());
    Sweeper::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_PET_ITEM, new PetItemInfoManager());
    PetItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_PET_FOOD, new PetFoodInfoManager());
    PetFood::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_PET_ENCHANT_ITEM, new PetEnchantItemInfoManager());
    PetEnchantItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_LUCKY_BAG, new LuckyBagInfoManager());
    LuckyBag::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_SMS_ITEM, new SMSItemInfoManager());
    SMSItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_CORE_ZAP, new CoreZapInfoManager());
    CoreZap::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_TRAP_ITEM, new TrapItemInfoManager());
    TrapItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_WAR_ITEM, new WarItemInfoManager());
    WarItem::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_CARRYING_RECEIVER, new CarryingReceiverInfoManager());
    CarryingReceiver::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_SHOULDER_ARMOR, new ShoulderArmorInfoManager());
    ShoulderArmor::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_DERMIS, new DermisInfoManager());
    Dermis::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_PERSONA, new PersonaInfoManager());
    Persona::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_FASCIA, new FasciaInfoManager());
    Fascia::initItemIDRegistry();

    addInfoClassManager(Item::ITEM_CLASS_MITTEN, new MittenInfoManager());
    Mitten::initItemIDRegistry();
}


//////////////////////////////////////////////////////////////////////////////
// put a class's info manager in the table and load its item infos
//////////////////////////////////////////////////////////////////////////////
void ItemInfoManager::addInfoClassManager(Item::ItemClass itemClass, InfoClassManager* pInfoClassManager) {
    Assert(itemClass < Item::ITEM_CLASS_MAX);
    Assert(pInfoClassManager != NULL);
    Assert(pInfoClassManager->getItemClass() == itemClass);
    Assert(m_InfoClassManagers[itemClass] == NULL);

    m_InfoClassManagers[itemClass] = pInfoClassManager;
    pInfoClassManager->init();
}


//////////////////////////////////////////////////////////////////////////////
// get sub info class manager
//////////////////////////////////////////////////////////////////////////////
InfoClassManager* ItemInfoManager::getInfoManager(Item::ItemClass Class) const

{
    Assert(Class < Item::ITEM_CLASS_MAX);
    Assert(m_InfoClassManagers[Class] != NULL);

    return m_InfoClassManagers[Class];
}


//////////////////////////////////////////////////////////////////////////////
// get item info
//////////////////////////////////////////////////////////////////////////////
ItemInfo* ItemInfoManager::getItemInfo(Item::ItemClass Class, ItemType_t ItemType) const

{
    Assert(Class < Item::ITEM_CLASS_MAX);
    Assert(m_InfoClassManagers[Class] != NULL);

    return m_InfoClassManagers[Class]->getItemInfo(ItemType);
}


//////////////////////////////////////////////////////////////////////////////
// get #item-info
//////////////////////////////////////////////////////////////////////////////
uint ItemInfoManager::getItemCount(Item::ItemClass Class) const

{
    Assert(Class < Item::ITEM_CLASS_MAX);
    Assert(m_InfoClassManagers[Class] != NULL);

    return m_InfoClassManagers[Class]->getInfoCount();
}

//////////////////////////////////////////////////////////////////////////////
// get possible item type vector for specific item class
//////////////////////////////////////////////////////////////////////////////
vector<ItemType_t> ItemInfoManager::getPossibleItemTypes(Item::ItemClass IClass, uint minLevel, uint maxLevel)

{
    Assert(IClass < Item::ITEM_CLASS_MAX);
    Assert(m_InfoClassManagers[IClass] != NULL);

    vector<ItemType_t> result;
    uint ItemCount = m_InfoClassManagers[IClass]->getInfoCount();

    for (ItemType_t i = 0; i < ItemCount; i++) {
        ItemInfo* pItemInfo = m_InfoClassManagers[IClass]->getItemInfo(i);
        Assert(pItemInfo != NULL);

        uint itemLevel = pItemInfo->getItemLevel();

        // If the level of the current item lies between the min level and
        // the max level, add the item type to the vector.
        if (minLevel <= itemLevel && itemLevel <= maxLevel) {
            result.push_back(i);
        }
    }

    return result;
}

bool ItemInfoManager::isPossibleItem(Item::ItemClass IClass, ItemType_t IType, const list<OptionType_t>& OType)

{
    Assert(IClass < Item::ITEM_CLASS_MAX);
    Assert(m_InfoClassManagers[IClass] != NULL);

    if (IType < m_InfoClassManagers[IClass]->getInfoCount()) {
        if (!OType.empty()) {
            try {
                list<OptionType_t>::const_iterator itr = OType.begin();
                for (; itr != OType.end(); itr++) {
                    OptionType_t optionType = *itr;

                    if (de::gameContext().optionInfos().getOptionInfo(optionType) == NULL) {
                        return false;
                    }
                }

                return true;
            } catch (NoSuchElementException& nsee) {
                cout << nsee.toString().c_str() << endl;
                return false;
            }
        } else {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////////////
string ItemInfoManager::toString() const

{
    StringStream msg;
    msg << "ItemInfoManager(";

    for (uint i = 0; i < Item::ITEM_CLASS_MAX; i++) {
        if (m_InfoClassManagers[i] == NULL)
            msg << "NULL";
        else
            msg << m_InfoClassManagers[i]->toString();
    }

    msg << ")";
    return msg.toString();
}
