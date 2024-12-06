CC = gcc

CFLAGS = -Wall -Wextra -Werror -O0 -std=c99 -ggdb

INCLUDES = -I./submodules/SDL/include -I./include -I/include/glad

LIBS = -L./submodules/SDL/build -lSDL3

SRC = src/main.c src/gl.c

EXE = build/SDL_playground

# Build everything
all: SDL $(EXE)

# Build the main executable
$(EXE): $(SRC)
	cp -r shaders build/
	$(shell mkdir -p build)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(EXE) $(SRC) $(LIBS)


SDL:
	cd submodules/SDL && \
	(mkdir build || true) && cd build && \
	cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .. && \
	cmake --build . --config Release --parallel


clean:
	rm -f $(EXE)
	rm -rf build

run: $(EXE)
	LD_LIBRARY_PATH=$(CURDIR)/submodules/SDL/build:$$LD_LIBRARY_PATH $(EXE)

help:
	@echo "Usage: make [all|clean|run|help]"
	@echo "  all:   Build the executable"
	@echo "  clean: Remove the executable"
	@echo "  run:   Run the executable"
	@echo "  SDL:   Build the SDL library"
	@echo "  help:  Display this help message"

.PHONY: all clean run help SDL
