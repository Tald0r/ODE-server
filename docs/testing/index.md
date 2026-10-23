# Guides

* [Testing overview](overview.md) - make test or make dev-test before every push: the wire tests over the goldens, one runtime test binary per server over the production objects, the isolated decision tests, the script audits and the MySQL tier.

# Reference

* [Rules and the tests that own them](rules-and-owners.md) - Every architectural rule in one table: the rule, the test, ratchet or gate that owns it, what fails when it is broken, and the concept that explains it.
* [What the runtime and library tests cover](test-coverage.md) - Per process and per library, what the runtime tests and the isolated startup, socket and worker tests exercise beyond the rule each owns: the inputs they supply, the failures they inject and the policies they pin.

# Rules

* [Fix log and task status](fix-log-and-status.md) - A bug found while restructuring is recorded in the fix log with its status, and a restructuring task is done only once its owner exists, both updated in the same commit as the work.
* [Ratchets](ratchets.md) - Ratchet numbers only go down and fail in both directions; a drop is tightened in ratchets.sh and the plan's table in the same commit, and every baseline comes from running its command.
