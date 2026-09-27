//////////////////////////////////////////////////////////////////////////////
// Filename    : MonsterInfo.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "MonsterInfo.h"

#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>

#include "EffectDetectInvisibility.h"
#include "EffectObservingEye.h"
#include "KernelContext.h"
#include "MonsterInfoTypes.h"
#include "MonsterSummonInfo.h"
#include "Properties.h"
#include "SXml.h"
#include "StringStream.h"
#include "Treasure.h"
#include "repository/ContentInfoRepository.h"

int DefaultClanID[CLAN_MAX] = {
    0, // CLAN_NONE,                    // Belongs to no clan          0
    1, // CLAN_VAMPIRE_MONSTER,         // Wandering vampire monster   1
    2, // CLAN_VAMPIRE_BATHORY_MONSTER, // Bathory clan monster        2
    3, // CLAN_VAMPIRE_TEPEZ_MONSTER,   // Tepez clan monster          3
    4, // CLAN_SLAYER_MONSTER,          // Slayer monster              4
};

extern int DefaultClanID[CLAN_MAX];

//////////////////////////////////////////////////////////////////////////////
// functions
//////////////////////////////////////////////////////////////////////////////

void saveTreasure(const string& filename, TreasureList* pTreasureList) {
    ofstream file(filename.c_str(), ios::out | ios::binary | ios::trunc);
    if (!file)
        return;

    const list<Treasure*>& TList = pTreasureList->getTreasures();
    int TreasureCount = TList.size();

    file.write((char*)&TreasureCount, sizeof(int));

    list<Treasure*>::const_iterator itr = TList.begin();
    for (; itr != TList.end(); itr++) {
        Treasure* pTreasure = *itr;

        int ItemRatio = pTreasure->m_ItemRatio;
        int OptionRatio = pTreasure->m_OptionRatio;

        file.write((char*)&ItemRatio, sizeof(int));
        file.write((char*)&OptionRatio, sizeof(int));

        vector<TreasureItemClass*> TICs = pTreasure->m_TreasureItemClasses;
        int ItemClassCount = TICs.size();

        file.write((char*)&ItemClassCount, sizeof(int));

        for (uint i = 0; i < TICs.size(); i++) {
            TreasureItemClass* pTIC = TICs[i];

            int ItemClass = (int)pTIC->m_ItemClass;
            int ItemClassRatio = pTIC->m_Ratio;

            file.write((char*)&ItemClass, sizeof(int));
            file.write((char*)&ItemClassRatio, sizeof(int));

            vector<TreasureItemType*> TITs = pTIC->m_TreasureItemTypes;
            int ItemTypeCount = TITs.size();

            file.write((char*)&ItemTypeCount, sizeof(int));

            for (uint j = 0; j < TITs.size(); j++) {
                TreasureItemType* pTIT = TITs[j];

                int ItemType = pTIT->m_ItemType;
                int ItemTypeRatio = pTIT->m_Ratio;

                file.write((char*)&ItemType, sizeof(int));
                file.write((char*)&ItemTypeRatio, sizeof(int));

                vector<TreasureOptionType*> TOTs = pTIT->m_TreasureOptionTypes;
                int OptionTypeCount = TOTs.size();

                file.write((char*)&OptionTypeCount, sizeof(int));

                for (uint k = 0; k < TOTs.size(); k++) {
                    TreasureOptionType* pTOT = TOTs[k];

                    int OptionType = pTOT->m_OptionType;
                    int OptionRatio = pTOT->m_Ratio;

                    file.write((char*)&OptionType, sizeof(int));
                    file.write((char*)&OptionRatio, sizeof(int));
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// class MonsterInfo member methods
//////////////////////////////////////////////////////////////////////////////

MonsterInfo::MonsterInfo()

{
    __BEGIN_TRY

    m_MonsterType = 0;
    m_SpriteType = 0;
    m_HName = "";
    m_EName = "";
    m_Level = 0;
    m_STR = 0;
    m_DEX = 0;
    m_INT = 0;
    m_BodySize = BODYSIZE_SMALL;
    m_HP = 0;
    m_Exp = 0;
    m_MainColor = 0;
    m_SubColor = 0;
    m_Alignment = ALIGNMENT_FRIENDLY;
    m_AttackOrder = ATTACK_FIRST;
    m_Moral = 0;
    m_Delay = 0;
    m_AttackDelay = 0;
    m_Sight = 0;
    m_MoveMode = Creature::MOVE_MODE_WALKING;
    m_AIType = 0;

    m_EnhanceHP = 0;
    m_EnhanceToHit = 0;
    m_EnhanceDefense = 0;
    m_EnhanceProtection = 0;
    m_EnhanceMinDamage = 0;
    m_EnhanceMaxDamage = 0;

    m_pSlayerTreasureList = NULL;  // new TreasureList;
    m_pVampireTreasureList = NULL; // new TreasureList;
    m_pOustersTreasureList = NULL; // new TreasureList;

    m_RegenType[REGENTYPE_HIDE] = 128;
    m_RegenType[REGENTYPE_PORTAL] = 0;
    m_RegenType[REGENTYPE_INVISIBLE] = 0;
    m_RegenType[REGENTYPE_BAT] = 0;

    m_UnburrowChance = 128;
    m_bMaster = false;
    m_ClanType = CLAN_VAMPIRE_MONSTER;
    m_pMonsterSummonInfo = NULL;

    // 2002.10.16 by sigi
    m_bNormalRegen = true;

    __END_CATCH
}

MonsterInfo::~MonsterInfo()

{
    __BEGIN_TRY

    // This is managed by MonsterInfo.
    // SAFE_DELETE(m_pSlayerTreasureList);
    // SAFE_DELETE(m_pVampireTreasureList);

    m_pSlayerTreasureList = NULL;
    m_pVampireTreasureList = NULL;
    m_pOustersTreasureList = NULL;

    SAFE_DELETE(m_pMonsterSummonInfo);

    __END_CATCH_NO_RETHROW
}

void MonsterInfo::setRegenType(RegenType rt, int percent) {
    m_RegenType[rt] = percent;

    // Changing the REGENTYPE_HIDE probability so the total came to 100% was
    // considered, but it did not seem to be worth it.
}

//---------------------------------------------------------------------------
// select RegenType
//---------------------------------------------------------------------------
// Check the Portal, Invisible and Bat probabilities in order; whatever is left is Hide.
//---------------------------------------------------------------------------
RegenType MonsterInfo::selectRegenType() const {
    // To gain a little speed,
    // this is out of 128 rather than out of 100.
    int dice = rand() & 0x0000007F; // rand()%100;

    int acc = 0;
    const int REGENTYPE_MAX_1 = REGENTYPE_MAX - 1;
    for (int i = 0; i < REGENTYPE_MAX_1; i++) {
        // Accumulate each probability and check against it.
        acc += m_RegenType[i];
        if (dice < acc) {
            return (RegenType)i;
        }
    }

    return REGENTYPE_HIDE;
}

void MonsterInfo::setMoveMode(const string& moveMode)

{
    __BEGIN_TRY

    if (moveMode == "WALK")
        m_MoveMode = Creature::MOVE_MODE_WALKING;
    else if (moveMode == "FLY")
        m_MoveMode = Creature::MOVE_MODE_FLYING;
    else if (moveMode == "BURROW")
        m_MoveMode = Creature::MOVE_MODE_BURROWING;
    else
        throw Error("invalid move mode");

    __END_CATCH
}

void MonsterInfo::parseEnhanceAttr(const string& enhance)

{
    m_EnhanceHP = 0;
    m_EnhanceToHit = 0;
    m_EnhanceDefense = 0;
    m_EnhanceProtection = 0;
    m_EnhanceMinDamage = 0;
    m_EnhanceMaxDamage = 0;

    if (enhance.size() < 1)
        return;

    size_t a = 0, b = 0, c = 0;

    while (c < enhance.size() - 1) {
        ///////////////////////////////////////////////////////
        // (HP,1)(DAMAGE,1)...
        // a  b ca
        ///////////////////////////////////////////////////////
        a = enhance.find_first_of('(', c);
        b = enhance.find_first_of(',', a + 1);
        c = enhance.find_first_of(')', b + 1);

        if (a > b || b > c)
            break;

        string part = enhance.substr(a + 1, b - a - 1);
        int value = atoi(enhance.substr(b + 1, c - b - 1).c_str());

        if (part == "HP")
            m_EnhanceHP = value;
        else if (part == "DEFENSE")
            m_EnhanceDefense = value;
        else if (part == "PROTECTION")
            m_EnhanceProtection = value;
        else if (part == "TOHIT")
            m_EnhanceToHit = value;
        else if (part == "DAMAGE")
            m_EnhanceMinDamage = m_EnhanceMaxDamage = value;
        else if (part == "MINDAMAGE")
            m_EnhanceMinDamage = value;
        else if (part == "MAXDAMAGE")
            m_EnhanceMaxDamage = value;
    }
}

void MonsterInfo::parseSlayerTreasureString(const string& text)

{
    __BEGIN_TRY

    string newtext = trim(text);

    m_pSlayerTreasureList->parseString(newtext);

    __END_CATCH
}

void MonsterInfo::setSlayerTreasureList(TreasureList* pTreasureList)

{
    __BEGIN_TRY

    Assert(pTreasureList != NULL);
    // SAFE_DELETE(m_pSlayerTreasureList);
    m_pSlayerTreasureList = pTreasureList;

    __END_CATCH
}

void MonsterInfo::parseVampireTreasureString(const string& text)

{
    __BEGIN_TRY

    string newtext = trim(text);

    m_pVampireTreasureList->parseString(newtext);

    __END_CATCH
}

void MonsterInfo::setVampireTreasureList(TreasureList* pTreasureList)

{
    __BEGIN_TRY

    Assert(pTreasureList != NULL);
    // SAFE_DELETE(m_pVampireTreasureList);
    m_pVampireTreasureList = pTreasureList;

    __END_CATCH
}

void MonsterInfo::parseOustersTreasureString(const string& text)

{
    __BEGIN_TRY

    string newtext = trim(text);

    m_pOustersTreasureList->parseString(newtext);

    __END_CATCH
}

void MonsterInfo::setOustersTreasureList(TreasureList* pTreasureList)

{
    __BEGIN_TRY

    Assert(pTreasureList != NULL);

    m_pOustersTreasureList = pTreasureList;

    __END_CATCH
}

void MonsterInfo::setMonsterSummonInfo(const string& text) {
    SAFE_DELETE(m_pMonsterSummonInfo);

    if (text.size() > 0) {
        m_pMonsterSummonInfo = new MonsterSummonInfo;

        m_pMonsterSummonInfo->parseString(text);

        //		cout << "[SummonInfo] "
        //			<< getHName().c_str() << " : "
        //			<< m_pMonsterSummonInfo->toString().c_str()
        //			<< endl;
    }
}

bool MonsterInfo::getMonsterSummonInfo(int step, SUMMON_INFO2& summonInfo) const {
    if (m_pMonsterSummonInfo == NULL) {
        summonInfo.pMonsters = NULL;
        return false;
    }

    summonInfo.pMonsters = (MonsterCollection*)m_pMonsterSummonInfo->getRandomMonsterCollection(step);

    return true;
}

bool MonsterInfo::hasNextMonsterSummonInfo(int step) const {
    if (m_pMonsterSummonInfo == NULL) {
        return false;
    }

    return m_pMonsterSummonInfo->hasNextMonsterCollection(step);
}

// a                    b
// IMMUNE_TO_BLOOD_DRAIN, IMMUNE_TO_PARALYZE, IMMUNE_TO_POISON, IMMUNE_TO_ACID, IMMUNE_TO_CURSE
void MonsterInfo::setDefaultEffects(const string& text) {
    m_DefaultEffects.clear();

    if (text.size() < 1)
        return;

    size_t a = 0, b = 0;

    while (b < text.size() - 1) {
        b = text.find_first_of(',', a);

        string part = trim(text.substr(a, b - a)); // b-1 -a+1);

        if (part == "IMMUNE_TO_BLOOD_DRAIN")
            m_DefaultEffects.push_back(Effect::EFFECT_CLASS_IMMUNE_TO_BLOOD_DRAIN);
        else if (part == "IMMUNE_TO_PARALYZE")
            m_DefaultEffects.push_back(Effect::EFFECT_CLASS_IMMUNE_TO_PARALYZE);
        else if (part == "IMMUNE_TO_POISON")
            m_DefaultEffects.push_back(Effect::EFFECT_CLASS_IMMUNE_TO_POISON);
        else if (part == "IMMUNE_TO_ACID")
            m_DefaultEffects.push_back(Effect::EFFECT_CLASS_IMMUNE_TO_ACID);
        else if (part == "IMMUNE_TO_CURSE")
            m_DefaultEffects.push_back(Effect::EFFECT_CLASS_IMMUNE_TO_CURSE);
        else if (part == "IMMUNE_TO_HALLUCINATION")
            m_DefaultEffects.push_back(Effect::EFFECT_CLASS_IMMUNE_TO_HALLUCINATION);
        else if (part == "DETECT_INVISIBILITY")
            m_DefaultEffects.push_back(Effect::EFFECT_CLASS_DETECT_INVISIBILITY);
        else if (part == "OBSERVING_EYE")
            m_DefaultEffects.push_back(Effect::EFFECT_CLASS_OBSERVING_EYE);
        else {
            cout << "MonsterInfo::setDefaultEffects() - Wrong EffectClass: " << part.c_str() << endl;
            Assert(false);
        }

        a = b + 1;
    }
}

void MonsterInfo::addDefaultEffects(Creature* pCreature) const {
    list<Effect::EffectClass>::const_iterator itr = m_DefaultEffects.begin();

    for (; itr != m_DefaultEffects.end(); itr++) {
        Effect::EffectClass effectClass = *itr;

        // Only the flag is raised; no Effect object is attached to the creature.
        pCreature->setFlag(effectClass);
    }
}


string MonsterInfo::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "MonsterInfo(" << "MonsterType:" << m_MonsterType << ",SpriteType:" << m_SpriteType << ",HName:" << m_HName
        << ",EName:" << m_EName << ",Level:" << (int)m_Level << ",STR:" << (int)m_STR << ",DEX:" << (int)m_DEX
        << ",INT:" << (int)m_INT << ",HP:" << m_HP << ",Exp:" << m_Exp << ",MainColor:" << (int)m_MainColor
        << ",SubColor:" << (int)m_SubColor << ",Alignment:" << MAlignment2String[m_Alignment]
        << ",AttackOrder:" << AttackOrder2String[m_AttackOrder] << ",Moral:" << (int)m_Moral
        << ",Delay:" << (int)m_Delay << ",Sight:" << (int)m_Sight << ",MoveMode:" << MoveMode2String[m_MoveMode]
        << ",RegenPortal:" << m_RegenType[REGENTYPE_PORTAL] << ",RegenInvisible:" << m_RegenType[REGENTYPE_INVISIBLE]
        << ",RegenBat:" << m_RegenType[REGENTYPE_BAT] << ",RegenHide:" << m_RegenType[REGENTYPE_HIDE]
        << ",UnburrowChance:" << m_UnburrowChance << ",HasTreasure:" << (int)m_bHasTreasure << ")";

    return msg.toString();

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
// class MonsterInfoManager member methods
////////////////////////////////////////////////////////////////////////////////

MonsterInfoManager::MonsterInfoManager()

{
    __BEGIN_TRY

    m_MonsterInfos = NULL;

    __END_CATCH
}

MonsterInfoManager::~MonsterInfoManager()

{
    __BEGIN_TRY

    SAFE_DELETE_ARRAY(m_MonsterInfos);

    __END_CATCH_NO_RETHROW
}

void MonsterInfoManager::init()

{
    __BEGIN_TRY

    load();

    __END_CATCH
}

void MonsterInfoManager::clearTreasures() {
    m_SlayerTreasureLists.clear();
    m_VampireTreasureLists.clear();
    m_OustersTreasureLists.clear();
}

void MonsterInfoManager::load()

{
    __BEGIN_TRY
    Properties& config = de::kernelContext().config();

    __BEGIN_DEBUG

    ContentInfoRepository& repository = defaultContentInfoRepository();

    // MAX(MType) sizes MonsterInfoManager's MonsterInfo* array; an
    // empty table is an error.
    int maxMonsterType = 0;
    if (!repository.loadMaxMonsterType(maxMonsterType)) {
        throw Error("MonsterInfo table is empty.. insert any row.");
    }

    // The client indexes slayers, vampires, monsters and NPCs as one
    // character range, so the array is 4 larger than the real max:
    // 0:Male Slayer, 1:Female Slayer, 2:Male Vampire, 3:Female Vampire
    m_MaxMonsterType = maxMonsterType + 4 + 1;
    m_MonsterInfos = new MonsterInfo*[m_MaxMonsterType];

    for (uint i = 0; i < m_MaxMonsterType; i++)
        m_MonsterInfos[i] = NULL;

    // Read every monster row into a MonsterInfo and add it.
    vector<MonsterInfoRow> rows = repository.loadMonsterInfos();

    for (size_t r = 0; r < rows.size(); r++) {
        const MonsterInfoRow& row = rows[r];

        MonsterInfo* pMonsterInfo = new MonsterInfo();

        pMonsterInfo->setMonsterType(row.monsterType);
        pMonsterInfo->setSpriteType(row.spriteType);
        pMonsterInfo->setHName(row.hName);
        pMonsterInfo->setEName(row.eName);
        pMonsterInfo->setLevel(row.level);
        pMonsterInfo->setSTR(row.str);
        pMonsterInfo->setDEX(row.dex);
        pMonsterInfo->setINT(row.inte);
        pMonsterInfo->setBodySize(row.bodySize);
        pMonsterInfo->setExp(row.exp);
        pMonsterInfo->setMainColor(row.mainColor);
        pMonsterInfo->setSubColor(row.subColor);
        pMonsterInfo->setAlignment((MAlignment)row.alignment);
        pMonsterInfo->setAttackOrder((AttackOrder)row.attackOrder);
        pMonsterInfo->setMoral(row.moral);
        pMonsterInfo->setDelay(row.delay);
        pMonsterInfo->setAttackDelay(row.attackDelay);
        pMonsterInfo->setSight(row.sight);
        pMonsterInfo->setMeleeRange(row.meleeRange);
        pMonsterInfo->setMissileRange(row.missileRange);

        int nPortal = row.regenPortal;
        int nInvisible = row.regenInvisible;
        int nBat = row.regenBat;

        pMonsterInfo->setRegenType(REGENTYPE_PORTAL, nPortal);
        pMonsterInfo->setRegenType(REGENTYPE_INVISIBLE, nInvisible);
        pMonsterInfo->setRegenType(REGENTYPE_BAT, nBat);

        pMonsterInfo->setMoveMode(row.moveMode);
        pMonsterInfo->setAIType(row.aiType);
        pMonsterInfo->parseEnhanceAttr(row.enhance);
        pMonsterInfo->setUnburrowChance(row.unburrowChance);
        pMonsterInfo->setMaster(row.master != 0);
        pMonsterInfo->setClanType(row.clanType);
        pMonsterInfo->setDefaultEffects(row.defaultEffects);
        pMonsterInfo->setChief(row.chief != 0);
        pMonsterInfo->setNormalRegen(row.normalRegen != 0);
        pMonsterInfo->setHasTreasure(row.hasTreasure != 0);
        pMonsterInfo->setMonsterClass(row.monsterClass);
        pMonsterInfo->setSkullType(row.skullType);

        addMonsterInfo(pMonsterInfo->getMonsterType(), pMonsterInfo);
    }

    // MonsterSummonInfo is set in a second pass: it compares names, so
    // every monster must already be loaded.
    vector<MonsterSummonRow> summons = repository.loadMonsterSummonInfos();

    for (size_t s = 0; s < summons.size(); s++) {
        MonsterInfo* pMonsterInfo = m_MonsterInfos[summons[s].monsterType];
        Assert(pMonsterInfo != NULL);

        pMonsterInfo->setMonsterSummonInfo(summons[s].summonInfo);
    }


    clearTreasures();

    for (uint i = 0; i < m_MaxMonsterType; i++) {
        MonsterInfo* pInfo = m_MonsterInfos[i];
        if (pInfo != NULL) {
            if (pInfo->hasTreasure()) {
                string slayer_filename;
                string vampire_filename;
                string ousters_filename;

                if (pInfo->getMonsterClass() == 0) {
                    slayer_filename = config.getProperty("HomePath") + "/data/" + pInfo->getEName() + ".slayer.bin";
                    vampire_filename = config.getProperty("HomePath") + "/data/" + pInfo->getEName() + ".vampire.bin";
                    ousters_filename = config.getProperty("HomePath") + "/data/" + pInfo->getEName() + ".ousters.bin";
                } else {
                    char buffer[10];
                    snprintf(buffer, sizeof(buffer), "Class%d", pInfo->getMonsterClass());
                    slayer_filename = config.getProperty("HomePath") + "/data/" + buffer + ".slayer.bin";
                    vampire_filename = config.getProperty("HomePath") + "/data/" + buffer + ".vampire.bin";
                    ousters_filename = config.getProperty("HomePath") + "/data/" + buffer + ".ousters.bin";
                }

                TreasureList* pSlayerTreasureList = m_SlayerTreasureLists.getTreasure(slayer_filename);
                TreasureList* pVampireTreasureList = m_VampireTreasureLists.getTreasure(vampire_filename);
                TreasureList* pOustersTreasureList = m_OustersTreasureLists.getTreasure(ousters_filename);

                if (pSlayerTreasureList == NULL) {
                    printf("Load - slayer_filename:%s\n", slayer_filename.c_str());
                    pSlayerTreasureList = m_SlayerTreasureLists.loadTreasure(slayer_filename);
                    Assert(pSlayerTreasureList != NULL);
                }

                if (pVampireTreasureList == NULL) {
                    printf("Load - vampire_filename:%s\n", vampire_filename.c_str());
                    pVampireTreasureList = m_VampireTreasureLists.loadTreasure(vampire_filename);
                    Assert(pVampireTreasureList != NULL);
                }

                if (pOustersTreasureList == NULL) {
                    printf("Load - ousters_filename:%s\n", ousters_filename.c_str());
                    pOustersTreasureList = m_OustersTreasureLists.loadTreasure(ousters_filename);
                    Assert(pOustersTreasureList != NULL);
                }

                pInfo->setSlayerTreasureList(pSlayerTreasureList);
                pInfo->setVampireTreasureList(pVampireTreasureList);
                pInfo->setOustersTreasureList(pOustersTreasureList);

                // verify
                // cout << "MonsterType:" << pInfo->getMonsterType()
                //	<< ",MonsterName:" << pInfo->getEName()
                //	<< ",SlayerTreasure:" << pInfo->getSlayerTreasureList()->getTreasures().size()
                //	<< ",VampireTreasure:" << pInfo->getVampireTreasureList()->getTreasures().size()
                //	<< endl;
            } else {
                cout << "[NoTreasureFile] " << pInfo->getEName().c_str() << endl;
            }
        }
    }

    unordered_map<int, vector<SpriteType_t>>::iterator itr = m_MonsterClassMap.begin();
    unordered_map<int, vector<SpriteType_t>>::iterator endItr = m_MonsterClassMap.end();

    for (; itr != endItr; ++itr) {
        cout << "Class[" << itr->first << "] : ";
        copy(itr->second.begin(), itr->second.end(), ostream_iterator<SpriteType_t>(cout, ", "));
        cout << endl;
    }

    __END_DEBUG
    __END_CATCH
}


void MonsterInfoManager::reload(MonsterType_t monsterType)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    bool bLoadAll = (monsterType == 0);

    ContentInfoRepository& repository = defaultContentInfoRepository();

    // Every row unless one MonsterType was asked for.
    vector<MonsterReloadRow> rows =
        bLoadAll ? repository.loadMonsterInfosForReload() : repository.loadMonsterInfoForReload(monsterType);

    for (size_t r = 0; r < rows.size(); r++) {
        const MonsterReloadRow& row = rows[r];

        int MType = row.monsterType;

        MonsterInfo* pMonsterInfo = (MonsterInfo*)getMonsterInfo(MType);

        if (pMonsterInfo != NULL) {
            pMonsterInfo->setSpriteType(row.spriteType);
            pMonsterInfo->setHName(row.hName);
            pMonsterInfo->setEName(row.eName);
            pMonsterInfo->setLevel(row.level);
            pMonsterInfo->setSTR(row.str);
            pMonsterInfo->setDEX(row.dex);
            pMonsterInfo->setINT(row.inte);
            pMonsterInfo->setBodySize(row.bodySize);
            pMonsterInfo->setExp(row.exp);
            pMonsterInfo->setMainColor(row.mainColor);
            pMonsterInfo->setSubColor(row.subColor);
            pMonsterInfo->setAlignment((MAlignment)row.alignment);
            pMonsterInfo->setAttackOrder((AttackOrder)row.attackOrder);
            pMonsterInfo->setMoral(row.moral);
            pMonsterInfo->setDelay(row.delay);
            pMonsterInfo->setAttackDelay(row.attackDelay);
            pMonsterInfo->setSight(row.sight);
            pMonsterInfo->setMeleeRange(row.meleeRange);
            pMonsterInfo->setMissileRange(row.missileRange);

            int nPortal = row.regenPortal;
            int nInvisible = row.regenInvisible;
            int nBat = row.regenBat;

            pMonsterInfo->setRegenType(REGENTYPE_PORTAL, nPortal);
            pMonsterInfo->setRegenType(REGENTYPE_INVISIBLE, nInvisible);
            pMonsterInfo->setRegenType(REGENTYPE_BAT, nBat);

            pMonsterInfo->setMoveMode(row.moveMode);
            pMonsterInfo->setAIType(row.aiType);
            pMonsterInfo->parseEnhanceAttr(row.enhance);
            pMonsterInfo->setUnburrowChance(row.unburrowChance);
            pMonsterInfo->setMaster(row.master != 0);
            pMonsterInfo->setClanType(row.clanType);
            pMonsterInfo->setMonsterSummonInfo(row.monsterSummonInfo);
            pMonsterInfo->setDefaultEffects(row.defaultEffects);
            pMonsterInfo->setNormalRegen(row.normalRegen != 0);
        }
    }

    __END_DEBUG
    __END_CATCH
}

void MonsterInfoManager::addMonsterInfo(MonsterType_t monsterType, MonsterInfo* pMonsterInfo) {
    __BEGIN_TRY

    if (monsterType >= m_MaxMonsterType) {
        cerr << "MonsterInfoManager::addMonsterInfo() : Out of bounds" << endl;
        throw OutOfBoundException();
    }

    if (m_MonsterInfos[monsterType] != NULL) {
        cerr << "MonsterInfoManager::addMonsterInfo() : Duplicated Exception" << endl;
        throw DuplicatedException();
    }

    m_MonsterInfos[monsterType] = pMonsterInfo;

    if (pMonsterInfo->isNormalRegen()) {
        m_MonsterSpriteSet[pMonsterInfo->getSpriteType()].push_back(monsterType);
    }

    m_MonsterSpriteTypes[pMonsterInfo->getHName()] = pMonsterInfo->getSpriteType();

    if (pMonsterInfo->isChief()) {
        m_ChiefMonster[pMonsterInfo->getHName()] = pMonsterInfo->getMonsterType();
    }

    if (pMonsterInfo->getMonsterClass() != 0 && pMonsterInfo->isNormalRegen()) {
        vector<SpriteType_t>& mList = m_MonsterClassMap[pMonsterInfo->getMonsterClass()];
        vector<SpriteType_t>::iterator itr = lower_bound(mList.begin(), mList.end(), pMonsterInfo->getSpriteType());
        if (itr == mList.end() || *itr != pMonsterInfo->getSpriteType())
            mList.insert(itr, pMonsterInfo->getSpriteType());
    }

    __END_CATCH
}

const MonsterInfo* MonsterInfoManager::getMonsterInfo(MonsterType_t monsterType) const {
    __BEGIN_TRY

    if (monsterType >= m_MaxMonsterType) {
        cerr << "MonsterInfoManager::getMonsterInfo() : Out of bounds" << endl;
        throw OutOfBoundException();
    }

    if (m_MonsterInfos[monsterType] == NULL) {
        cerr << "MonsterInfoManager::getMonsterInfo() : No Such Element Exception" << endl;
        throw NoSuchElementException();
    }

    // Once it has been checked above, [] may be used.
    return m_MonsterInfos[monsterType];

    __END_CATCH
}

const vector<MonsterType_t>& MonsterInfoManager::getMonsterTypeBySprite(SpriteType_t spriteType) const {
    __BEGIN_TRY

    if (m_MonsterSpriteSet[spriteType].empty()) {
        cerr << "MonsterInfoManager::getMonsterTypeBySprite() : no such element exception" << (int)spriteType << endl;
        throw NoSuchElementException();
    }

    if (spriteType >= MAX_SPRITE_TYPE) {
        cerr << "MonsterInfoManager::getMonsterTypeBySprite() : out of bounds" << endl;
        throw OutOfBoundException();
    }

    return m_MonsterSpriteSet[spriteType];

    __END_CATCH
}

string MonsterInfoManager::getNameBySpriteType(SpriteType_t spriteType) const {
    __BEGIN_TRY

    if (m_MonsterSpriteSet[spriteType].empty()) {
        cerr << "MonsterInfoManager::getMonsterTypeBySprite() : no such element exception" << (int)spriteType << endl;
        throw NoSuchElementException();
    }

    if (spriteType >= MAX_SPRITE_TYPE) {
        cerr << "MonsterInfoManager::getMonsterTypeBySprite() : out of bounds" << endl;
        throw OutOfBoundException();
    }

    return m_MonsterInfos[m_MonsterSpriteSet[spriteType].front()]->getHName();

    __END_CATCH
}

SpriteType_t MonsterInfoManager::getSpriteTypeByName(const string& monsterName) const {
    __BEGIN_TRY

    unordered_map<string, SpriteType_t>::const_iterator itr = m_MonsterSpriteTypes.find(monsterName);

    if (itr == m_MonsterSpriteTypes.end()) {
        // cerr << "MonsterInfoManager::getMonsterTypeBySprite() : no such element exception" << endl;
        // throw NoSuchElementException();

        return 0;
    }

    return itr->second;

    __END_CATCH
}

MonsterType_t MonsterInfoManager::getChiefMonsterTypeByName(const string& monsterName) const {
    __BEGIN_TRY

    unordered_map<string, SpriteType_t>::const_iterator itr = m_ChiefMonster.find(monsterName);

    if (itr == m_ChiefMonster.end()) {
        // cerr << "MonsterInfoManager::getMonsterTypeBySprite() : no such element exception" << endl;
        // throw NoSuchElementException();

        return 0;
    }

    return itr->second;

    __END_CATCH
}

MonsterType_t MonsterInfoManager::getRandomMonsterByClass(int minClass, int maxClass) {
    int totalCand = 0;
    for (int i = minClass; i <= maxClass; ++i) {
        totalCand += m_MonsterClassMap[i].size();
    }

    int value = rand() % totalCand;
    SpriteType_t Target;
    int index = minClass;

    while (true) {
        vector<SpriteType_t>& sList = m_MonsterClassMap[index];
        if (value >= sList.size()) {
            value -= sList.size();
            ++index;
        } else {
            Target = sList[value];
            break;
        }
    }

    return getMonsterTypeBySprite(Target).front();
}

string MonsterInfoManager::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "MonsterInfoManager(";

    for (uint i = 0; i < m_MaxMonsterType; i++) {
        if (m_MonsterInfos[i] == NULL)
            msg << "NULL";
        else
            msg << m_MonsterInfos[i]->toString();
    }

    msg << ")";
    return msg.toString();

    __END_CATCH
}
