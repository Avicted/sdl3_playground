CC = gcc
CXX = g++

CFLAGS = -Wall -Wextra -Werror -O0 -std=c99 -ggdb
CXXFLAGS = -Wall -Wextra -Werror -O0 -std=c++17 -ggdb -Wno-error=missing-field-initializers -Wno-missing-field-initializers

INCLUDES = -I./submodules/SDL/include -I./include -I/include/glad -I./submodules/glm

LIBS = -L./submodules/SDL/build -L./submodules/glm/build/glm -lSDL3 -lglm -lm

SRC = src/main.cpp src/gl.c

EXE = build/SDL_playground

# Build everything
all: SDL glm $(EXE)

# Build the main executable
$(EXE): $(SRC)
	cp -r shaders build/
	cp -r resources build/
	$(shell mkdir -p build)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(EXE) $(SRC) $(LIBS)

SDL:
	cd submodules/SDL && \
	(mkdir build || true) && cd build && \
	cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .. && \
	cmake --build . --config Release --parallel

glm:
	cd submodules/glm && \
	cmake \
		-DGLM_BUILD_TESTS=OFF \
		-DBUILD_SHARED_LIBS=ON \
		-B build . && \
	cmake --build build -- all

clean:
	rm -f $(EXE)
	rm -rf build

run: $(EXE)
	LD_LIBRARY_PATH=$(CURDIR)/submodules/SDL/build:$(CURDIR)/submodules/glm/build/glm:$$LD_LIBRARY_PATH $(EXE)

help:
	@echo "Usage: make [all|clean|run|help]"
	@echo "  all:   Build the executable"
	@echo "  clean: Remove the executable"
	@echo "  run:   Run the executable"
	@echo "  SDL:   Build the SDL library"
	@echo "  help:  Display this help message"

.PHONY: all clean run help SDL
