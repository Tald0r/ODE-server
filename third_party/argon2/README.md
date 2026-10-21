# argon2 (vendored)

- **Version:** 20190702 (the latest upstream release tag)
- **Upstream:** https://github.com/P-H-C/phc-winner-argon2
- **Files:** `LICENSE`, `include/argon2.h`, `src/{argon2,core,encoding,ref,thread}.c`,
  `src/{core,encoding,thread}.h`, `src/blake2/{blake2b.c,blake2.h,blake2-impl.h,blamka-round-ref.h}`
  — taken verbatim from the `20190702` tag, unmodified, in upstream's own
  layout. `SHA256SUMS` lists their digests (the release tarball is
  `daf972a89577f8772602bf2eb38b6a3dd3d922bf5724d45e7f9589b5e830442c`).
- **License:** CC0 1.0 or Apache 2.0, at your option (see `LICENSE`)
- **Not vendored:** `opt.c`/`blamka-round-opt.h` (the SSE2/AVX2 fill), the
  command-line tool, benchmarks and test drivers. `ref.c` is the portable
  fill, so the same code builds for every Zig target.

Backs `src/server/loginserver/PasswordHash.cpp`: argon2id hashing and
verification of account passwords. It exposes a C API only, so it fits the
toolchain rule in `docs/build/toolchain.md` (no C++-API dependencies under
`zig c++`).

## Conventions

- **Do not reformat these files.** `make fmt` only globs `src/` and `tests/`,
  so they are outside it by construction; keep them byte-identical to upstream
  so a version bump is a clean diff.
- To upgrade, replace the files from the new tag, refresh `SHA256SUMS` and the
  version above. `tests/password_hash_test.cpp` checks upstream's own
  known-answer vectors, so a broken or tampered copy fails there rather than
  silently accepting or rejecting logins.
