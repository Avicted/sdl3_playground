CC = gcc
CXX = g++

CFLAGS = -Wall -Wextra -Werror -O0 -std=c99 -ggdb
CXXFLAGS = -Wall -Wextra -Werror -O0 -std=c++17 -ggdb -Wno-error=missing-field-initializers -Wno-missing-field-initializers

INCLUDES =  -I./submodules/SDL/include \
			-I./include \
			-I./include/stb \
			-I./submodules/glm \
			-I./submodules/box2d/include

LIBS = -L./submodules/SDL/build \
	   -L./submodules/glm/build/glm \
	   -L./submodules/box2d/build/src \
	   -lSDL3 -lglm -lbox2d -lm

SRC = src/main.cpp src/renderer.cpp

EXE = build/SDL_playground

# Build everything
all: SDL glm box2D compile_shaders $(EXE)

# Build the main executable
$(EXE): $(SRC)
	$(shell mkdir -p build)
	cp -r shaders build/
	cp -r resources build/
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(EXE) $(SRC) $(LIBS)

compile_shaders: 
	cd ./shaders/source && ./compile.sh && cd ../../

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

box2D:
	cd submodules/box2d && ./build.sh

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
	@echo "  glm:   Build the glm library"
	@echo "  box2D: Build the box2D library"
	@echo "  help:  Display this help message"

.PHONY: all clean run help SDL
