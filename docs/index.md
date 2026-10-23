---
okf_version: "0.2"
---

# Guides

* [The knowledge bundle](knowledge-bundle.md) - How docs/ is organised as an Open Knowledge Format v0.2 bundle, which frontmatter keys and types it uses, how to add or move a document, and the ctest that keeps it whole.

# Folders

* [Architecture](architecture/) - The three processes, the de-kernel / de-core / ServerCore / runtime layering, contexts instead of globals, repository seams, and the source rules the ratchets hold.
* [Build](build/) - The make targets, the container build, the native macOS build, formatting, CI, the dependencies and the toolchain notes.
* [Database](database/) - MySQL setup, the configuration files, and the rules every seed row and table obeys.
* [Gameserver](game/) - The gameplay process: its layout, its connection and player handoffs, and the world and server catalogues it shares with the other two.
* [Loginserver](login/) - Authentication, account ownership, kicks, character flows and the catalogues the login server selects from, each with the test that owns it.
* [Operations](operations/) - Running the three servers, production logging and the WebSocket gateway.
* [Protocol](protocol/) - Packets by link, the wire contract and its goldens, factory registrations, the client link, the frame gate, fuzzing and the formulas shared with the client.
* [Reviews](reviews/) - The restructuring plan with its task statuses and ratchet table, and the fix log.
* [Sharedserver](shared/) - The guild and catalogue process: construction, catalogue loads and guild loading, each with the test that owns it.
* [Startup](startup/) - How a main() is composed from the shared startup, lifecycle, listener, socket and worker libraries, and the rules their tests own.
* [Testing](testing/) - What the suite runs, every rule with the test that owns it, the shrink-only ratchets, and where a bug or a task status is recorded.
* [Threading](threading/) - The threads in each process, the zone-group mutation rule, critical sections, cross-thread communication and the violations that were closed.

# Changes

* [Update log](log.md) - What changed in the bundle, newest first, including where each moved document went.
