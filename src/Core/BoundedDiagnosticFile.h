#ifndef DARKEDEN_BOUNDED_DIAGNOSTIC_FILE_H
#define DARKEDEN_BOUNDED_DIAGNOSTIC_FILE_H

#include <cstddef>

#include <string_view>

namespace de {
namespace detail {
// Internal fallback sink, separate from rate limiting so signal handling can
// be verified independently of earlier failures in the same process.
void writeDiagnosticStderr(std::string_view record) noexcept;
} // namespace detail
struct DiagnosticFileLimits {
    std::size_t bytes = 10 * 1024 * 1024;
    unsigned backups = 2;
};

// Diagnostic storage only, never a transaction/audit acknowledgement. Writers
// cooperate through a stable sidecar lock, including across server processes.
bool appendBoundedDiagnosticFile(const char* path, std::string_view record, DiagnosticFileLimits limits = {}) noexcept;

// Preserves existing append-only retention for legacy audit/mixed logs. Uses
// the same interprocess lock and failure reporting, but never rotates files.
bool appendUnrotatedLogFile(const char* path, std::string_view record) noexcept;

// Reports the first failure and at most one summary per minute, without
// allocating, recursively logging, or including the failed record's contents.
void reportDiagnosticFileFailure(const char* operation, int error) noexcept;
} // namespace de

#endif
