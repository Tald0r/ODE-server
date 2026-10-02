#ifndef DARKEDEN_LOGIN_CHARACTER_LIST_H
#define DARKEDEN_LOGIN_CHARACTER_LIST_H

#include <memory>
#include <string>

#include "Types.h"

class LCPCList;
class LoginCharacterRepository;

namespace de {

// Build a complete owned reply from the account's character rows. Callers retain
// the reply through synchronous sending; no session or process context is read.
// Missing race rows and database failures retain the login disconnect policy.
[[nodiscard]] std::unique_ptr<LCPCList> makeLoginCharacterList(WorldID_t worldID, const std::string& account,
                                                               LoginCharacterRepository& repository);

} // namespace de

#endif
