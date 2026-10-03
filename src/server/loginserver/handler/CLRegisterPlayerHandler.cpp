#include "CLRegisterPlayer.h"

#ifdef __LOGIN_SERVER__
#include "Assert1.h"
#include "LoginPlayer.h"
#include "LoginRegistration.h"
#include "repository/LoginAccountRepository.h"
#endif

void CLRegisterPlayerHandler::execute(CLRegisterPlayer* packet, Player* player) {
    __BEGIN_TRY __BEGIN_DEBUG_EX
#ifdef __LOGIN_SERVER__
        Assert(packet != nullptr);
    Assert(player != nullptr);
    __BEGIN_DEBUG
    auto* loginPlayer = dynamic_cast<LoginPlayer*>(player);
    (void)de::registerLoginPlayer(*loginPlayer, *packet, defaultLoginAccountRepository(),
                                  de::defaultLoginRegistrationActions());
    __END_DEBUG
#endif
    __END_DEBUG_EX __END_CATCH
}
