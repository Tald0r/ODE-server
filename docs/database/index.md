# Guides

* [Database setup](overview.md) - MySQL 5.7 or 8 with the relaxed sql_mode, utf8mb4 end to end with a session pin, the two databases and how to load them, argon2id passwords and the one-time migrations.

# Reference

* [Configuration](configuration.md) - The three conf files, the keys that matter most, the two connection blocks and the Exchange's point tables, and what diagnostics may and may not contain.

# Rules

* [Seed-data rules](seed-data-rules.md) - Every initdb table is InnoDB in utf8mb4_unicode_ci, every seed Player row ships a current argon2id hash, and the seed guild unions are consistent.
