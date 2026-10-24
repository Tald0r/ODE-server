
#include "EffectKeepSweeper.h"

#include "GCRemoveEffect.h"
#include "Item.h"

EffectKeepSweeper::EffectKeepSweeper(Item* pItem) {
    setTarget(pItem);
    m_Part = 0;
}

void EffectKeepSweeper::unaffect() {
    Item* pItem = dynamic_cast<Item*>(m_pTarget);
    Assert(pItem != NULL);

    pItem->removeFlag(getEffectClass());
}

string EffectKeepSweeper::toString() const {
    return "EffectKeepSweeper";
}
