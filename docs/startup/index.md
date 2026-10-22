# Overview

* [Startup composition](overview.md) - How each main() is composed from the shared startup, port, parser, application, lifecycle, shutdown, fatal-handler, environment, listener, connection and worker libraries, and which test owns each.

# Rules

* [Lifecycle rules](lifecycle-rules.md) - Arguments, configuration and ports are validated before anything is published; shutdown is requested before cleanup, every cleanup step is attempted, and a worker failure survives into the exit status.
* [Sockets and connections](sockets-and-connections.md) - Listener startup retries only bind failures, every socket keeps an owner through construction, adoption and replacement, and connection keys never leave the process.
* [Workers and synchronization](workers-and-synchronization.md) - Mutexes use native ownership, every worker is a managed jthread whose entry follows publication, and pools retain their members through a failed registration.
