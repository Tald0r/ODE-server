#include "ServerProcessEnvironment.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>

#include <sys/resource.h>

namespace de {

std::error_code raiseCoreDumpLimit() noexcept {
    struct rlimit limit {};
    if (getrlimit(RLIMIT_CORE, &limit) != 0)
        return {errno, std::generic_category()};
    limit.rlim_cur = limit.rlim_max;
    if (setrlimit(RLIMIT_CORE, &limit) != 0)
        return {errno, std::generic_category()};
    return {};
}

void seedProcessRandomness(unsigned int seed) noexcept {
    std::srand(seed);
}

void initializeGameProcess(unsigned int seed) noexcept {
    seedProcessRandomness(seed);
    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGALRM, SIG_IGN);
    std::signal(SIGCHLD, SIG_IGN);
}

} // namespace de
