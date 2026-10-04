#include "CLLogin.h"

#ifdef __LOGIN_SERVER__
#include "Assert1.h"
#include "LoginAuthentication.h"
#include "LoginFlow.h"
#include "LoginPlayer.h"
#include "repository/LoginAccountRepository.h"
#endif

void CLLoginHandler::execute(CLLogin* packet, Player* player) {
    __BEGIN_TRY __BEGIN_DEBUG_EX
#ifdef __LOGIN_SERVER__
        Assert(packet != nullptr);
    Assert(player != nullptr);
    de::loginPlayer(*dynamic_cast<LoginPlayer*>(player), *packet, defaultLoginAccountRepository(),
                    de::defaultLoginFlowActions());
#endif
    __END_DEBUG_EX __END_CATCH
}

bool CLLoginHandler::checkNetMarbleClient(CLLogin* pPacket, Player* pPlayer)

{
#ifdef __LOGIN_SERVER__

    return de::checkNetMarbleLoginGate(*dynamic_cast<LoginPlayer*>(pPlayer), *pPacket, defaultLoginAccountRepository(),
                                       de::defaultLoginAuthenticationActions());

#endif

    return true;
}


bool CLLoginHandler::checkFreePass(CLLogin* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    return de::verifyNetMarblePassword(*pPacket, defaultLoginAccountRepository(),
                                       de::defaultLoginAuthenticationActions());

#endif

    __END_DEBUG_EX __END_CATCH

        return false;
}

bool CLLoginHandler::checkWebLogin(CLLogin* pPacket, Player* pPlayer) {
    __BEGIN_TRY

#ifdef __LOGIN_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    return de::authorizeWebLogin(*dynamic_cast<LoginPlayer*>(pPlayer), *pPacket, defaultLoginAccountRepository(),
                                 de::defaultLoginAuthenticationActions());

#endif

    __END_CATCH

    return true;
}
