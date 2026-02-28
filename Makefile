# =============================================================================
# RIFTLang Makefile - Cross-Platform (Windows MinGW / Linux)
# =============================================================================

PROJECT_NAME    := riftlang
VERSION         := 1.0.0
BUILD_DATE      := 2026-02-28

SRC_DIR         := .
INC_DIR         := .
OBJ_DIR         := build
BIN_DIR         := bin

# Source files (in current directory)
SOURCES         := riftlang.c main.c
HEADERS         := riftlang.h

OBJECTS         := $(OBJ_DIR)/riftlang.o $(OBJ_DIR)/main.o

# -----------------------------------------------------------------------------
# Platform Detection
# -----------------------------------------------------------------------------

ifeq ($(OS),Windows_NT)
    # Windows (MinGW)
    PLATFORM    := windows
    EXE_EXT     := .exe
    DLL_EXT     := .dll
    STATIC_EXT  := .a
    
    CC          := gcc
    MKDIR       := mkdir
    RM          := del /Q
    RMDIR       := rmdir /S /Q
    
    # MinGW flags - NO pthread, NO regex library (built-in)
    CFLAGS      := -std=c11 -Wall -Wextra -Wpedantic -I$(INC_DIR)
    CFLAGS      += -D_WIN32 -DWIN32_LEAN_AND_MEAN 
    CFLAGS      += -DVERSION=\"$(VERSION)\" -DBUILD_DATE=\"$(BUILD_DATE)\"
    CFLAGS      += -O2 -DNDEBUG
    
    LDFLAGS     := 
    LIBS        := -lm
    
    # Use Windows native threads
    CFLAGS      += -mthreads
    
    DLLFLAGS    := -shared
    
else
    # Linux / macOS
    PLATFORM    := linux
    EXE_EXT     :=
    DLL_EXT     := .so
    STATIC_EXT  := .a
    
    CC          := gcc
    MKDIR       := mkdir -p
    RM          := rm -f
    RMDIR       := rm -rf
    
    CFLAGS      := -std=c11 -Wall -Wextra -Wpedantic -I$(INC_DIR)
    CFLAGS      += -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
    CFLAGS      += -DVERSION=\"$(VERSION)\" -DBUILD_DATE=\"$(BUILD_DATE)\"
    CFLAGS      += -O2 -DNDEBUG
    
    LDFLAGS     := 
    LIBS        := -lpthread -lm -lrt
    
    DLLFLAGS    := -shared -fPIC
endif

# Debug build override
ifeq ($(DEBUG),1)
    CFLAGS      := $(filter-out -O2 -DNDEBUG,$(CFLAGS)) -g -O0 -DDEBUG
endif

# -----------------------------------------------------------------------------
# Target Names
# -----------------------------------------------------------------------------

TARGET_EXE      := $(BIN_DIR)/$(PROJECT_NAME)$(EXE_EXT)
TARGET_DLL      := $(BIN_DIR)/lib$(PROJECT_NAME)$(DLL_EXT)
TARGET_STATIC   := $(BIN_DIR)/lib$(PROJECT_NAME)$(STATIC_EXT)

# -----------------------------------------------------------------------------
# Build Rules
# -----------------------------------------------------------------------------

.PHONY: all clean exe dll static

all: exe dll static

# Create directories (Windows-compatible)
$(OBJ_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist $(OBJ_DIR) $(MKDIR) $(OBJ_DIR)
else
	$(MKDIR) $(OBJ_DIR)
endif

$(BIN_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist $(BIN_DIR) $(MKDIR) $(BIN_DIR)
else
	$(MKDIR) $(BIN_DIR)
endif

# Compile riftlang.c
$(OBJ_DIR)/riftlang.o: riftlang.c riftlang.h | $(OBJ_DIR)
	@echo CC riftlang.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile main.c - CRITICAL: Define RIFTLANG_OPEN_MAIN
$(OBJ_DIR)/main.o: main.c riftlang.h | $(OBJ_DIR)
	@echo CC main.c
	$(CC) $(CFLAGS) -DRIFTLANG_OPEN_MAIN=1 -c $< -o $@

# Build executable (includes main.o)
exe: $(TARGET_EXE)

$(TARGET_EXE): $(OBJECTS) | $(BIN_DIR)
	@echo LINK $@
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

# Build shared library (excludes main.o)
dll: $(TARGET_DLL)

$(TARGET_DLL): $(OBJ_DIR)/riftlang.o | $(BIN_DIR)
	@echo DLL $@
	$(CC) $(DLLFLAGS) -o $@ $< $(LIBS)

# Build static library (excludes main.o)
static: $(TARGET_STATIC)

$(TARGET_STATIC): $(OBJ_DIR)/riftlang.o | $(BIN_DIR)
	@echo AR $@
	$(AR) rcs $@ $<

# Clean build artifacts
clean:
ifeq ($(OS),Windows_NT)
	@echo CLEAN Windows build artifacts
	@if exist $(OBJ_DIR)\*.o $(RM) $(OBJ_DIR)\*.o
	@if exist $(BIN_DIR)\*.exe $(RM) $(BIN_DIR)\*.exe
	@if exist $(BIN_DIR)\*.dll $(RM) $(BIN_DIR)\*.dll
	@if exist $(BIN_DIR)\*.a $(RM) $(BIN_DIR)\*.a
else
	@echo CLEAN Linux build artifacts
	$(RM) $(OBJ_DIR)/*.o
	$(RM) $(BIN_DIR)/*
endif

# Debug print
print-%:
	@echo $* = $($*)