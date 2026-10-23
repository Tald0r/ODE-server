#include "MiniGameQuestInfo.h"

MiniGameQuestStatus* MiniGameQuestInfo::makeQuestStatus(PlayerCreature* pPC) const {
    return new MiniGameQuestStatus(getQuestID(), VSDateTime::currentDateTime().addSecs(getTimeLimit()), m_GameType);
}
