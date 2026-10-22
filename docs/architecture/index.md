# Overview

* [Server architecture](overview.md) - Three cooperating processes, each an executable of one main.cpp over a runtime object library, built on a macro-free kernel, pure domain formulas and a shared ServerCore.

# Rules

* [Assertions and NDEBUG](assertions-never-ndebug.md) - NDEBUG is never defined, so Assert and __BEGIN_TRY keep one meaning in every configuration; the Debug-only ownership checks ride their own macro.
* [Contexts, not globals](contexts-not-globals.md) - No g_p* extern survives; a manager is taken from the context of its layer, whose accessor asserts that its creator registered it.
* [Dead sources and source encoding](dead-sources-and-encoding.md) - Every .cpp is compiled by a named target and every header is included; sources are UTF-8 with English comments that describe behaviour.
* [Exceptions and outcomes](exceptions-and-outcomes.md) - Throw an Error, never a string literal; gameplay results are Outcome values and exceptions are for programming and configuration errors.
* [Module layering](module-layering.md) - de-kernel includes only kernel files and no server macro, de-core includes only itself, ServerCore pulls in no MySQL, Lua or socket transport, and the include checker names the offending edge.
* [Preprocessor macros](preprocessor-macros.md) - The server-type macros and __COMBAT__ are set per target, a macro nothing defines is never reintroduced, and the mofus guards are not the switches they look like.
* [Repository seams](repository-seams.md) - Every SQL statement lives behind a *Repository interface with a MySQL implementation under a repository/ directory, reached through a default accessor, and the header is the authority on its tables.
