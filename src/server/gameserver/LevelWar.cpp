#include "LevelWar.h"

void LevelWar::execute() {
    switch (m_State) {
    case READY:
        executeStart();
        break;
    case START:
        executeEnd();
        break;
    default:
        Assert(/*level war state is invalid*/ false);
    }
}

void LevelWar::executeStart() {
    if (m_pManager->hasWar())
        return;

    m_pManager->startWar();
    m_State = START;

    m_pManager->addSchedule(new Schedule(this, VSDateTime::currentDateTime().addSecs(3600)));

    m_pManager->makeGCWarList();
    m_pManager->broadcastGCWarList();
}

void LevelWar::executeEnd() {
    m_pManager->endWar();

    m_State = READY;

    // LevelWarManager is the one that does this
    // m_pManager->addSchedule( new Schedule( this, m_pManager->getNextLevelWarTime() ) );
}

string LevelWar::toString() const

{
    StringStream msg;

    msg << "LevelWar";

    return msg.toString();
}
