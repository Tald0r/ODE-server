//////////////////////////////////////////////////////////////////////////////
// Filename    : Regeneration.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "RegenerationSkill.h"

#include "EffectRegeneration.h"
#include "GCSkillFailed1.h"
#include "GCSkillFailed2.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK5.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GCStatusCurrentHP.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////
//
// RegenerationSkill::execute()
//
//////////////////////////////////////////////////////////////////////
void RegenerationSkill::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{}

RegenerationSkill g_RegenerationSkill;
