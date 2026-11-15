CC ?= cc
CFLAGS ?= -Os -g
CPPFLAGS += -D_GNU_SOURCE=1 -D_FILE_OFFSET_BITS=64 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Iinclude -Isrc -Ivendor/cjson -Ivendor/qrcodegen -include include/fl_compat.h
WARNINGS := -Wall -Wextra -Wpedantic
APP ?= build/friends-library
CORE := $(filter-out src/main.c src/ui_sdl.c,$(wildcard src/*.c)) $(wildcard vendor/cjson/*.c vendor/qrcodegen/*.c)
ALL := $(wildcard src/*.c) $(wildcard vendor/cjson/*.c vendor/qrcodegen/*.c)
SDL_CFLAGS ?= $(shell sdl-config --cflags)
SDL_LIBS ?= $(shell sdl-config --libs)

.PHONY: all test clean
all: $(APP)

build:
	mkdir -p build

$(APP): $(ALL) $(wildcard include/*.h) Makefile | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(SDL_CFLAGS) $(ALL) -o $@ $(LDFLAGS) $(SDL_LIBS) -lpthread -lm

build/test-core: $(CORE) tests/test_core.c $(wildcard include/*.h) Makefile | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(CORE) tests/test_core.c -o $@ $(LDFLAGS) -lpthread -lm

build/test-backend: $(CORE) tests/test_backend.c $(wildcard include/*.h) Makefile | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(CORE) tests/test_backend.c -o $@ $(LDFLAGS) -lpthread -lm

build/test-ui: $(CORE) src/ui_sdl.c tests/test_ui.c $(wildcard include/*.h) Makefile | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(SDL_CFLAGS) $(CORE) src/ui_sdl.c tests/test_ui.c -o $@ $(LDFLAGS) $(SDL_LIBS) -lpthread -lm

build/test-download: $(CORE) tests/test_download.c $(wildcard include/*.h) Makefile | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(CORE) tests/test_download.c -o $@ $(LDFLAGS) -lpthread -lm

build/test-app: $(CORE) src/main.c src/ui_sdl.c tests/test_app.c $(wildcard include/*.h) Makefile | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(SDL_CFLAGS) $(CORE) src/ui_sdl.c tests/test_app.c -o $@ $(LDFLAGS) $(SDL_LIBS) -lpthread -lm

build/test-favorites: $(CORE) src/main.c src/ui_sdl.c tests/test_favorites.c $(wildcard include/*.h) Makefile | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(SDL_CFLAGS) $(CORE) src/ui_sdl.c tests/test_favorites.c -o $@ $(LDFLAGS) $(SDL_LIBS) -lpthread -lm

build/test-phone: $(CORE) tests/test_phone.c $(wildcard include/*.h) Makefile | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(CORE) tests/test_phone.c -o $@ $(LDFLAGS) -lpthread -lm

build/test-https: $(CORE) tests/test_https.c $(wildcard include/*.h) Makefile | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(CORE) tests/test_https.c -o $@ $(LDFLAGS) -lpthread -lm

test: build/test-core build/test-backend build/test-ui build/test-download build/test-app build/test-favorites build/test-phone build/test-https
	./build/test-core
	./build/test-backend "$(CURDIR)"
	SDL_VIDEODRIVER=dummy ./build/test-ui
	./build/test-download
	./build/test-app "$(CURDIR)"
	./build/test-favorites
	./build/test-phone "$(CURDIR)"
	./build/test-https "$(CURDIR)"

clean:
	rm -f build/friends-library build/friends-library-arm build/test-core build/test-backend build/test-ui build/test-download build/test-app build/test-favorites
