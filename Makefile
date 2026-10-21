# DarkEden Makefile

.PHONY: all fmt fmt-check fmt-check-all clean help debug release test \
        dev-test dev-build dev-shell dev-clean integration-test

# Default target
all: debug

# Wire-contract test suite (docs/reviews/RESTRUCTURING.md Phase 1). Run it locally
# before every push; CI (.github/workflows/cpp20.yml) runs the same suite via
# tools/devbuild.sh only on master pushes/merges, to conserve Actions minutes.
test:
	cmake -B build -DCMAKE_BUILD_TYPE=Debug -DDARKEDEN_BUILD_TESTS=ON
	cmake --build build --target wire_tests -j$(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
	cd build && ctest --output-on-failure

# Containerised build and test (tools/devbuild.sh). Prefer these on a machine
# where the checkout is a bind mount into the container - a Windows host, say -
# because compiling straight off the mount is I/O bound and leaves most of the
# CPU idle. Measured on this project: a full build went from ~20 minutes at
# ~20% CPU to ~3.5 minutes at ~95%, and a no-op rebuild from minutes to
# seconds. The script explains why; it needs the darkeden-dev image
# (docker build -f Dockerfile.dev -t darkeden-dev .).
dev-test:
	bash tools/devbuild.sh test
dev-build:
	bash tools/devbuild.sh build
dev-shell:
	bash tools/devbuild.sh shell
dev-clean:
	bash tools/devbuild.sh clean

# MySQL-backed repository integration tier (docs/reviews/RESTRUCTURING.md 3.2):
# throwaway MySQL 5.7 + initdb/ schema + the real MySQL*Repository impls,
# fixture lifecycle handrolled in the script. Needs docker + darkeden-dev.
integration-test:
	bash tests/integration/mysql_test.sh

release:
	cmake -B build -DCMAKE_BUILD_TYPE=Release -DDARKEDEN_BUILD_TESTS=OFF
	cmake --build build -j$(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

debug:
	cmake -B build -DCMAKE_BUILD_TYPE=Debug -DDARKEDEN_BUILD_TESTS=OFF
	cmake --build build -j$(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)


# Format code with clang-format
fmt:
	@echo "Formatting C++ code with clang-format..."
	find src tests -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -exec clang-format -i {} +
	@echo "Code formatted successfully!"

# Check format for modified files only (fast)
fmt-check:
	@echo "Checking format for modified files..."
	@failed=false; \
	files=$$(git diff --name-only --diff-filter=ACM | grep -E '\.(cpp|h|hpp)$$' || true); \
	if [ -z "$$files" ]; then \
		echo "No modified C++ files to check."; \
	else \
		for file in $$files; do \
			if [ -f "$$file" ]; then \
				if ! clang-format "$$file" | diff -q "$$file" - > /dev/null 2>&1; then \
					echo "[FAIL] $$file needs formatting"; \
					failed=true; \
				fi; \
			fi; \
		done; \
	fi; \
	if $$failed; then \
		echo ""; \
		echo "Format only the files you touched (see CLAUDE.md), e.g.:"; \
		echo "  clang-format -i <file>"; \
		exit 1; \
	fi; \
	echo "[OK] All modified files are properly formatted!"

# Check format for all files (slow)
fmt-check-all:
	@echo "Checking format for ALL files..."
	@failed=false; \
	for file in $$(find src tests -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \)); do \
		if ! clang-format "$$file" | diff -q "$$file" - > /dev/null 2>&1; then \
			echo "[FAIL] $$file needs formatting"; \
			failed=true; \
		fi; \
	done; \
	if $$failed; then \
		echo ""; \
		echo "Run 'make fmt' to fix formatting issues"; \
		exit 1; \
	fi; \
	echo "[OK] All files are properly formatted!"

# Clean build artifacts
clean:
	rm -rf build bin lib

# Show help message
help:
	@echo "DarkEden Makefile targets:"
	@echo "  all           - Debug build (default, same as 'debug')"
	@echo "  debug         - Debug build (-DCMAKE_BUILD_TYPE=Debug)"
	@echo "  release       - Release build (-DCMAKE_BUILD_TYPE=Release)"
	@echo "  test          - Build and run the wire-contract test suite"
	@echo "  dev-test      - Same suite, built in the darkeden-dev container volume"
	@echo "  dev-build     - All production targets, built in the container volume"
	@echo "  dev-shell     - Shell in the container workspace"
	@echo "  dev-clean     - Drop the container workspace + compiler-cache volumes"
	@echo "  integration-test - MySQL-backed repository tests (docker + darkeden-dev)"
	@echo "  fmt           - Format ALL C++ code (avoid before committing; see CLAUDE.md)"
	@echo "  fmt-check     - Check format for modified files only (fast)"
	@echo "  fmt-check-all - Check format for all files (slow)"
	@echo "  clean         - Clean build artifacts"
	@echo "  help          - Show this help message"
