#ifndef DARKEDEN_LOGIN_INCOMING_REQUEST_H
#define DARKEDEN_LOGIN_INCOMING_REQUEST_H

#include <functional>
#include <string>

#include "Types.h"

class GameServerInfoManager;
class LGIncomingConnection;
class LoginAccountRepository;
class LoginCharacterRepository;
class LoginPlayer;
class Properties;
struct SelectPCRequest;
struct SelectedCharacter;

namespace de {

struct LoginIncomingRequestServices {
    const GameServerInfoManager& servers;
    const Properties& config;
    LoginAccountRepository& accounts;
    LoginCharacterRepository& characters;
    std::function<void(const std::string& host, uint port, const LGIncomingConnection& packet)> send;
    std::function<void(const std::string& host, uint port)> reportDestination;
};

// Prepare the accepted selection and destination before publishing a reply-ready
// phase/address. The excel96 deployment uses the catalogue UDP port; all others
// use GameServerUDPPort. Destination diagnostics are best effort.
// Invalid endpoints/ports raise ConnectException before publication; missing
// configuration/catalogue values retain NoSuchElementException.
// Sender failure restores the previous phase/address without allocation and
// propagates unchanged. After sender return, write account location, then the
// character group, from owned snapshots. Persistence failure keeps the published
// pending request and propagates; a sent datagram and earlier writes cannot be
// rolled back. Account ownership stays with the caller on every outcome.
// Callers serialize this flow with player access/replies. Callbacks borrow their
// arguments only for their call and must not retire or mutate this player.
void requestLoginIncomingConnection(LoginPlayer& player, const SelectPCRequest& request,
                                    const SelectedCharacter& selected, const LoginIncomingRequestServices& services);

} // namespace de

#endif
