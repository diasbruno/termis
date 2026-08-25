.PHONY: help configure build examples test clean compile-flags FORCE

BUILD_DIR ?= build
GENERATOR ?= Unix Makefiles
EXAMPLE_SOURCES := $(wildcard examples/*.termis)
EXAMPLE_BINS := $(patsubst examples/%.termis,$(BUILD_DIR)/examples/%,$(EXAMPLE_SOURCES))

help:
	@printf '%s\n' 'Termis development commands:'
	@printf '%s\n' ''
	@printf '%s\n' '  make configure  Configure the CMake build directory'
	@printf '%s\n' '  make build      Build termisc'
	@printf '%s\n' '  make examples   Build every example program'
	@printf '%s\n' '  make test       Run the test suite'
	@printf '%s\n' '  make compile-flags'
	@printf '%s\n' '                  Generate compile_flags.txt for Clang tooling'
	@printf '%s\n' '  make clean      Remove the build directory'
	@printf '%s\n' ''
	@printf '%s\n' 'Variables:'
	@printf '%s\n' '  BUILD_DIR       Build directory, default: build'
	@printf '%s\n' '  GENERATOR       CMake generator, default: Unix Makefiles'

configure:
	cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)"

build: configure
	cmake --build $(BUILD_DIR)

examples: build $(EXAMPLE_BINS)

$(BUILD_DIR)/examples/%: examples/%.termis
	@mkdir -p $(dir $@)
	$(BUILD_DIR)/termisc -I std -o $@ $<

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

compile-flags: compile_flags.txt

compile_flags.txt: FORCE
	@{ \
		printf '%s\n' \
			'-xc++' \
			'-std=c++20' \
			'-I' \
			'$(CURDIR)/src' \
			'-Wall' \
			'-Wextra' \
			'-Wpedantic'; \
		clang++ -E -x c++ - -v < /dev/null 2>&1 \
			| awk '/#include <...> search starts here:/{include=1; next} /End of search list./{include=0} include { sub(/^ /, ""); if (sub(/ \(framework directory\)$$/, "")) { print "-iframework"; print } else { print "-isystem"; print } }'; \
	} > compile_flags.txt

clean:
	cmake -E rm -rf $(BUILD_DIR)
