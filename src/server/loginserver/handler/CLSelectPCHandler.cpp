//////////////////////////////////////////////////////////////////////////////
// Filename    : CLSelectPCHandler.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLSelectPC.h"

#ifdef __LOGIN_SERVER__
#include <utility>

#include "Assert1.h"
#include "CharacterSelection.h"
#include "DatabaseError.h"
#include "GameServerManager.h"
#include "KernelContext.h"
#include "LCSelectPCError.h"
#include "LGIncomingConnection.h"
#include "LoginCharacterTopology.h"
#include "LoginContext.h"
#include "LoginIncomingRequest.h"
#include "LoginPlayer.h"
#include "ServerContext.h"
#include "repository/LoginAccountRepository.h"
#include "repository/LoginCharacterRepository.h"

#endif

//////////////////////////////////////////////////////////////////////////////
// CLSelectPCHandler::execute()
//
// Loads the PC named by the packet, checks that the account may bring it
// into the game, and tells the game server that runs its zone to expect
// the incoming connection.
//////////////////////////////////////////////////////////////////////////////
void CLSelectPCHandler::execute(CLSelectPC* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    Properties& config = de::kernelContext().config();

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

    WorldID_t WorldID = pLoginPlayer->getWorldID();

    SelectPCRequest request;
    request.worldID = WorldID;
    request.serverGroupID = pLoginPlayer->getServerGroupID();
    request.playerID = pLoginPlayer->getID();
    request.pcName = pPacket->getPCName();
    request.pcType = pPacket->getPCType();
    request.inCharacterManagement = (pLoginPlayer->getPlayerStatus() == LPS_PC_MANAGEMENT);

    // The external billing gate that used to answer SELECT_PC_CANNOT_PLAY and
    // SELECT_PC_NOT_BILLING_CHECK is switched off, so nothing produces those
    // two codes and no account state is checked here.

    LoginCharacterTopology topology(de::serverContext().serverInfos(), de::loginContext().zoneInfos(),
                                    de::loginContext().zoneGroupInfos());

    try {
        Outcome<SelectedCharacter, SelectPCRejection> outcome =
            decideSelectPC(request, defaultLoginCharacterRepository(), topology);

        if (outcome.isRejected()) {
            LCSelectPCError lcSelectPCError;

            switch (outcome.rejection()) {
            case SelectPCRejection::DidNotAgree:
                lcSelectPCError.setCode(SELECT_PC_DIDNOT_AGREE);
                break;

            case SelectPCRejection::FreePlayLimit:
            case SelectPCRejection::NonPKServerLimit:
                lcSelectPCError.setCode(SELECT_PC_CANNOT_PLAY_BY_ATTR);
                break;

            // The three below cannot be produced by a client that speaks
            // the protocol, so the connection is dropped rather than
            // answered with an error packet.
            case SelectPCRejection::InvalidStatus:
                throw DisconnectException("invalid player status");

            case SelectPCRejection::NoSuchCharacter:
                throw InvalidProtocolException("no such PC exist.");

            case SelectPCRejection::NoSlot:
                throw InvalidProtocolException("no slot exist.");
            }

            pLoginPlayer->sendPacket(&lcSelectPCError);
            return;
        }

        const SelectedCharacter selected = std::move(outcome).events();

        GameServerManager& gameServers = de::loginContext().gameServers();
        de::requestLoginIncomingConnection(
            *pLoginPlayer, request, selected,
            {de::serverContext().serverInfos(), config, defaultLoginAccountRepository(),
             defaultLoginCharacterRepository(),
             [&gameServers](const std::string& host, uint port, const LGIncomingConnection& packet) {
                 gameServers.sendPacket(host, port, &packet);
             },
             [](const std::string& host, uint port) {
                 cout << "gameserver ip: " << host << ", port: " << port << endl;
             }});
    } catch (const DatabaseError& error) {
        // A SQL failure arrives as END_DB's DatabaseError carrying the line
        // it wrote to DBError.log; the reason travels with the disconnect.
        throw DisconnectException("CLSelectPCHandler : " + error.message());
    } catch (NoSuchElementException& nsee) {
        StringStream msg;

        msg << "Critical Error : data intergrity broken at ZoneInfo - ZoneGroupInfo - GameServerInfo : "
            << nsee.toString();

        throw Error(msg.toString());
    }

#endif

    __END_DEBUG_EX __END_CATCH
}
