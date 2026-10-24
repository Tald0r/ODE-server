//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemLoaderManager.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ItemLoaderManager.h"

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
#include "Glove.h"
#include "Helm.h"
#include "HolyWater.h"
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
#include "Ousters.h"
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
#include "Slayer.h"
#include "SlayerPortalItem.h"
#include "Sweeper.h"
#include "Sword.h"
#include "TrapItem.h"
#include "Trouser.h"
#include "Vampire.h"
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
#include "Zone.h"
#include "item/MoonCard.h"


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
ItemLoaderManager::ItemLoaderManager()

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
ItemLoaderManager::~ItemLoaderManager()

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void ItemLoaderManager::init()

{
    m_pItemLoaders[Item::ITEM_CLASS_AR] = new ARLoader();
    m_pItemLoaders[Item::ITEM_CLASS_BELT] = new BeltLoader();
    m_pItemLoaders[Item::ITEM_CLASS_BLADE] = new BladeLoader();
    m_pItemLoaders[Item::ITEM_CLASS_BOMB] = new BombLoader();
    m_pItemLoaders[Item::ITEM_CLASS_BOMB_MATERIAL] = new BombMaterialLoader();
    m_pItemLoaders[Item::ITEM_CLASS_BRACELET] = new BraceletLoader();
    m_pItemLoaders[Item::ITEM_CLASS_COAT] = new CoatLoader();
    m_pItemLoaders[Item::ITEM_CLASS_CROSS] = new CrossLoader();
    m_pItemLoaders[Item::ITEM_CLASS_MACE] = new MaceLoader();
    m_pItemLoaders[Item::ITEM_CLASS_ETC] = new ETCLoader();
    m_pItemLoaders[Item::ITEM_CLASS_GLOVE] = new GloveLoader();
    m_pItemLoaders[Item::ITEM_CLASS_HELM] = new HelmLoader();
    m_pItemLoaders[Item::ITEM_CLASS_HOLYWATER] = new HolyWaterLoader();
    m_pItemLoaders[Item::ITEM_CLASS_KEY] = new KeyLoader();
    m_pItemLoaders[Item::ITEM_CLASS_LEARNINGITEM] = new LearningItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_MAGAZINE] = new MagazineLoader();
    m_pItemLoaders[Item::ITEM_CLASS_MINE] = new MineLoader();
    m_pItemLoaders[Item::ITEM_CLASS_MONEY] = new MoneyLoader();
    m_pItemLoaders[Item::ITEM_CLASS_MOTORCYCLE] = new MotorcycleLoader();
    m_pItemLoaders[Item::ITEM_CLASS_NECKLACE] = new NecklaceLoader();
    m_pItemLoaders[Item::ITEM_CLASS_POTION] = new PotionLoader();
    m_pItemLoaders[Item::ITEM_CLASS_RING] = new RingLoader();
    m_pItemLoaders[Item::ITEM_CLASS_SG] = new SGLoader();
    m_pItemLoaders[Item::ITEM_CLASS_SMG] = new SMGLoader();
    m_pItemLoaders[Item::ITEM_CLASS_SHIELD] = new ShieldLoader();
    m_pItemLoaders[Item::ITEM_CLASS_SHOES] = new ShoesLoader();
    m_pItemLoaders[Item::ITEM_CLASS_SWORD] = new SwordLoader();
    m_pItemLoaders[Item::ITEM_CLASS_SR] = new SRLoader();
    m_pItemLoaders[Item::ITEM_CLASS_TROUSER] = new TrouserLoader();
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_BRACELET] = new VampireBraceletLoader();
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_COAT] = new VampireCoatLoader();
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_NECKLACE] = new VampireNecklaceLoader();
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_RING] = new VampireRingLoader();
    m_pItemLoaders[Item::ITEM_CLASS_WATER] = new WaterLoader();
    m_pItemLoaders[Item::ITEM_CLASS_SKULL] = new SkullLoader();
    m_pItemLoaders[Item::ITEM_CLASS_SERUM] = new SerumLoader();
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_ETC] = new VampireETCLoader();
    m_pItemLoaders[Item::ITEM_CLASS_SLAYER_PORTAL_ITEM] = new SlayerPortalItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM] = new VampirePortalItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_GIFT_BOX] = new EventGiftBoxLoader();
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_STAR] = new EventStarLoader();
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_EARRING] = new VampireEarringLoader();
    m_pItemLoaders[Item::ITEM_CLASS_RELIC] = new RelicLoader();
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_WEAPON] = new VampireWeaponLoader();
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_AMULET] = new VampireAmuletLoader();
    m_pItemLoaders[Item::ITEM_CLASS_QUEST_ITEM] = new QuestItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_TREE] = new EventTreeLoader();
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_ETC] = new EventETCLoader();
    m_pItemLoaders[Item::ITEM_CLASS_BLOOD_BIBLE] = new BloodBibleLoader();
    m_pItemLoaders[Item::ITEM_CLASS_CASTLE_SYMBOL] = new CastleSymbolLoader();
    m_pItemLoaders[Item::ITEM_CLASS_COUPLE_RING] = new CoupleRingLoader();
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_COUPLE_RING] = new VampireCoupleRingLoader();
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_ITEM] = new EventItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_DYE_POTION] = new DyePotionLoader();
    m_pItemLoaders[Item::ITEM_CLASS_RESURRECT_ITEM] = new ResurrectItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_MIXING_ITEM] = new MixingItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_ARMSBAND] = new OustersArmsbandLoader();
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_BOOTS] = new OustersBootsLoader();
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_CHAKRAM] = new OustersChakramLoader();
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_CIRCLET] = new OustersCircletLoader();
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_COAT] = new OustersCoatLoader();
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_PENDENT] = new OustersPendentLoader();
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_RING] = new OustersRingLoader();
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_STONE] = new OustersStoneLoader();
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_WRISTLET] = new OustersWristletLoader();
    m_pItemLoaders[Item::ITEM_CLASS_LARVA] = new LarvaLoader();
    m_pItemLoaders[Item::ITEM_CLASS_PUPA] = new PupaLoader();
    m_pItemLoaders[Item::ITEM_CLASS_COMPOS_MEI] = new ComposMeiLoader();
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM] = new OustersSummonItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_EFFECT_ITEM] = new EffectItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_CODE_SHEET] = new CodeSheetLoader();
    m_pItemLoaders[Item::ITEM_CLASS_MOON_CARD] = new MoonCardLoader();
    m_pItemLoaders[Item::ITEM_CLASS_SWEEPER] = new SweeperLoader();
    m_pItemLoaders[Item::ITEM_CLASS_PET_ITEM] = new PetItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_PET_FOOD] = new PetFoodLoader();
    m_pItemLoaders[Item::ITEM_CLASS_PET_ENCHANT_ITEM] = new PetEnchantItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_LUCKY_BAG] = new LuckyBagLoader();
    m_pItemLoaders[Item::ITEM_CLASS_SMS_ITEM] = new SMSItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_CORE_ZAP] = new CoreZapLoader();
    m_pItemLoaders[Item::ITEM_CLASS_TRAP_ITEM] = new TrapItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_WAR_ITEM] = new WarItemLoader();
    m_pItemLoaders[Item::ITEM_CLASS_CARRYING_RECEIVER] = new CarryingReceiverLoader();
    m_pItemLoaders[Item::ITEM_CLASS_SHOULDER_ARMOR] = new ShoulderArmorLoader();
    m_pItemLoaders[Item::ITEM_CLASS_DERMIS] = new DermisLoader();
    m_pItemLoaders[Item::ITEM_CLASS_PERSONA] = new PersonaLoader();
    m_pItemLoaders[Item::ITEM_CLASS_FASCIA] = new FasciaLoader();
    m_pItemLoaders[Item::ITEM_CLASS_MITTEN] = new MittenLoader();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void ItemLoaderManager::load(Slayer* pSlayer)

{
    // The motorcycle has to be loaded first: it carries the items below.
    m_pItemLoaders[Item::ITEM_CLASS_MOTORCYCLE]->load(pSlayer);

    // The belt comes next, for the same reason.
    m_pItemLoaders[Item::ITEM_CLASS_BELT]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_MAGAZINE]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_POTION]->load(pSlayer);

    m_pItemLoaders[Item::ITEM_CLASS_AR]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_BLADE]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_BOMB]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_BOMB_MATERIAL]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_BRACELET]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_COAT]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_CROSS]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_MACE]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_ETC]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_GLOVE]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_HELM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_HOLYWATER]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_KEY]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_LEARNINGITEM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_MINE]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_MONEY]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_NECKLACE]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_RING]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_SG]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_SMG]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_SHIELD]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_SHOES]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_SWORD]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_SR]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_TROUSER]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_BRACELET]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_COAT]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_NECKLACE]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_RING]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_WATER]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_SKULL]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_SERUM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_ETC]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_SLAYER_PORTAL_ITEM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_GIFT_BOX]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_STAR]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_EARRING]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_RELIC]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_WEAPON]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_AMULET]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_QUEST_ITEM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_TREE]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_ETC]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_BLOOD_BIBLE]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_CASTLE_SYMBOL]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_COUPLE_RING]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_ITEM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_DYE_POTION]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_RESURRECT_ITEM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_MIXING_ITEM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_EFFECT_ITEM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_CODE_SHEET]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_MOON_CARD]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_SWEEPER]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_PET_ITEM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_PET_FOOD]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_PET_ENCHANT_ITEM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_LUCKY_BAG]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_SMS_ITEM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_CORE_ZAP]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_TRAP_ITEM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_WAR_ITEM]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_CARRYING_RECEIVER]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_SHOULDER_ARMOR]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_DERMIS]->load(pSlayer);
    m_pItemLoaders[Item::ITEM_CLASS_PERSONA]->load(pSlayer);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void ItemLoaderManager::load(Vampire* pVampire)

{
    // A vampire has no motorcycle, so there is none to load.
    // The belt still has to be loaded first.
    m_pItemLoaders[Item::ITEM_CLASS_BELT]->load(pVampire);

    m_pItemLoaders[Item::ITEM_CLASS_AR]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_BLADE]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_BOMB]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_BOMB_MATERIAL]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_BRACELET]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_COAT]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_CROSS]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_MACE]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_ETC]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_GLOVE]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_HELM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_HOLYWATER]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_KEY]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_LEARNINGITEM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_MAGAZINE]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_MINE]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_MONEY]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_NECKLACE]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_POTION]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_RING]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_SG]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_SMG]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_SHIELD]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_SHOES]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_SWORD]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_SR]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_TROUSER]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_BRACELET]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_COAT]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_NECKLACE]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_RING]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_WATER]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_SKULL]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_SERUM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_ETC]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_SLAYER_PORTAL_ITEM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_GIFT_BOX]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_STAR]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_EARRING]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_RELIC]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_WEAPON]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_AMULET]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_QUEST_ITEM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_TREE]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_ETC]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_BLOOD_BIBLE]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_CASTLE_SYMBOL]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_VAMPIRE_COUPLE_RING]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_ITEM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_DYE_POTION]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_RESURRECT_ITEM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_MIXING_ITEM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_EFFECT_ITEM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_CODE_SHEET]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_MOON_CARD]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_SWEEPER]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_PET_ITEM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_PET_FOOD]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_PET_ENCHANT_ITEM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_LUCKY_BAG]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_SMS_ITEM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_CORE_ZAP]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_TRAP_ITEM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_WAR_ITEM]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_CARRYING_RECEIVER]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_SHOULDER_ARMOR]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_DERMIS]->load(pVampire);
    m_pItemLoaders[Item::ITEM_CLASS_PERSONA]->load(pVampire);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void ItemLoaderManager::load(Ousters* pOusters)

{
    // The armsband is the ousters' belt, so it is loaded first: a larva, a
    // pupa or a compos mei goes into it.
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_ARMSBAND]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_BOOTS]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_CHAKRAM]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_CIRCLET]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_COAT]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_PENDENT]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_RING]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_STONE]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_WRISTLET]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_LARVA]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_PUPA]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_COMPOS_MEI]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_SKULL]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_QUEST_ITEM]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_GIFT_BOX]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_STAR]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_TREE]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_ETC]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_EVENT_ITEM]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_DYE_POTION]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_RESURRECT_ITEM]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_MIXING_ITEM]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_EFFECT_ITEM]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_CODE_SHEET]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_MOON_CARD]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_SWEEPER]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_PET_ITEM]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_PET_FOOD]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_PET_ENCHANT_ITEM]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_LUCKY_BAG]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_SMS_ITEM]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_CORE_ZAP]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_TRAP_ITEM]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_WAR_ITEM]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_FASCIA]->load(pOusters);
    m_pItemLoaders[Item::ITEM_CLASS_MITTEN]->load(pOusters);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void ItemLoaderManager::load(Zone* pZone)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string ItemLoaderManager::toString() const

{
    StringStream msg;

    msg << "ItemLoaderManager(";

    for (uint i = 0; i < Item::ITEM_CLASS_MAX; i++) {
        if (m_pItemLoaders[i] == NULL) {
            msg << "NULL";
        } else {
            msg << m_pItemLoaders[i]->getItemClassName();
        }
    }

    msg << ")";

    return msg.toString();
}
