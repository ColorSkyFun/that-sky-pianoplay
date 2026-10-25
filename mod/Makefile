# Build with MinGW-w64 (the same toolchain HTModLoader uses).
#
# Usage:
#   make SDK_ROOT=/path/to/html-sdk
#
# The SDK zip should contain:
#   include/htmodloader/htmod.h
#   include/htmodloader/htmodloader.h
#   lib/libhtmodloader.a  (or htmodloader.lib)

CXX ?= g++
SDK_ROOT ?= ../../html-sdk

CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
CXXFLAGS += -I$(SDK_ROOT)/include

# The SDK ships an MSVC import library (htmodloader.lib); some builds use a
# MinGW archive instead.
HTLIB := $(firstword $(wildcard $(SDK_ROOT)/lib/htmodloader.lib $(SDK_ROOT)/lib/libhtmodloader.a))
LDFLAGS = $(HTLIB) -static -s -shared

TARGET = piano-rpc.dll

SRCS = \
    src/dllmain.cpp \
    src/Common.cpp \
    src/GameState.cpp \
    src/Instrument.cpp \
    src/Rpc.cpp \
    src/NotePlayer.cpp \
    src/Scheduler.cpp \
    src/LuaRuntime.cpp \
    src/Hooks.cpp

OBJS = $(patsubst src/%.cpp,build/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build $(TARGET)

.PHONY: all clean
