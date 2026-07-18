.PHONY: all build-app build-release test-core test-all clean

PROJECT := MacPeripheralHub.xcodeproj
SCHEME := MacPeripheralHub
DERIVED_DATA := build/DerivedData

all: build-app test-core

build-app:
	xcodebuild -project $(PROJECT) -scheme $(SCHEME) -configuration Debug -derivedDataPath $(DERIVED_DATA) build

build-release:
	xcodebuild -project $(PROJECT) -scheme $(SCHEME) -configuration Release -derivedDataPath $(DERIVED_DATA) build

test-core:
	@printf "Core test target is ready; C tests will be added in roadmap item 2.\\n"

test-all: build-app test-core

clean:
	xcodebuild -project $(PROJECT) -scheme $(SCHEME) -derivedDataPath $(DERIVED_DATA) clean
