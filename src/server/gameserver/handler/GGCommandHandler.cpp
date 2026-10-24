//----------------------------------------------------------------------
//
// Filename    : GGCommandHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "GGCommand.h"

#ifdef __GAME_SERVER__

#include "VariableManager.h"
#include "gm/GMCommands.h"

#endif

//----------------------------------------------------------------------
//
// GGCommandHander::execute()
//
// Runs the chat-style command another game server relayed. The message
// carries no player and no creature, so the relay table - the commands that
// are correct without one - is what answers it.
//
//----------------------------------------------------------------------
void GGCommandHandler::execute(GGCommand* pPacket)

{
    __BEGIN_DEBUG_EX __BEGIN_DEBUG

#ifdef __GAME_SERVER__

            cout
        << "[" << pPacket->getHost().c_str() << ":" << pPacket->getPort() << "] " << pPacket->toString().c_str()
        << endl;

    filelog("ggCommand.txt", "[%s:%d] %s", pPacket->getHost().c_str(), pPacket->getPort(), pPacket->toString().c_str());

    // The '*' that introduces the command is the first character of the
    // relayed message.
    const string msg = pPacket->getCommand();
    de::gm::relayCommands().dispatch(de::gm::CommandContext{nullptr, nullptr, msg, 0, de::gm::Permission::God});

#endif

    __END_DEBUG
    __END_DEBUG_EX
}
