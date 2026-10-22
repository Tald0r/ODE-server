---
type: Rule
title: "CI"
description: "The C++20 and format workflows run on master pushes and manual dispatch only, so every branch is verified locally with the full suite before it is pushed."
tags: [build, process]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# CI

**Feature branches are verified locally.** The C++20 workflow (`.github/workflows/cpp20.yml`)
runs the suite and the production builds only on master pushes and merges, or on a manual
`workflow_dispatch`, to conserve Actions minutes; `format-check.yml` follows the same triggers
([formatting](formatting.md)). So `make dev-test` or `make test` runs before every push
([testing overview](../testing/overview.md)), and the generated-file pins and ratchets it carries
are what CI would have refused ([ratchets](../testing/ratchets.md)).

There is no CI fuzz job; the fuzz targets' replay tests run in the ordinary suite and the
libFuzzer runs are manual ([fuzzing](../protocol/fuzzing.md)). The MySQL-backed repository tier,
`make integration-test`, needs docker and the `darkeden-dev` image and is run by hand as well.
