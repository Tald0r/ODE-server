cmake_minimum_required(VERSION 3.16)
project(DarkEden VERSION 1.0 LANGUAGES C CXX)

# ============================================================================
# Global settings
# ============================================================================
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ language standard (20)")
set_property(CACHE CMAKE_CXX_STANDARD PROPERTY STRINGS 20)
if(NOT CMAKE_CXX_STANDARD EQUAL 20)
    message(FATAL_ERROR "DarkEden requires CMAKE_CXX_STANDARD=20")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories. Development lanes override the root so different compiler,
# target and build-type combinations cannot share artifacts.
set(DARKEDEN_OUTPUT_ROOT "${CMAKE_SOURCE_DIR}" CACHE PATH "Root directory for DarkEden build artifacts")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${DARKEDEN_OUTPUT_ROOT}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${DARKEDEN_OUTPUT_ROOT}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${DARKEDEN_OUTPUT_ROOT}/bin")

# ============================================================================
# Global compile options
# ============================================================================
# Base macro definitions - every target needs them
if(APPLE)
    add_compile_definitions(__APPLE__ _REENTRANT)
elseif(UNIX)
    add_compile_definitions(__LINUX__ _REENTRANT)
endif()

# Warning settings
set(WARNING_FLAGS
    -Wall
    -Wformat # kept on since the 2026-09-06 width fix; Statement::executeQuery carries the printf attribute
    -Wno-format-security
)
add_compile_options(${WARNING_FLAGS})

# Build type flags.
#
# NDEBUG is never defined. The code base used it for far more than
# assertions, and none of it is safe to drop:
#
#   * ~165 call sites put the work inside Assert(expr), e.g.
#     Assert(pTree->GetAttribute("class", iClass)) in
#     GQuestGiveItemElement.cpp.
#   * 344 call sites use __END_CATCH_NO_RETHROW (Exception.h) to swallow
#     exceptions, mostly in client packet handlers; with the handlers
#     compiled away those exceptions would escape instead.
#
# src/Core/Assert.h and Exception.h stop the compile with an #error when
# NDEBUG is defined, so a toolchain or flag that defines it fails the
# build instead of changing what the servers do.
# DE_OWNERSHIP_CHECKS arms the thread-ownership machinery (task 3.4:
# ZoneGroup lock bookkeeping + the aborting assertOwned() on the Zone
# mutation gateways). It rides its own macro rather than NDEBUG because this
# project deliberately never defines NDEBUG (see below) — Debug is the only
# configuration where the checks exist at all; every optimized build
# compiles them away completely.
#
# Setting these flags is not enough on its own: `zig c++` defines NDEBUG
# itself for every optimized mode (-O1 and up, -Os), so the optimized
# configurations undefine it again with -UNDEBUG. The C flags keep CMake's
# defaults: the only C target is the vendored argon2, which has no assert()
# and never includes Assert.h.
set(CMAKE_CXX_FLAGS_DEBUG "-g -DDE_OWNERSHIP_CHECKS")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -UNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -UNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -UNDEBUG")

# ============================================================================
# Dependencies
# ============================================================================
find_package(Threads REQUIRED)

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_LIBRARIES Threads::Threads)
check_cxx_source_compiles("
    #include <thread>
    #include <stop_token>
    #include <condition_variable>
    #include <mutex>
    #include <chrono>
    int main() {
        std::jthread worker([](std::stop_token token) {
            std::mutex mutex;
            std::condition_variable_any wake;
            std::unique_lock lock(mutex);
            wake.wait_for(lock, token, std::chrono::milliseconds(1), [] { return false; });
        });
        worker.request_stop();
    }
" DARKEDEN_HAS_COOPERATIVE_THREADS)
unset(CMAKE_REQUIRED_LIBRARIES)
if(NOT DARKEDEN_HAS_COOPERATIVE_THREADS)
    message(FATAL_ERROR "DarkEden needs a C++20 standard library with std::jthread, std::stop_token and stop-aware condition_variable_any. Use the pinned Zig toolchain: docker build -f Dockerfile.dev -t darkeden-dev . then make dev-test.")
endif()

# MySQL
if(APPLE)
    # Homebrew's mysql-client formulae are keg-only: nothing is linked into
    # <prefix>/include or <prefix>/lib, so name their opt/ directories, for
    # both Homebrew prefixes (/opt/homebrew on Apple Silicon, /usr/local on
    # Intel). mysql-client@8.4 comes first because it is the client the
    # native build was verified with; the unversioned formula is a newer
    # series, used only when no versioned keg is installed.
    find_path(MYSQL_INCLUDE_DIR mysql/mysql.h
        PATHS
        /opt/homebrew/include
        /opt/homebrew/opt/mysql-client@8.4/include
        /opt/homebrew/opt/mysql-client@8.0/include
        /opt/homebrew/opt/mysql-client/include
        /usr/local/opt/mysql-client@8.4/include
        /usr/local/opt/mysql-client@8.0/include
        /usr/local/opt/mysql-client/include
        /opt/homebrew/Cellar/mysql-client/*/include
        /opt/homebrew/Cellar/mariadb-connector-c/*/include
        /usr/local/include
        NO_DEFAULT_PATH
    )
    find_library(MYSQL_LIBRARY mysqlclient
        PATHS
        /opt/homebrew/lib
        /opt/homebrew/opt/mysql-client@8.4/lib
        /opt/homebrew/opt/mysql-client@8.0/lib
        /opt/homebrew/opt/mysql-client/lib
        /usr/local/opt/mysql-client@8.4/lib
        /usr/local/opt/mysql-client@8.0/lib
        /usr/local/opt/mysql-client/lib
        /opt/homebrew/Cellar/mysql-client/*/lib
        /opt/homebrew/Cellar/mariadb-connector-c/*/lib
        /opt/homebrew/Cellar/mariadb-connector-c/*/lib/mariadb
        /usr/local/lib
        NO_DEFAULT_PATH
    )
elseif(UNIX)
    find_path(MYSQL_INCLUDE_DIR mysql/mysql.h
        PATHS /usr/include /usr/local/include
        NO_DEFAULT_PATH
    )
    find_library(MYSQL_LIBRARY mysqlclient
        PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu
    )
endif()

# Lua 5.1 / LuaJIT
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(LUA51 lua5.1)
endif()
if(NOT LUA51_FOUND)
    if(APPLE)
        find_path(LUA51_INCLUDE_DIRS lua.h
            PATHS
            /opt/homebrew/opt/luajit/include/luajit-2.1
            /opt/homebrew/include/luajit-2.1
            /opt/homebrew/include/lua5.1
            /usr/local/include/lua5.1
        )
        find_library(LUA51_LIBRARIES luajit
            PATHS /opt/homebrew/lib /usr/local/lib
        )
        if(NOT LUA51_LIBRARIES)
            find_library(LUA51_LIBRARIES lua5.1
                PATHS /opt/homebrew/lib /usr/local/lib
            )
        endif()
    else()
        find_path(LUA51_INCLUDE_DIRS lua.h PATHS /usr/include/lua5.1 /usr/local/include/lua5.1)
        find_library(LUA51_LIBRARIES lua5.1 PATHS /usr/lib /usr/local/lib)
    endif()
endif()

# XML parsing: vendored tinyxml2 (third_party/tinyxml2, pinned at 10.0.0).
#
# Replaces xerces-c. The five data/*.xml files declare no schema, DTD or
# namespace, so none of xerces' validation machinery had anything to act on,
# and it was the project's only C++-ABI-sensitive dependency -- which made it
# the one thing standing between this build and an alternative toolchain
# (libc++ vs libstdc++ would clash on the XMLException catch in SXml.cpp).
# Vendoring keeps the version pinned in-tree instead of taking whatever the
# host distribution happens to ship.
add_library(tinyxml2 STATIC third_party/tinyxml2/tinyxml2.cpp)
target_include_directories(tinyxml2 PUBLIC third_party/tinyxml2)
set_target_properties(tinyxml2 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Password hashing: vendored argon2 reference implementation
# (third_party/argon2, pinned at 20190702, C API). ref.c is the portable
# fill; the SSE build (opt.c) is deliberately not vendored so one source set
# serves every Zig target. Always optimized: a Debug loginserver would
# otherwise spend seconds per login in an -O0 memory-hard function, and
# there is nothing to debug in the library itself.
add_library(argon2 STATIC
    third_party/argon2/src/argon2.c
    third_party/argon2/src/core.c
    third_party/argon2/src/encoding.c
    third_party/argon2/src/ref.c
    third_party/argon2/src/thread.c
    third_party/argon2/src/blake2/blake2b.c
)
target_include_directories(argon2 PUBLIC third_party/argon2/include)
target_compile_options(argon2 PRIVATE -O2)
target_link_libraries(argon2 PUBLIC Threads::Threads)
set_target_properties(argon2 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ============================================================================
# Include directories (global)
# ============================================================================
set(DARKEDEN_PROJECT_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/server/database
)

# /usr/include is already an implicit native include path.  Passing it
# explicitly to a Zig cross-target build puts host headers ahead of Zig's
# target sysroot and produces an invalid mixture of libc headers.
if(MYSQL_INCLUDE_DIR AND
   (NOT DARKEDEN_ZIG_TARGET OR NOT MYSQL_INCLUDE_DIR STREQUAL "/usr/include"))
    list(APPEND DARKEDEN_PROJECT_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})
endif()

# LuaState.h spells the macOS headers <luajit-2.1/lua.h>, so the include root
# is the directory above the one found for them, which Apple Clang does not
# search by default the way Linux searches /usr/include. With Homebrew's
# luajit that is the keg's own include/, which holds nothing but LuaJIT.
# The PATHS above are CMake's last resort, though: the find_path searches
# CMake's default prefixes first, so a lua.h directly under one of those
# wins, and the lua5.1 fallbacks make the root Homebrew's shared include/,
# whose headers can then shadow the system's.
if(APPLE AND LUA51_INCLUDE_DIRS)
    get_filename_component(DARKEDEN_LUA_INCLUDE_ROOT "${LUA51_INCLUDE_DIRS}" DIRECTORY)
    list(APPEND DARKEDEN_PROJECT_INCLUDE_DIRS ${DARKEDEN_LUA_INCLUDE_ROOT})
endif()

include_directories(${DARKEDEN_PROJECT_INCLUDE_DIRS})

include_directories(third_party/tinyxml2)

# ============================================================================
# Subdirectories
# ============================================================================
option(DARKEDEN_BUILD_TESTS "Build the wire-contract test suite (tests/)" OFF)

add_subdirectory(src/Core)
add_subdirectory(src/domain)
add_subdirectory(src/server)

if(DARKEDEN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Install rules
# ============================================================================
install(TARGETS gameserver loginserver sharedserver
    RUNTIME DESTINATION bin
)
