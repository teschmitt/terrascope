# ---------------------------------------------------------------------------
# Convenience targets for formatting and linting.
#
# Usage:
#   make format          Auto-format all source and test files
#   make format-check    Check formatting (dry-run, exits non-zero on diff)
#   make lint            Run clang-tidy on compiled sources
# ---------------------------------------------------------------------------

# Auto-detect versioned tool names (devcontainer has clang-tidy-20, CI has clang-tidy)
CLANG_FORMAT := $(or $(shell command -v clang-format 2>/dev/null), \
                     $(shell command -v clang-format-20 2>/dev/null))
CLANG_TIDY   := $(or $(shell command -v clang-tidy 2>/dev/null), \
                     $(shell command -v clang-tidy-20 2>/dev/null))

BUILD_DIR    ?= build
PROJECT_ROOT := $(CURDIR)

# All source and header files under src/ and tests/
FORMAT_FILES := $(shell find src tests -name '*.c' -o -name '*.h')

# Only lint files that are actually in compile_commands.json (the build
# decides which conditionally-compiled sources are included).
LINT_FILES    = $(shell grep '"file"' $(BUILD_DIR)/compile_commands.json 2>/dev/null \
                  | sed 's/.*"file": "//;s/".*//' \
                  | grep '$(PROJECT_ROOT)/src/')

.PHONY: format format-check lint

format:
	$(CLANG_FORMAT) -i $(FORMAT_FILES)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)

lint:
	@test -f $(BUILD_DIR)/compile_commands.json || \
		{ echo "error: build first to generate compile_commands.json (west build -b qemu_riscv64 -p)"; exit 1; }
	@$(CLANG_TIDY) -p $(BUILD_DIR) --extra-arg-before=-Qunused-arguments $(LINT_FILES) 2>&1 \
		| tee /tmp/clang-tidy-output.txt; \
	if grep -q '$(PROJECT_ROOT)/src/.*warning:' /tmp/clang-tidy-output.txt; then \
		echo ""; \
		echo "clang-tidy warnings found in project sources:"; \
		grep '$(PROJECT_ROOT)/src/.*warning:' /tmp/clang-tidy-output.txt; \
		exit 1; \
	fi
