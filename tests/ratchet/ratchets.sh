#!/usr/bin/env bash
# Shrink-only ratchets for docs/RESTRUCTURING.md, plus generated-file pins.
#
# Each ratchet compares a measured count against the baseline recorded here.
#   measured > baseline  -> FAIL: new debt of a kind being paid down.
#   measured < baseline  -> FAIL: progress! tighten the baseline below (and
#                           the table in docs/RESTRUCTURING.md) in the same
#                           commit, so the number stays honest.
# Never raise a baseline.
#
# Run from the repository root (ctest does this via WORKING_DIRECTORY).
set -uo pipefail

fail=0

check_ratchet() {
    local id="$1" desc="$2" baseline="$3" measured="$4"
    # A non-numeric measurement (e.g. `wc -l < missing-file` yields "")
    # would make both [ -gt ] and [ -lt ] fail and fall through to [OK] —
    # a ratchet that silently passes when its subject disappears.
    if ! [[ "$measured" =~ ^[0-9]+$ ]]; then
        echo "[FAIL] $id $desc: measurement produced '$measured' (not a number — file missing or renamed?)"
        fail=1
    elif [ "$measured" -gt "$baseline" ]; then
        echo "[FAIL] $id $desc: measured $measured > baseline $baseline (new debt — remove it instead)"
        fail=1
    elif [ "$measured" -lt "$baseline" ]; then
        echo "[FAIL] $id $desc: measured $measured < baseline $baseline (progress — tighten the baseline in tests/ratchet/ratchets.sh and docs/RESTRUCTURING.md in this commit)"
        fail=1
    else
        echo "[OK]   $id $desc: $measured"
    fi
}

# --- R1: g_p* global-singleton extern declarations -------------------------
R1=$(grep -rE '^extern .*\* g_p' src --include='*.h' --include='*.cpp' | wc -l)
check_ratchet R1 "global singleton externs" 0 "$R1"

# --- R2: files with inline SQL in the gameserver root ----------------------
R2=$(grep -lE 'executeQuery' src/server/gameserver/*.cpp src/server/gameserver/*.h 2>/dev/null | wc -l)
check_ratchet R2 "gameserver-root files with inline SQL" 0 "$R2"

# --- R3: files with inline SQL outside database/ and any repository/ -------
# Repository impls are the sanctioned quarantine for SQL — R3 measures SQL
# loose in game logic, so every repository/ directory under src/ is
# excluded whichever binary owns it (gameserver/, loginserver/,
# sharedserver/ and ServerCore's src/server/repository/), while the
# loginserver/sharedserver/ServerCore game logic itself still counts.
# gameserver/repository/ joined the exclusion 2026-09-01 (317→314: two
# files cleansed, one pilot impl no longer counted — an extraction that
# creates more impl files than it cleanses would otherwise RAISE a
# shrink-only ratchet); the other directories were admitted 2026-09-07
# before they existed, so the baseline did not move. (Trailing slash:
# only a directory is excluded.) The count is 0: every statement in the
# tree lives in a repository impl or in database/, so a new executeQuery in
# game logic fails here.
R3=$(grep -rlE 'executeQuery' src --include='*.cpp' | grep -v 'server/database' |
    grep -v '/repository/' | wc -l)
check_ratchet R3 "files with inline SQL outside database/, repository/" 0 "$R3"

# --- R4: packet headers still carrying execute() on the packet -------------
R4=$(grep -rlE 'void execute\(Player' src/Core --include='*.h' | wc -l)
check_ratchet R4 "packet headers with execute()" 0 "$R4"

# --- R5: __BEGIN_TRY control-flow macro sites in gameserver ----------------
# handler/, packetfill/ and gm/ are excluded. handler/ and packetfill/ hold
# sources that moved there from src/Core in task 2.4, where this metric never
# counted them; gm/ holds the GM command bodies that moved out of
# handler/CGSayHandler.cpp in task 4.1, and the 33 macro pairs in them are
# those same handler bodies at a new address, not new debt. Including either
# set would jump the baseline without anything having been written. Fold them
# in (with a re-baseline note) when they become de-core extraction targets in
# 3.x.
R5=$(grep -rE '__BEGIN_TRY' src/server/gameserver --include='*.cpp' | grep -vE 'gameserver/(gm|handler|packetfill)/' | wc -l)
check_ratchet R5 "__BEGIN_TRY sites in gameserver" 5123 "$R5"

# --- R6: god-file line counts (task 3.3 files only, so far) -----------------
# Formula extraction to de-core (src/domain) shrinks these; each delegation
# that moves math out must tighten the number here. The doc's other god
# files join when their own extractions start. R6a-c baselines measured
# 2026-08-31 post-3.3-extraction; R6d added 2026-09-01 with the
# SkillFormula.cpp computeOutput extraction (the doc's 08-29 numbers
# predate the clang-format-18 pass and are superseded).
# R6a shrinks as SkillUtil.cpp's concerns leave it: the damage formulas and the
# code that applies them live in SkillDamage.cpp, everything a kill earns a
# creature -- experience, alignment, fame -- in SkillExperience.cpp, and the
# distance, line-of-sight, splash and facing work in SkillGeometry.cpp. What is
# left is the mana and HP costs, the slot run-time and zone-level gates, the
# skill-failure packets and the elemental lookups. Under the 2,000-line phase
# exit criterion, so the ratchet is a pin rather than a god-file baseline now.
R6a=$(wc -l < src/server/gameserver/skill/SkillUtil.cpp 2>/dev/null || echo missing)
check_ratchet R6a "SkillUtil.cpp lines" 678 "$R6a"
# R6b shrinks as InitAllStat.cpp's per-race stat code leaves it: the Slayer,
# Vampire and Ousters members -- the castle skills, the all-stat
# recalculation and the item, option and blood bible contributions to it --
# live in SlayerStat.cpp / VampireStat.cpp / OustersStat.cpp. What is left is
# the two bodies no race unit owns, PlayerCreature::applyBloodBibleSign and
# Monster::initAllStat. Under the 2,000-line phase exit criterion, so the
# ratchet is a pin rather than a god-file baseline now.
R6b=$(wc -l < src/server/gameserver/InitAllStat.cpp 2>/dev/null || echo missing)
check_ratchet R6b "InitAllStat.cpp lines" 230 "$R6b"
R6c=$(wc -l < src/server/gameserver/skill/HitRoll.cpp 2>/dev/null || echo missing)
check_ratchet R6c "HitRoll.cpp lines" 642 "$R6c"
R6d=$(wc -l < src/server/gameserver/skill/SkillFormula.cpp 2>/dev/null || echo missing)
check_ratchet R6d "SkillFormula.cpp lines" 818 "$R6d"
# R6e added with the 4.1 GM-command extraction: the 33 command bodies and
# the branch ladder left CGSayHandler.cpp for gm/, leaving the packet
# handler itself.
R6e=$(wc -l < src/server/gameserver/handler/CGSayHandler.cpp 2>/dev/null || echo missing)
check_ratchet R6e "CGSayHandler.cpp lines" 111 "$R6e"
# R6f: the *command console, whose sub-command bodies are one function per
# name in ConsoleCommands.cpp beside the console that dispatches them.
R6f=$(wc -l < src/server/gameserver/gm/ConsoleCommands.cpp 2>/dev/null || echo missing)
check_ratchet R6f "ConsoleCommands.cpp lines" 1574 "$R6f"

# R6g: Zone.cpp with broadcast, scan/visibility, movement, the loaders,
# spawn/despawn and the item tables split out to ZoneBroadcast.cpp /
# ZoneScan.cpp / ZoneMove.cpp / ZoneLoad.cpp / ZoneSpawn.cpp / ZoneItem.cpp.
# What is left is the zone's own state: tiles, effects, creature lookup, the
# NPC registry and the heartbeat. Under the 2,000-line phase exit criterion.
R6g=$(wc -l < src/server/gameserver/Zone.cpp 2>/dev/null || echo missing)
check_ratchet R6g "Zone.cpp lines" 1265 "$R6g"

# R6h-j: the three race classes. Persistence, gold, item-shape, inventory,
# free-play, skill-slot-table, exps, initial-rank, silver-damage and
# connect-time item-loading bodies now live once on PlayerCreature; what is
# left in each file is its own wear code, its load/save record code, and the
# two skill-slot bodies whose content really is per-race (addSkill). The wear
# bodies are still identical in Vampire and Ousters, but only after
# substituting the class-scoped WearPart enum, whose values are the slot ids
# the client sends, so reconciling them is a protocol change rather than a
# refactor.
R6h=$(wc -l < src/server/gameserver/Slayer.cpp 2>/dev/null || echo missing)
check_ratchet R6h "Slayer.cpp lines" 2953 "$R6h"
R6i=$(wc -l < src/server/gameserver/Vampire.cpp 2>/dev/null || echo missing)
check_ratchet R6i "Vampire.cpp lines" 1919 "$R6i"
R6j=$(wc -l < src/server/gameserver/Ousters.cpp 2>/dev/null || echo missing)
check_ratchet R6j "Ousters.cpp lines" 1848 "$R6j"

# --- R7: pre-C++17 dynamic exception specifications ------------------------
# The migration also normalized real `throw(expr)` expressions to `throw expr`
# so this deliberately simple textual ban is unambiguous and catches typed,
# empty, and commented-out specifications alike.
R7=$(grep -rlE 'throw[[:space:]]*\(' src --include='*.h' --include='*.cpp' | wc -l)
check_ratchet R7 "files with parenthesized throw syntax" 0 "$R7"

# --- R8: __PRETTY_FUNCTION__ in code ---------------------------------------
# Call-site diagnostics take the enclosing function from a defaulted
# std::source_location parameter, so no source line needs the macro. The rule
# is line-based and deliberately simple: a line whose first non-blank
# characters are `//` is a comment and does not count (the comments that
# explain the source_location/__PRETTY_FUNCTION__ equivalence are allowed to
# say the name), every other matching line does. A block comment or a `//`
# trailing real code is not recognised as a comment, so a new use cannot hide
# behind one.
R8=$(grep -rh '__PRETTY_FUNCTION__' src --include='*.h' --include='*.cpp' | grep -vcE '^[[:space:]]*//')
check_ratchet R8 "non-comment __PRETTY_FUNCTION__ lines" 0 "$R8"

# --- R9: hand-written length-prefixed string reads -------------------------
# A string field is a length prefix then that many bytes, and de::wire
# helpers (src/Core/WireString.h) carry it with the bounds stated once --
# readString/writeString over a BYTE prefix, readString16/writeString16 over
# a WORD one. What this counts is the legacy shape: a length read into a
# local and handed straight to read(string&, uint), with the bounds spelled
# out around it, into a member or into a local. None is left; the count is of
# call lines, using R8's comment rule so WireString.h's own example of the
# shape it replaces does not count itself.
R9=$(grep -rhE 'iStream\.read\([A-Za-z_][A-Za-z0-9_]*, sz[A-Za-z0-9_]*\);' src/Core \
    --include='*.h' --include='*.cpp' | grep -vcE '^[[:space:]]*//')
check_ratchet R9 "hand-written length-prefixed string reads" 0 "$R9"

# --- R10: what a failed statement throws, and who catches it ---------------
# A throw of a c_str() taken from a local string hands the handler storage
# that dies with the clause it came from. END_DB and END_DB_EX answer a
# failed statement with a DatabaseError (src/server/database/DatabaseError.h)
# that owns the line they wrote to DBError.log, so no statement in the tree
# has that shape.
R10a=$(grep -rnE 'throw [A-Za-z_]+\.c_str\(\)' src | wc -l)
check_ratchet R10a "throws of a pointer into a local string" 0 "$R10a"

# The receiving end. All 34 handlers name DatabaseError, so one left here
# would be dead: no statement throws a const char*, and R11 holds the rest of
# the tree at zero bare literal throws. Textual, so a commented-out clause
# counts too.
R10b=$(grep -rn 'catch (const char\*' src | wc -l)
check_ratchet R10b "catch (const char*) handlers left in src" 0 "$R10b"


# --- R11: bare string-literal throws ---------------------------------------
# A `throw "text"` puts a const char* on the stack. Nothing in the tree
# catches that type (R10b), and neither __END_CATCH nor the swallowing
# __END_CATCH_NO_RETHROW matches it, so it walks past every handler the code
# around it wrote and lands in a catch (...) backstop -- or, out of a
# destructor, in std::terminate. `throw Error("text")` reaches the handler
# that was written for it. None is left, so a new one fails here. Line-based
# with R8's comment rule, so a commented-out throw does not count.
R11=$(grep -rh 'throw "' src --include='*.h' --include='*.cpp' | grep -vcE '^[[:space:]]*//')
check_ratchet R11 "bare string-literal throws" 0 "$R11"

# --- R12: non-ASCII text in throw messages ---------------------------------
# What a throw carries is a diagnostic, read in a log or on a console, and
# the tree's code language is English. The legacy messages were legacy-code-
# page bytes: some survived the migration as readable Korean, most as
# mojibake, a few as U+FFFD runs with the original text gone. All are
# English now. Line-based like R8/R11, with the same comment rule. The
# class is spelled as the bytes it excludes -- everything from \x01 to
# \x7f, so what is left is a byte with the high bit set, which is what a
# UTF-8 lead or continuation byte is -- rather than as [^[:print:]], which
# would also match the CR of a CRLF working tree and count every ASCII
# message whose literal runs past the end of its line. LC_ALL=C keeps the
# range byte-wise, and the bracket form keeps this portable where grep -P
# is not. The pattern wants `throw`, a type, `(` and the opening quote on
# one line: a literal that starts on a continuation line is not counted,
# and two are in the tree, both English.
R12=$(LC_ALL=C grep -rhE $'throw[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\\([[:space:]]*"[^"]*[^\x01-\x7f]' \
    src --include='*.h' --include='*.cpp' | grep -vcE '^[[:space:]]*//')
check_ratchet R12 "throw messages carrying non-ASCII text" 0 "$R12"

# --- R13: duplicated include guards ----------------------------------------
# Two headers sharing a guard name means whichever one a translation unit
# reaches first silently swallows the other: the second #include expands to
# nothing and its declarations are simply absent, which surfaces as an
# unrelated "undeclared identifier" far from the cause. Eighteen headers were
# renamed to make the names unique tree-wide, including per-server twins
# (loginserver/sharedserver copies of GameServerInfo.h and friends) that
# cannot meet in one binary today -- uniqueness across the whole tree is the
# invariant worth holding, because which headers share a binary changes.
#
# Measured on each header's FIRST #ifndef, which is its guard, rather than on
# every `^#ifndef` line: the tree also tests ordinary macros that way
# (Core/Types.h tests __XMAS_EVENT_CODE__ before defining it), so a
# line-based count could never reach zero. The awk resets `seen` at FNR==1 so `find -exec ... +`
# batching stays per-file, and strips the CR of the CRLF working tree before
# comparing. The list is materialised first so an empty one (a broken find,
# a moved src/) fails loudly instead of passing as zero duplicates.
guards=$(mktemp)
find src -name '*.h' -exec awk '
    FNR == 1 { seen = 0 }
    !seen && /^[[:space:]]*#[[:space:]]*ifndef[[:space:]]/ {
        line = $0
        sub(/\r$/, "", line)
        sub(/^[[:space:]]*#[[:space:]]*ifndef[[:space:]]+/, "", line)
        sub(/[[:space:]].*$/, "", line)
        print line
        seen = 1
    }' {} + | sort > "$guards"
if [ "$(wc -l < "$guards")" -lt 1000 ]; then
    echo "[FAIL] R13 duplicated include guards: only $(wc -l < "$guards") guards found under src/ (find or awk broken?)"
    fail=1
else
    R13=$(uniq -d < "$guards" | wc -l)
    check_ratchet R13 "duplicated include guard names" 0 "$R13"
    if [ "$R13" -gt 0 ]; then
        uniq -d < "$guards" | sed 's/^/         /'
    fi
fi
rm -f "$guards"
# --- R14: mentions of macros nothing defines -------------------------------
# A macro no build defines -- not cmake/, a CMakeLists.txt, a Makefile, a
# Dockerfile or a workflow -- makes the block behind it dead text: it never
# reached the compiler, so the #else/#ifndef branch beside it was the only
# code the servers ran, while the block kept reading as live code and
# drifting out of sync with the headers it names. A translation-unit-local
# #define arms one again, and one did: it gave that TU a
# SystemAvailabilitiesManager with an extra member and a layout no other TU
# shared.
#
# The region builds went first. __THAILAND_SERVER__ and __CHINA_SERVER__
# selected a Thailand or a China build; counted with them are the
# misspellings __CHAINA_SERVER__ and __THIALAND_SERVER__, which appeared
# inside the same conditions, and __INTERNATIONAL_SERVER__, a dead #elif in
# the same Encrypter.h chain. __NETMARBLE_SERVER__ was a portal build (a
# terms-of-use byte on the end of LCPCList, DELETE instead of INACTIVE in the
# character purge) and __TEST_SERVER__ a test build (fame*10 in the Blood
# Bible ladders, an auth timer on connect, a level-150 class-exp gift).
# Three of the __NETMARBLE_SERVER__ blocks called
# LoginPlayer::setAgree/isAgree, which no header declares, so they could not
# have compiled had it been defined.
#
# The feature macros followed: __OLD_GUILD_WAR__ (guild union and tax
# handlers that only answered "not supported yet", and a one-attacker war
# schedule where the live read takes five), __CONNECT_BILLING_SYSTEM__ and
# __COUT_BILLING_SYSTEM__ (the external billing link and its console trace),
# __PAY_SYSTEM_ZONE__ / __PAY_SYSTEM_LOGIN__ / __PAY_SYSTEM_FREE_LIMIT__
# (the paid-zone, paid-login and free-play-limit gates -- with none of them
# defined GamePlayer::isPayPlaying() answers true for every player and each
# gate passes), __UNDERWORLD__ (an underworld monster flag, its protection
# rule and its prize), __ACTIVE_QUEST__ (an NPC quest board),
# __ACTIVE_SERVICE_DEADLINE__ (a date past which the session's encrypt code
# was to be corrupted) and __WINDOWS__ (the Windows arm of the platform
# switch; __LINUX__ comes from the top-level CMakeLists.txt and __APPLE__
# from the compiler, so that arm was the only dead one).
#
# Deliberately not counted: the instrumentation toggles a developer switches
# on by hand (__PROFILE_*, __FULL_PROFILE__, __DEBUG_OUTPUT__,
# __OUTPUT_INIT__). Comments count -- a comment describing one of these
# branches describes code that is not there, so it states what the code does
# instead.
R14=$(LC_ALL=C grep -rhE '__((THAILAND|THIALAND|CHINA|CHAINA|INTERNATIONAL|NETMARBLE|TEST)_SERVER|OLD_GUILD_WAR|CONNECT_BILLING_SYSTEM|COUT_BILLING_SYSTEM|PAY_SYSTEM_(ZONE|LOGIN|FREE_LIMIT)|UNDERWORLD|ACTIVE_QUEST|ACTIVE_SERVICE_DEADLINE|WINDOWS)__' \
    src --include='*.h' --include='*.cpp' | wc -l)
check_ratchet R14 "mentions of macros nothing defines" 0 "$R14"

# --- R15: src/**/*.cpp that no target compiles -----------------------------
# A source no target names is never compiled, so nothing it says is true of a
# running server: it drifts out of sync with the headers it includes while
# still reading as live code. 39 were removed at once, among them a second
# PlayerManager.cpp whose body predated the encoding migration and a
# gameserver SocketImpl.h/.cpp pair that the Core one shadows.
#
# A source counts as built when a CMakeLists.txt names it, when
# tests/arch/kernel_files.txt lists it (src/Core/CMakeLists.txt feeds that
# file straight into de-kernel), or when another source #includes it. A
# CMake reference is relative to its own CMakeLists.txt, so the awk resolves
# each against the directory of FILENAME: a basename match would call
# src/server/gameserver/SocketImpl.cpp built because the kernel list carries
# src/Core/SocketImpl.cpp. Comments are stripped first -- two quest sources
# sat behind a '#' in a source list and were dead.
#
# Scoped to src plus the top-level CMakeLists.txt rather than a bare `find
# .`: the container configures its build tree inside the source root, and
# CMake's own compiler probes put a CMakeLists.txt and GLOB-using .cmake
# modules there. The name-based check is only sound while no target globs,
# so a file(GLOB ...) in the build files fails loudly instead of quietly
# weakening it, and the built-name list is materialised first for R13's
# reason: a broken find would otherwise read as zero dead sources.
if grep -n 'file([[:space:]]*GLOB' CMakeLists.txt \
    $(find src cmake -name CMakeLists.txt -o -name '*.cmake') 2>/dev/null; then
    echo "[FAIL] R15 unbuilt sources: a target globs its sources, so naming one proves nothing"
    fail=1
else
    r15_built=$(mktemp)
    r15_dead=$(mktemp)
    # The source-reference class spells its braces "}{": find rejects an
    # -exec ... {} + whose command text holds a second "{}", and a class
    # written "${}" would be that second one.
    find src -name CMakeLists.txt -exec awk '
        {
            dir = FILENAME
            sub(/\/?CMakeLists\.txt$/, "", dir)
            line = $0
            sub(/\r$/, "", line)
            sub(/#.*$/, "", line)
            while (match(line, /[A-Za-z0-9_$}{\/.+-]+\.cpp/)) {
                ref = substr(line, RSTART, RLENGTH)
                line = substr(line, RSTART + RLENGTH)
                sub(/^\$\{CMAKE_SOURCE_DIR\}\//, "", ref)
                sub(/^\$\{CMAKE_CURRENT_SOURCE_DIR\}\//, "", ref)
                if (ref ~ /\$\{/) continue
                if (ref !~ /^(src|tests|third_party)\// && dir != "") ref = dir "/" ref
                print ref
            }
        }' {} + > "$r15_built"
    grep -E '^src/.*\.cpp$' tests/arch/kernel_files.txt >> "$r15_built"
    LC_ALL=C grep -rhoE '#[[:space:]]*include[[:space:]]*"[^"]*\.cpp"' src \
        --include='*.cpp' --include='*.h' | sed 's/.*"\(.*\)"/\1/' >> "$r15_built"
    sort -u -o "$r15_built" "$r15_built"
    if [ "$(wc -l < "$r15_built")" -lt 1500 ]; then
        echo "[FAIL] R15 unbuilt sources: only $(wc -l < "$r15_built") built source names found (find or awk broken?)"
        fail=1
    else
        # An #include of a .cpp spells the path the way the including file
        # reaches it, so a built name may match the real path by suffix.
        find src -name '*.cpp' | sort | awk -v builtfile="$r15_built" '
            BEGIN { while ((getline b < builtfile) > 0) if (b != "") built[++n] = b }
            {
                for (i = 1; i <= n; i++) {
                    if ($0 == built[i]) next
                    if (length($0) > length(built[i]) &&
                        substr($0, length($0) - length(built[i])) == "/" built[i]) next
                }
                print
            }' > "$r15_dead"
        R15=$(wc -l < "$r15_dead")
        check_ratchet R15 "src sources no target compiles" 0 "$R15"
        if [ "$R15" -gt 0 ]; then
            sed 's/^/         /' < "$r15_dead"
        fi
    fi
    rm -f "$r15_built" "$r15_dead"
fi

# --- R16: headers under src/ that nothing includes -------------------------
# A header no translation unit reaches is not part of any build: nothing it
# declares is checked against the code it describes, so it drifts while
# still reading as live declarations and the compiler never says so. Five
# went at once, among them an ItemNumberManager.h whose body is not valid
# C++ and a CombatSystemManager.h for a relic system the servers run
# elsewhere. Three more headers were reached only by includes that used
# nothing from them, for classes declared here and defined nowhere; the
# includes went with the headers.
#
# The measure is an approximation, stated rather than hidden: a header
# counts as included when some #include "..." text under src/ or tests/
# equals the header's path or the path ends with "/" plus that text. That
# is looser than resolving each include against the including file's own
# directory and the -I list the CMake files build, so "Item.h" marks every
# path ending in /Item.h as reached. Every error it makes is in the same
# direction -- calling a header used -- so it never calls a live header
# dead. Comments are not stripped for the same reason: an include behind
# /* */ still counts as a mention.
#
# The include-text list is materialised first for R13's reason: a broken
# grep would otherwise read as zero orphan headers.
r16_inc=$(mktemp)
r16_dead=$(mktemp)
LC_ALL=C grep -rhoE '#[[:space:]]*include[[:space:]]*"[^"]*\.h"' src tests \
    --include='*.c' --include='*.cc' --include='*.cpp' --include='*.h' \
    --include='*.hpp' --include='*.inc' |
    sed 's/.*"\(.*\)"/\1/' | sort -u > "$r16_inc"
if [ "$(wc -l < "$r16_inc")" -lt 1500 ]; then
    echo "[FAIL] R16 orphan headers: only $(wc -l < "$r16_inc") include texts found (grep or find broken?)"
    fail=1
else
    find src -name '*.h' | sort | awk -v incfile="$r16_inc" '
        BEGIN { while ((getline i < incfile) > 0) if (i != "") inc[++n] = i }
        {
            for (k = 1; k <= n; k++) {
                if ($0 == inc[k]) next
                if (length($0) > length(inc[k]) &&
                    substr($0, length($0) - length(inc[k])) == "/" inc[k]) next
            }
            print
        }' > "$r16_dead"
    R16=$(wc -l < "$r16_dead")
    check_ratchet R16 "headers under src/ nothing includes" 0 "$R16"
    if [ "$R16" -gt 0 ]; then
        sed 's/^/         /' < "$r16_dead"
    fi
fi
rm -f "$r16_inc" "$r16_dead"

# --- R17: source lines carrying non-ASCII bytes ----------------------------
# The tree's code language is English, so each of these is a line a reader
# cannot read. The legacy text came through the encoding migration in three
# states: readable Korean; mojibake, where EUC-KR/CP949 bytes, or the Chinese
# build's GBK bytes, were decoded as Latin-1 or CP949 and re-encoded as
# UTF-8; and U+FFFD runs, where the text itself is gone and only the code
# beside it still says what it meant. The count holds at zero: a literal
# that must keep foreign bytes because data is matched against it (the
# reserved staff titles a character name may not contain, the chief-monster
# prefix, the GM chat aliases the command ladder still tests) is written as
# escaped UTF-8 bytes with an English comment beside it.
#
# Line-based, and the byte class is spelled the way R12 spells it: exclude
# everything from \x01 to \x7f, so what is left is a byte with the high bit
# set -- a UTF-8 lead or continuation byte. That keeps the CR of a CRLF
# working tree out of the count, which [^[:print:]] would not, and LC_ALL=C
# keeps the range byte-wise where a locale would read it as characters.
R17=$(LC_ALL=C grep -rhE $'[^\x01-\x7f]' src --include='*.h' --include='*.cpp' | wc -l)
check_ratchet R17 "source lines carrying non-ASCII bytes" 0 "$R17"

# --- R18: commented-out code inside /* */ blocks ---------------------------
# Code that was switched off years ago says nothing true about the running
# server: it names globals that were deleted, calls signatures that changed
# and describes branches the build no longer has. It also reads as if it
# were a description of the code beside it, which is the expensive part.
# Comments that explain behaviour stay; this counts the ones that are code.
#
# The measure is deliberately narrow, because a narrow measure can be
# trusted. Only lines inside a *multi-line* /* */ comment count, and only
# those that look like a statement: a line ending in ";", "{" or "}", a
# preprocessor directive, a line opening with a control keyword or a type
# name, or an identifier followed by "(". The same test over "//" lines was
# measured and rejected: on a sample of 50 it called 13 prose lines code --
# section banners ("// class Foo member methods"), end-of-block markers
# ("// for"), and ordinary sentences that happen to open with "delete" or
# to mention a function by name. One in four is too loose to ratchet on, so
# "//" commented-out code is removed by hand without being counted here.
#
# The scanner walks each file character by character rather than matching
# line by line, so a "/*" inside a string literal or behind a "//" does not
# open a block. The first and last lines of a block carry the delimiters
# and are left out by the same test that skips a bare "/*" or "*/".
#
# The five lines the count still holds are not switched-off code: they are
# the two block comments in handler/CGWhisperHandler.cpp that name, field by
# field, what the relay puts into a GGServerChat packet, and the scan reads
# a line like "pPacket->getColor() = the text color" as a call.
r18_raw=$(find src \( -name '*.cpp' -o -name '*.h' \) -print0 | perl -0 -ne '
    BEGIN { $files = 0; $count = 0 }
    chomp; my $f = $_;
    open(my $fh, "<", $f) or next; $files++;
    my $src = do { local $/; <$fh> }; close $fh;
    my @blocks; my $i = 0; my $n = length($src);
    while ($i < $n) {
        my $c = substr($src, $i, 1);
        if ($c eq "\"" or $c eq chr(39)) {
            my $q = $c; $i++;
            while ($i < $n) {
                my $d = substr($src, $i, 1);
                if ($d eq chr(92)) { $i += 2; next }
                $i++;
                last if $d eq $q or $d eq "\n";
            }
            next;
        }
        if ($c eq "/" and substr($src, $i + 1, 1) eq "/") {
            my $j = index($src, "\n", $i); $j = $n if $j < 0; $i = $j; next;
        }
        if ($c eq "/" and substr($src, $i + 1, 1) eq "*") {
            my $j = index($src, "*/", $i + 2); $j = $j < 0 ? $n : $j + 2;
            my $chunk = substr($src, $i, $j - $i);
            push @blocks, $chunk if $chunk =~ /\n/;
            $i = $j; next;
        }
        $i++;
    }
    for my $chunk (@blocks) {
        for my $line (split(/\n/, $chunk, -1)) {
            $line =~ s/\r$//;
            $line =~ s/^\s*\*(?=\s|$)//;
            $line =~ s/^\s+//; $line =~ s/\s+$//;
            next if $line eq "" or $line eq "*/" or $line eq "/*";
            $count++ if
                $line =~ /^#\s*(include|define|ifdef|ifndef|endif|else|elif|if|pragma|undef)\b/
             or $line =~ /[;{}]$/
             or $line =~ /^(if|for|while|switch|return|else|case|do|delete|new|goto|break|continue|try|catch|throw|struct|class|template|typedef|using|namespace|public|private|protected|const|static|void|int|bool|char|float|double|unsigned|uint|BYTE|WORD|DWORD|BOOL)\b/
             or $line =~ /^[A-Za-z_][A-Za-z0-9_:<>.\-\[\]*&]*\s*\(/;
        }
    }
    END { print "$files $count\n" }
')
# R13's rule: a scan that found almost nothing must fail loudly rather than
# read as a clean tree. src/ holds well over three thousand sources.
r18_files=${r18_raw%% *}
R18=${r18_raw##* }
if ! [[ "$r18_files" =~ ^[0-9]+$ ]] || [ "$r18_files" -lt 3000 ]; then
    echo "[FAIL] R18 commented-out code: only '$r18_files' files scanned (find or perl broken?)"
    fail=1
else
    check_ratchet R18 "commented-out code lines in /* */ blocks" 5 "$R18"
fi

# --- Removed dead services must not return --------------------------------
# China billing, theoneserver, updateserver, cacheserver (all 2026-09-05),
# and the log client that never opened its socket (2026-09-22).
# Historical build logs and documentation are not build inputs.
if grep -riE 'chinabilling|cbilling|theoneserver|TOpackets|updateserver|Upackets|__UPDATE_(SERVER|CLIENT)__|cacheserver|LogClient|LogData|openLogClient' src \
    --include='*.cpp' --include='*.h' --include='*.hpp' \
    --include='CMakeLists.txt' --include='*.cmake' --include='Makefile'; then
    echo "[FAIL] obsolete dead-service references remain in source/build files"
    fail=1
else
    scan_status=$?
    if [ "$scan_status" -eq 1 ]; then
        echo "[OK]   no obsolete dead-service source/build references"
    else
        echo "[FAIL] could not scan for obsolete dead-service references"
        fail=1
    fi
fi

# --- Every dump table is InnoDB in utf8mb4 --------------------------------
# The servers pin their session to utf8mb4 (database/Connection.cpp); a table
# in another character set would silently transcode or truncate what they
# store. The dumps must create both databases and all tables that way. The
# trailing [[:space:]]* tolerates the CR of a CRLF checkout synced into the
# container (grep's ERE has no \r escape).
if grep -nE '^\) ENGINE=' initdb/*.sql |
    grep -vE 'ENGINE=InnoDB( AUTO_INCREMENT=[0-9]+)? DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;[[:space:]]*$'; then
    echo "[FAIL] a dump table is not InnoDB + utf8mb4 + utf8mb4_unicode_ci (see above)"
    fail=1
elif grep -nE 'latin1|CHARSET=utf8;|SET NAMES utf8 |character_set_client = utf8 ' initdb/*.sql; then
    echo "[FAIL] a dump still names latin1 or three-byte utf8 (see above)"
    fail=1
else
    echo "[OK]   every dump table is InnoDB + utf8mb4_unicode_ci"
fi

# --- No seed guild belongs to two guild unions ------------------------------
# A guild is in at most one union, as its master or as a member: the join
# offer and its acceptance refuse a guild already in one, and the union
# manager maps each guild to a single union. GuildUnionMember's key is
# (OwnerGuildID, UnionID), so the table itself would take a second row, and
# a guild with two gets a different answer from the union manager's map
# than from its member row. The scan fails, rather than passes, when it
# finds no union rows to check.
union_dupes=$(perl -ne '
    if (/^INSERT INTO `GuildUnionInfo` VALUES (.*);/) {
        my $v = $1;
        while ($v =~ /\((\d+),(\d+)\)/g) { $unions++; $in{$2}{$1} = 1; }
    }
    if (/^INSERT INTO `GuildUnionMember` VALUES (.*);/) {
        my $v = $1;
        while ($v =~ /\((\d+),(\d+)\)/g) { $in{$2}{$1} = 1; }
    }
    END {
        unless ($unions) { print "NO_UNIONS\n"; exit; }
        for my $g (sort { $a <=> $b } keys %in) {
            my @u = sort { $a <=> $b } keys %{ $in{$g} };
            print "guild $g is in unions @u\n" if @u > 1;
        }
    }' initdb/DARKEDEN.sql)
if [ "$union_dupes" = "NO_UNIONS" ]; then
    echo "[FAIL] no GuildUnionInfo rows found in initdb/DARKEDEN.sql (dump layout changed?)"
    fail=1
elif [ -n "$union_dupes" ]; then
    echo "$union_dupes"
    echo "[FAIL] a seed guild belongs to more than one guild union (see above)"
    fail=1
else
    echo "[OK]   no seed guild belongs to two guild unions"
fi

# --- Every seed union has a member or a pending join offer ------------------
# The union manager opens a union only for a join offer it records, and
# dissolves one once it has neither a member nor a pending join offer: when
# its last member leaves with no offer pending, or its last pending offer is
# accepted without a join, denied, or expires (unionIsAbandoned in
# src/server/gameserver/guild/GuildUnionJoinOffer.h, applied by every path
# that takes a member or an offer away). A union with neither
# a GuildUnionMember row nor a JOIN row in GuildUnionOffer keeps its master
# guild "in a union", so that guild can join no other. A QUIT or ESCAPE
# row does not count: the first is a member's, the second a former member's
# penalty.
union_abandoned=$(perl -ne '
    if (/^INSERT INTO `GuildUnionInfo` VALUES (.*);/) {
        my $v = $1;
        while ($v =~ /\((\d+),(\d+)\)/g) { $unions++; $master{$1} = $2; }
    }
    if (/^INSERT INTO `GuildUnionMember` VALUES (.*);/) {
        my $v = $1;
        while ($v =~ /\((\d+),(\d+)\)/g) { $held{$1} = 1; }
    }
    if (/^INSERT INTO `GuildUnionOffer` VALUES (.*);/) {
        my $v = $1;
        while ($v =~ /\((\d+),\x27JOIN\x27,(\d+),/g) { $held{$1} = 1; }
    }
    END {
        unless ($unions) { print "NO_UNIONS\n"; exit; }
        for my $u (sort { $a <=> $b } keys %master) {
            print "union $u (master guild $master{$u}) has no member and no join offer\n" unless $held{$u};
        }
    }' initdb/DARKEDEN.sql)
if [ "$union_abandoned" = "NO_UNIONS" ]; then
    echo "[FAIL] no GuildUnionInfo rows found in initdb/DARKEDEN.sql (dump layout changed?)"
    fail=1
elif [ -n "$union_abandoned" ]; then
    echo "$union_abandoned"
    echo "[FAIL] a seed guild union has no member and no pending join offer (see above)"
    fail=1
else
    echo "[OK]   every seed guild union has a member or a pending join offer"
fi

# --- Every seed join offer names a union ------------------------------------
# A JOIN row in GuildUnionOffer asks to join a union that exists: the union
# manager opens the union before it records the offer, and a union that goes
# takes the JOIN and QUIT rows naming it with it. An offer naming no union
# can be neither accepted nor denied, and the one row per guild it occupies
# answers every later offer of that guild with ALREADY_OFFER_SOMETHING. The
# server's offer purge (decideUnionOfferPurge in
# src/server/gameserver/guild/GuildUnionJoinOffer.h) drops such a row on
# load; the seed does not carry one. A seed without offer rows passes.
union_orphan_offers=$(perl -ne '
    if (/^INSERT INTO `GuildUnionInfo` VALUES (.*);/) {
        my $v = $1;
        while ($v =~ /\((\d+),(\d+)\)/g) { $unions++; $exists{$1} = 1; }
    }
    if (/^INSERT INTO `GuildUnionOffer` VALUES (.*);/) {
        my $v = $1;
        while ($v =~ /\((\d+),\x27JOIN\x27,(\d+),/g) { push @joins, [$1, $2]; }
    }
    END {
        unless ($unions) { print "NO_UNIONS\n"; exit; }
        for my $j (@joins) {
            print "guild $j->[1] offers to join union $j->[0], which does not exist\n" unless $exists{$j->[0]};
        }
    }' initdb/DARKEDEN.sql)
if [ "$union_orphan_offers" = "NO_UNIONS" ]; then
    echo "[FAIL] no GuildUnionInfo rows found in initdb/DARKEDEN.sql (dump layout changed?)"
    fail=1
elif [ -n "$union_orphan_offers" ]; then
    echo "$union_orphan_offers"
    echo "[FAIL] a seed join offer names a guild union that does not exist (see above)"
    fail=1
else
    echo "[OK]   every seed join offer names an existing guild union"
fi

# --- Generated factory list is fresh ---------------------------------------
# The generator only writes to $OUT, so point it at a scratch copy of the
# tree's file rather than overwriting the tracked one: an interrupt (Ctrl-C,
# ctest timeout) between generate and restore used to leave the committed
# file replaced or truncated.
INC=tests/generated/AllPacketFactories.inc
scratch_dir=$(mktemp -d)
trap 'rm -rf "$scratch_dir"' EXIT
mkdir -p "$scratch_dir/tests/generated" "$scratch_dir/tests/tools" \
         "$scratch_dir/tests/arch" "$scratch_dir/src"
cp -r src/Core "$scratch_dir/src/" 2>/dev/null
# The generator reads the kernel membership list (the compiled packet set
# since the 2.4 flip), not the CMake source lists it used to parse.
cp tests/arch/kernel_files.txt "$scratch_dir/tests/arch/"
cp tests/tools/gen_factory_list.sh "$scratch_dir/tests/tools/"
if (cd "$scratch_dir" && bash tests/tools/gen_factory_list.sh > /dev/null 2>&1) &&
   diff -q "$INC" "$scratch_dir/$INC" > /dev/null 2>&1; then
    echo "[OK]   AllPacketFactories.inc matches a fresh generation"
else
    echo "[FAIL] AllPacketFactories.inc is stale — run tests/tools/gen_factory_list.sh and commit the result"
    fail=1
fi

# --- Every server-side factory the manager registers is in the inventory ---
# Registrations deliberately outside the inventory are listed (with reasons)
# in tests/ratchet/factory_exceptions.txt — currently empty since the
# __GAME_CLIENT__ relic registrations were deleted in 2.4. The file must
# still exist: with zero entries nothing exercises this plumbing, so a
# deleted or mistyped path would otherwise pass silently. The `|| true`
# keeps the no-match grep exit (1) from mattering if this script ever
# adopts `set -e` like its siblings.
if [ ! -f tests/ratchet/factory_exceptions.txt ]; then
    echo "[FAIL] tests/ratchet/factory_exceptions.txt is missing"
    fail=1
fi
registered=$(mktemp)
inventory=$(mktemp)
# The registrations are the `using <Name>Factories = FactoryList<...>;` type
# lists that init() concatenates per server (PacketMeta.h).
sed -n '/^using [A-Za-z0-9_]*Factories = FactoryList</,/>;/p' src/Core/PacketFactoryManager.cpp |
    grep -oE '^ +[A-Za-z0-9_]+Factory' | tr -d ' ' | sort -u > "$registered"
if [ ! -s "$registered" ]; then
    echo "[FAIL] no FactoryList registrations found in PacketFactoryManager.cpp"
    fail=1
fi
{
    grep -oE 'new [A-Za-z0-9_]+Factory' tests/generated/AllPacketFactories.inc | sed 's/new //'
    grep -vE '^\s*(#|$)' tests/ratchet/factory_exceptions.txt || true
} | sort -u > "$inventory"
missing=$(comm -23 "$registered" "$inventory")
rm -f "$registered" "$inventory"
if [ -n "$missing" ]; then
    echo "[FAIL] factories registered in PacketFactoryManager but missing from the wire inventory:"
    echo "$missing" | sed 's/^/         /'
    echo "         (add the packet sources to tests/arch/kernel_files.txt and regenerate, or"
    echo "          justify an entry in tests/ratchet/factory_exceptions.txt)"
    fail=1
else
    echo "[OK]   every registered factory is covered by the wire inventory"
fi

# --- Per-server registration membership is pinned ---------------------------
# The check above only proves registered <= inventory, so it cannot see a
# registration that was dropped. tests/tools/factory_registrations.pl derives
# each server's set from the FactoryList type lists and the per-server Concat
# selection; the committed tests/ratchet/factory_registrations.txt is the
# expected membership (as of the switch to type lists, identical to the
# addFactory() sequence it replaced). Any add or drop fails here; when it is
# intended, regenerate the file with the command in the script's header.
expected=tests/ratchet/factory_registrations.txt
if [ ! -f "$expected" ]; then
    echo "[FAIL] $expected is missing"
    fail=1
elif membership_diff=$(perl tests/tools/factory_registrations.pl | diff "$expected" - 2>&1); then
    echo "[OK]   per-server factory registrations match $expected ($(wc -l < "$expected") entries)"
else
    echo "[FAIL] per-server factory registrations differ from $expected:"
    echo "$membership_diff" | sed 's/^/         /'
    echo "         (if the change is intended: perl tests/tools/factory_registrations.pl > $expected)"
    fail=1
fi

# --- Every encrypter-using packet has per-code goldens -------------------
# A packet whose read/write call readEncrypt/writeEncrypt puts bytes on the
# wire that depend on the session encrypt code, and for the shuffled ones
# on which code % N case runs. packet_encrypter_test.cpp pins them all;
# this catches the next packet that starts using the encrypter without a
# golden (exceptions, with reasons: tests/ratchet/encrypter_exceptions.txt).
# Checks for code 5 specifically: that is the code that reaches the
# SHUFFLE_STATEMENT_5 case 0 order through the encrypted branch.
unpinned=$(grep -lE '(read|write)Encrypt\(' src/Core/*.cpp |
    xargs -n1 basename | sed 's/\.cpp$//' | sort -u |
    grep -vxFf <(grep -vE '^\s*(#|$)' tests/ratchet/encrypter_exceptions.txt) |
    while read -r name; do
        [ -f "tests/golden/$name.code5.hex" ] || echo "$name"
    done)
if [ -n "$unpinned" ]; then
    echo "[FAIL] packets use the encrypter but have no per-code goldens (tests/golden/<Name>.code5.hex):"
    echo "$unpinned" | sed 's/^/         /'
    echo "         (add fill()/expectEqual() + ENCRYPTER_PACKET_TESTS in tests/packet_encrypter_test.cpp and record)"
    fail=1
else
    echo "[OK]   every encrypter-using packet has per-code goldens"
fi

exit $fail
