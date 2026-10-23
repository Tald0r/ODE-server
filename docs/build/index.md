# Guides

* [Build commands](build-commands.md) - make wraps CMake: Debug by default with tests off, binaries in bin/ and libraries in lib/, and Debug is the configuration with the ownership checks armed.
* [Container builds](container-builds.md) - Never compile off the bind mount; tools/devbuild.sh syncs the inputs into a volume and builds there with Ninja and ccache: a full build takes ~3.5 minutes instead of ~20.
* [Native macOS builds](native-macos.md) - The servers build natively with Apple Clang and Homebrew's keg-only mysql-client@8.4 and luajit; the test suite needs the system assert.h forced in, and three tests fail only there.
* [Toolchain notes](toolchain.md) - Why xerces-c became vendored tinyxml2, how the project builds with zig cc, and where C++20 pays off: thread lifetime, typed packet buffers, compile-time packet metadata and location-free diagnostics.

# Reference

* [Dependencies](dependencies.md) - libmysqlclient, Lua 5.1 or LuaJIT and zlib from the system; tinyxml2 and argon2 vendored; every one a C API, which the Zig toolchain depends on.

# Rules

* [CI](ci.md) - The C++20 and format workflows run on master pushes and manual dispatch only, so every branch is verified locally with the full suite before it is pushed.
* [Formatting](formatting.md) - clang-format only the files you touched; a bare make fmt buries a diff in churn because most of src/ was formatted with an older version.
