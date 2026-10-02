#include "LoginCharacterList.h"

#include "DatabaseError.h"
#include "LCPCList.h"
#include "repository/LoginCharacterRepository.h"

namespace de {

std::unique_ptr<LCPCList> makeLoginCharacterList(WorldID_t WorldID, const std::string& account,
                                                 LoginCharacterRepository& repo) {
    auto reply = std::make_unique<LCPCList>();

    try {
        // Every ACTIVE Slayer row of the account is a character; its Race
        // column says which table holds the rest.
        vector<LoginSlayerListRow> slayers = repo.loadSlayerList(WorldID, account);

        DWORD shape;
        Color_t colors[PCSlayerInfo::SLAYER_COLOR_MAX];
        Color_t colorsVamp[PCVampireInfo::VAMPIRE_COLOR_MAX];

        for (size_t n = 0; n < slayers.size(); n++) {
            const LoginSlayerListRow& s = slayers[n];
            string race = s.race;
            string name = s.name;

            if (race == "SLAYER") {
                auto pPCSlayerInfo = std::make_unique<PCSlayerInfo>();

                pPCSlayerInfo->setName(name);
                pPCSlayerInfo->setSlot(s.slot);
                pPCSlayerInfo->setSex(s.sex);
                pPCSlayerInfo->setHairStyle(HAIR_STYLE1);
                pPCSlayerInfo->setHairColor(s.hairColor);
                pPCSlayerInfo->setSkinColor(s.skinColor);
                pPCSlayerInfo->setAdvancementLevel(s.advancementClass);
                pPCSlayerInfo->setSTR(s.str);
                pPCSlayerInfo->setSTRExp(s.strExp);
                pPCSlayerInfo->setDEX(s.dex);
                pPCSlayerInfo->setDEXExp(s.dexExp);
                pPCSlayerInfo->setINT(s.inte);
                pPCSlayerInfo->setINTExp(s.intExp);
                pPCSlayerInfo->setHP(s.hp, s.currentHP);
                pPCSlayerInfo->setMP(s.mp, s.currentMP);
                pPCSlayerInfo->setFame(s.fame);

                for (int j = 0; j < SKILL_DOMAIN_VAMPIRE; j++) {
                    pPCSlayerInfo->setSkillDomainLevel((SkillDomain)j, (SkillLevel_t)s.domainLevel[j]);
                }

                pPCSlayerInfo->setAlignment(s.alignment);

                shape = s.shape;

                colors[PCSlayerInfo::SLAYER_COLOR_HAIR] = pPCSlayerInfo->getHairColor();
                colors[PCSlayerInfo::SLAYER_COLOR_SKIN] = pPCSlayerInfo->getSkinColor();
                colors[PCSlayerInfo::SLAYER_COLOR_HELMET] = s.helmetColor;
                colors[PCSlayerInfo::SLAYER_COLOR_JACKET] = s.jacketColor;
                colors[PCSlayerInfo::SLAYER_COLOR_PANTS] = s.pantsColor;
                colors[PCSlayerInfo::SLAYER_COLOR_WEAPON] = s.weaponColor;
                colors[PCSlayerInfo::SLAYER_COLOR_SHIELD] = s.shieldColor;

                pPCSlayerInfo->setShapeInfo(shape, colors);
                pPCSlayerInfo->setRank(s.rank);

                reply->setPCInfo(pPCSlayerInfo->getSlot(), pPCSlayerInfo.get());
                pPCSlayerInfo.release();
            } else if (race == "VAMPIRE") {
                LoginVampireListRow v;

                if (!repo.loadVampireListRow(WorldID, account, name, v)) {
                    throw DisconnectException("No Vampire");
                }

                auto pPCVampireInfo = std::make_unique<PCVampireInfo>();

                pPCVampireInfo->setName(v.name);
                pPCVampireInfo->setSlot(v.slot);
                pPCVampireInfo->setSex(v.sex);
                pPCVampireInfo->setBatColor(v.batColor);
                pPCVampireInfo->setSkinColor(v.skinColor);
                pPCVampireInfo->setAdvancementLevel(v.advancementClass);
                pPCVampireInfo->setSTR(v.str);
                pPCVampireInfo->setDEX(v.dex);
                pPCVampireInfo->setINT(v.inte);
                pPCVampireInfo->setHP(v.hp, v.currentHP);
                pPCVampireInfo->setRank(v.rank);
                pPCVampireInfo->setExp(v.goalExp);
                pPCVampireInfo->setLevel(v.level);
                pPCVampireInfo->setBonus(v.bonus);
                pPCVampireInfo->setFame(v.fame);
                pPCVampireInfo->setAlignment(v.alignment);

                shape = v.shape;
                colorsVamp[0] = v.coatColor;

                pPCVampireInfo->setShapeInfo(shape, colorsVamp);

                reply->setPCInfo(pPCVampireInfo->getSlot(), pPCVampireInfo.get());
                pPCVampireInfo.release();
            } else {
                LoginOustersListRow o;

                if (!repo.loadOustersListRow(WorldID, account, name, o)) {
                    throw DisconnectException("No Ousters");
                }

                auto pPCOustersInfo = std::make_unique<PCOustersInfo>();

                pPCOustersInfo->setName(o.name);
                pPCOustersInfo->setSlot(o.slot);
                pPCOustersInfo->setSex(o.sex);
                pPCOustersInfo->setAdvancementLevel(o.advancementClass);
                pPCOustersInfo->setSTR(o.str);
                pPCOustersInfo->setDEX(o.dex);
                pPCOustersInfo->setINT(o.inte);
                pPCOustersInfo->setHP(o.hp, o.currentHP);
                pPCOustersInfo->setRank(o.rank);
                pPCOustersInfo->setExp(o.exp);
                pPCOustersInfo->setLevel(o.level);
                pPCOustersInfo->setBonus(o.bonus);
                pPCOustersInfo->setSkillBonus(o.skillBonus);
                pPCOustersInfo->setFame(o.fame);
                pPCOustersInfo->setAlignment(o.alignment);
                pPCOustersInfo->setCoatType((OustersCoatType)o.coatType);
                pPCOustersInfo->setArmType((OustersArmType)o.armType);
                pPCOustersInfo->setCoatColor(o.coatColor);
                pPCOustersInfo->setHairColor(o.hairColor);
                pPCOustersInfo->setArmColor(o.armColor);
                pPCOustersInfo->setBootsColor(o.bootsColor);

                reply->setPCInfo(pPCOustersInfo->getSlot(), pPCOustersInfo.get());
                pPCOustersInfo.release();
            }
        }
    } catch (const DatabaseError& error) {
        // Keep the established disconnect diagnostic, including its label.
        throw DisconnectException("LoginPlayer::makePCList : " + error.message());
    }
    return reply;
}

} // namespace de
