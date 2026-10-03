#include "GLKickVerify.h"

#ifdef __LOGIN_SERVER__
#include "LoginContext.h"
#include "LoginKickVerification.h"
#include "LoginPlayer.h"
#endif

void GLKickVerifyHandler::execute(GLKickVerify* packet) {
#ifdef __LOGIN_SERVER__
    try {
        (void)de::verifyLoginKick(de::loginContext().loginPlayers(), *packet,
                                  [](LoginPlayer& player) { player.sendLCLoginOK(); });
    } catch (const Throwable&) {
        // Keep the UDP handler's existing policy for failed completion. The
        // verification flow releases its manager lock for every exception type.
    }
#endif
}
