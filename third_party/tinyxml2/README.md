# tinyxml2 (vendored)

- **Version:** 10.0.0
- **Upstream:** https://github.com/leethomason/tinyxml2
- **Files:** `tinyxml2.h`, `tinyxml2.cpp`, `LICENSE.txt` — taken verbatim from
  the `10.0.0` tag, unmodified.
- **License:** zlib (see `LICENSE.txt`)

Replaces xerces-c as the backend for `src/Core/SXml.cpp`. See
`docs/build/toolchain.md` for why, and for what changed in the parsed output.

## Conventions

- **Do not reformat these files.** `make fmt` only globs `src/` and `tests/`,
  so they are outside it by construction; keep them byte-identical to upstream
  so a version bump is a clean diff.
- To upgrade, replace all three files from the new tag and update the version
  above. `tests/xml_parse_test.cpp` pins the parsed output of the real data
  files, so a behavioural change in the parser shows up as a golden diff
  rather than as a silent content change.
