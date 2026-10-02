#ifndef DARKEDEN_SERVER_PROCESS_ENVIRONMENT_H
#define DARKEDEN_SERVER_PROCESS_ENVIRONMENT_H

#include <ctime>

#include <system_error>

namespace de {

// Raise the soft core-dump limit to the inherited hard limit, without changing
// that hard limit. A zero hard limit remains disabled. Failure is observable,
// but the entry points keep their existing best-effort startup policy.
[[nodiscard]] std::error_code raiseCoreDumpLimit() noexcept;

// Seed the process-wide C random generator at the existing startup sites.
// An explicit seed permits deterministic tests; the default remains epoch time.
void seedProcessRandomness(unsigned int seed = static_cast<unsigned int>(std::time(nullptr))) noexcept;

// Gameserver's system initialization: reseed rand() and ignore SIGPIPE,
// SIGALRM and SIGCHLD. Keep this before worker startup. Like the legacy sysinit,
// these settings last until process exit; this is not a scoped signal guard.
// SIGTERM/SIGINT, their masks and shutdown state belong to ServerProcessShutdown.
void initializeGameProcess(unsigned int seed = static_cast<unsigned int>(std::time(nullptr))) noexcept;

} // namespace de

#endif
