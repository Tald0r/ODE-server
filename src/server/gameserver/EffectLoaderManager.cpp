//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectLoaderManager.cpp
// Written By  : elca, excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectLoaderManager.h"

#include "EffectAcidSwamp.h"
#include "EffectAftermath.h"
#include "EffectBloodDrain.h"
#include "EffectCanEnterGDRLair.h"
#include "EffectContinualBloodyWall.h"
#include "EffectDarkness.h"
#include "EffectEnemyErase.h"
#include "EffectGreenPoison.h"
#include "EffectIceField.h"
#include "EffectKillAftermath.h"
#include "EffectLight.h"
#include "EffectMute.h"
#include "EffectOnBridge.h"
#include "EffectProminence.h"
#include "EffectRestore.h"
#include "EffectYellowPoison.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"

EffectLoaderManager::EffectLoaderManager()

{}

EffectLoaderManager::~EffectLoaderManager()

{
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_BLOOD_DRAIN]);
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_LIGHT]);
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_AFTERMATH]);
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_ENEMY_ERASE]);
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_RESTORE]);
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_KILL_AFTERMATH]);
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_MUTE]);

    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_GREEN_POISON]);
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_YELLOW_POISON]);
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_DARKNESS]);
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_ACID_SWAMP]);
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_CONTINUAL_BLOODY_WALL]);
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_ICE_FIELD]);
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_PROMINENCE]);

    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_ON_BRIDGE]);
    SAFE_DELETE(m_pEffectLoaders[Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR]);
}

void EffectLoaderManager::init()

{
    m_pEffectLoaders[Effect::EFFECT_CLASS_BLOOD_DRAIN] = new EffectBloodDrainLoader();
    m_pEffectLoaders[Effect::EFFECT_CLASS_LIGHT] = new EffectLightLoader();
    m_pEffectLoaders[Effect::EFFECT_CLASS_AFTERMATH] = new EffectAftermathLoader();
    m_pEffectLoaders[Effect::EFFECT_CLASS_ENEMY_ERASE] = new EffectEnemyEraseLoader();
    m_pEffectLoaders[Effect::EFFECT_CLASS_RESTORE] = new EffectRestoreLoader();
    m_pEffectLoaders[Effect::EFFECT_CLASS_KILL_AFTERMATH] = new EffectKillAftermathLoader();
    m_pEffectLoaders[Effect::EFFECT_CLASS_MUTE] = new EffectMuteLoader();

    m_pEffectLoaders[Effect::EFFECT_CLASS_GREEN_POISON] = new EffectGreenPoisonLoader();
    m_pEffectLoaders[Effect::EFFECT_CLASS_YELLOW_POISON] = new EffectYellowPoisonLoader();
    m_pEffectLoaders[Effect::EFFECT_CLASS_DARKNESS] = new EffectDarknessLoader();
    m_pEffectLoaders[Effect::EFFECT_CLASS_ACID_SWAMP] = new EffectAcidSwampLoader();
    m_pEffectLoaders[Effect::EFFECT_CLASS_CONTINUAL_BLOODY_WALL] = new EffectContinualBloodyWallLoader();
    m_pEffectLoaders[Effect::EFFECT_CLASS_ICE_FIELD] = new EffectIceFieldLoader();
    m_pEffectLoaders[Effect::EFFECT_CLASS_PROMINENCE] = new EffectProminenceLoader();

    m_pEffectLoaders[Effect::EFFECT_CLASS_ON_BRIDGE] = new EffectOnBridgeLoader();
    m_pEffectLoaders[Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR] = new EffectCanEnterGDRLairLoader();
}

void EffectLoaderManager::load(Slayer* pSlayer)

{
    m_pEffectLoaders[Effect::EFFECT_CLASS_BLOOD_DRAIN]->load(pSlayer);
    m_pEffectLoaders[Effect::EFFECT_CLASS_LIGHT]->load(pSlayer);
    m_pEffectLoaders[Effect::EFFECT_CLASS_AFTERMATH]->load(pSlayer);
    m_pEffectLoaders[Effect::EFFECT_CLASS_ENEMY_ERASE]->load(pSlayer);
    m_pEffectLoaders[Effect::EFFECT_CLASS_RESTORE]->load(pSlayer);
    m_pEffectLoaders[Effect::EFFECT_CLASS_KILL_AFTERMATH]->load(pSlayer);
    m_pEffectLoaders[Effect::EFFECT_CLASS_MUTE]->load(pSlayer);
    m_pEffectLoaders[Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR]->load(pSlayer);
}

void EffectLoaderManager::load(Vampire* pVampire)

{
    m_pEffectLoaders[Effect::EFFECT_CLASS_BLOOD_DRAIN]->load(pVampire);
    m_pEffectLoaders[Effect::EFFECT_CLASS_LIGHT]->load(pVampire);
    m_pEffectLoaders[Effect::EFFECT_CLASS_AFTERMATH]->load(pVampire);
    m_pEffectLoaders[Effect::EFFECT_CLASS_ENEMY_ERASE]->load(pVampire);
    m_pEffectLoaders[Effect::EFFECT_CLASS_RESTORE]->load(pVampire);
    m_pEffectLoaders[Effect::EFFECT_CLASS_KILL_AFTERMATH]->load(pVampire);
    m_pEffectLoaders[Effect::EFFECT_CLASS_MUTE]->load(pVampire);
    m_pEffectLoaders[Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR]->load(pVampire);
}

void EffectLoaderManager::load(Ousters* pOusters)

{
    m_pEffectLoaders[Effect::EFFECT_CLASS_BLOOD_DRAIN]->load(pOusters);
    m_pEffectLoaders[Effect::EFFECT_CLASS_AFTERMATH]->load(pOusters);
    m_pEffectLoaders[Effect::EFFECT_CLASS_KILL_AFTERMATH]->load(pOusters);
    m_pEffectLoaders[Effect::EFFECT_CLASS_MUTE]->load(pOusters);
    m_pEffectLoaders[Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR]->load(pOusters);
}

void EffectLoaderManager::load(Zone* pZone)

{
    m_pEffectLoaders[Effect::EFFECT_CLASS_GREEN_POISON]->load(pZone);
    m_pEffectLoaders[Effect::EFFECT_CLASS_YELLOW_POISON]->load(pZone);
    m_pEffectLoaders[Effect::EFFECT_CLASS_DARKNESS]->load(pZone);
    m_pEffectLoaders[Effect::EFFECT_CLASS_ACID_SWAMP]->load(pZone);
    m_pEffectLoaders[Effect::EFFECT_CLASS_CONTINUAL_BLOODY_WALL]->load(pZone);
    m_pEffectLoaders[Effect::EFFECT_CLASS_ICE_FIELD]->load(pZone);
    m_pEffectLoaders[Effect::EFFECT_CLASS_PROMINENCE]->load(pZone);
    m_pEffectLoaders[Effect::EFFECT_CLASS_ON_BRIDGE]->load(pZone);
}

string EffectLoaderManager::toString() const

{
    return "";
}
