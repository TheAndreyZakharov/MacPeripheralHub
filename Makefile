.PHONY: all build-app build-release core-lib test-core test-all clean

PROJECT := MacPeripheralHub.xcodeproj
SCHEME := MacPeripheralHub
DERIVED_DATA := build/DerivedData
CORE_BUILD_DIR := build/core
CORE_LIB := $(CORE_BUILD_DIR)/libPeripheralCore.a
CORE_TEST := $(CORE_BUILD_DIR)/test_core_smoke
CORE_HEADERS := $(wildcard Core/include/*.h)
CORE_C_SOURCES := $(wildcard Core/src/*.c)
CORE_OBJC_SOURCES := $(wildcard Core/src/*.m)
CORE_OBJECTS := $(patsubst Core/src/%.c,$(CORE_BUILD_DIR)/%.o,$(CORE_C_SOURCES)) $(patsubst Core/src/%.m,$(CORE_BUILD_DIR)/%.o,$(CORE_OBJC_SOURCES))
CORE_TEST_SOURCES := $(wildcard Core/tests/*.c)
CC := clang
CFLAGS := -std=c17 -Wall -Wextra -Werror -pedantic -I Core/include
OBJCFLAGS := -fobjc-arc -Wall -Wextra -Werror -I Core/include
CORE_LDFLAGS := -lsqlite3 -framework CoreAudio -framework CoreFoundation -framework CoreGraphics -framework IOKit -framework AVFoundation -framework Foundation

all: build-app test-core

build-app:
	xcodebuild -project $(PROJECT) -scheme $(SCHEME) -configuration Debug -derivedDataPath $(DERIVED_DATA) build

build-release:
	xcodebuild -project $(PROJECT) -scheme $(SCHEME) -configuration Release -derivedDataPath $(DERIVED_DATA) build

core-lib: $(CORE_LIB)

$(CORE_LIB): $(CORE_OBJECTS)
	mkdir -p $(CORE_BUILD_DIR)
	libtool -static -o $@ $^

$(CORE_BUILD_DIR)/%.o: Core/src/%.c $(CORE_HEADERS)
	mkdir -p $(CORE_BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(CORE_BUILD_DIR)/%.o: Core/src/%.m $(CORE_HEADERS)
	mkdir -p $(CORE_BUILD_DIR)
	$(CC) $(OBJCFLAGS) -c $< -o $@

test-core: $(CORE_LIB) $(CORE_TEST_SOURCES)
	$(CC) $(CFLAGS) $(CORE_TEST_SOURCES) $(CORE_LIB) $(CORE_LDFLAGS) -o $(CORE_TEST)
	$(CORE_TEST)

test-all: build-app test-core

clean:
	xcodebuild -project $(PROJECT) -scheme $(SCHEME) -derivedDataPath $(DERIVED_DATA) clean
	rm -rf $(CORE_BUILD_DIR)
