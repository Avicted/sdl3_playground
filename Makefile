# Compiler
CC = gcc

export PATH := $(PATH):$(CURDIR)/submodules/SDL/build

# Flags
CFLAGS = -Wall -Wextra -Werror -O0 -std=c99 -ggdb

INCLUDES = -I./submodules/SDL/include

LIBS = -L./submodules/SDL/build -lSDL3

# Source files
SRC = src/main.c

# Executable
EXE = build/SDL_playground

# Build everything
all: SDL $(EXE)

# Build the main executable
$(EXE): $(SRC)
	$(shell mkdir -p build)
	$(CC) $(CFLAGS) $(INCLUDES) $(INC) -o $(EXE) $(SRC) $(LIBS)


SDL:
	cd submodules/SDL && \
	(mkdir build || true) && cd build && \
	cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .. && \
	cmake --build . --config Release --parallel

# sudo ninja install

# Clean up
clean:
	rm -f $(EXE)
	rm -rf build

# Run the program
run: $(EXE)
	LD_LIBRARY_PATH=$(CURDIR)/submodules/SDL/build:$$LD_LIBRARY_PATH $(EXE)

# Help message
help:
	@echo "Usage: make [all|clean|run|help]"
	@echo "  all:   Build the executable"
	@echo "  clean: Remove the executable"
	@echo "  run:   Run the executable"
	@echo "  help:  Display this help message"

.PHONY: all clean run help midiFile
