SHELL := /usr/bin/env bash

BUILD_TYPE ?= Debug
BUILD_DIR ?= build/$(BUILD_TYPE)
SOURCE_DIRS := apps libs
CPP_FILES := $(shell find $(SOURCE_DIRS) -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \))
CPP_SOURCES := $(shell find $(SOURCE_DIRS) -type f -name "*.cpp")
JOBS ?= $(shell nproc)
CONAN := python3 -m conans.conan
CONANFILE ?= external/conanfile.txt
TOOLCHAIN_FILE := $(BUILD_DIR)/conan_toolchain.cmake

.PHONY: help all setup deps configure build test format check ci clean

help:
	@echo "Targets:"
	@echo "  make setup         # Install system dependencies (Ubuntu/Debian)"
	@echo "  make deps          # Install project dependencies (Conan)"
	@echo "  make configure     # Configuring CMake"
	@echo "  make build         # Build project"
	@echo "  make test          # Run CTest"
	@echo "  make format        # Apply clang-format"
	@echo "  make check         # Validate formatting and run clang-tidy"
	@echo "  make ci            # check + test"
	@echo "Variables: BUILD_TYPE=Debug/Release (default Debug)"

all: check test

setup:
	bash scripts/setup.sh

deps:
	$(CONAN) profile detect --force
	$(CONAN) install $(CONANFILE) -of $(BUILD_DIR) -s build_type=$(BUILD_TYPE) --build=missing

configure: deps
	cmake -S . -B $(BUILD_DIR) -G Ninja \
    	-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) \
    	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
    	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build: configure
	cmake --build $(BUILD_DIR) -j $(JOBS)

test: build
	ctest --test-dir $(BUILD_DIR) -j $(JOBS) --output-on-failure

format:
	@if [ -n "$(CPP_FILES)" ]; then clang-format -i $(CPP_FILES); fi

check: configure
	@if [ -n "$(CPP_FILES)" ]; then clang-format --dry-run --Werror $(CPP_FILES); fi
	@if [ -n "$(CPP_SOURCES)" ]; then clang-tidy -p $(BUILD_DIR) $(CPP_SOURCES); fi

ci: check test

clean:
	rm -rf build
