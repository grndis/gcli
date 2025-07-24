# Makefile for gcli, gcommit & gcmd - Gemini CLI tools
# Automatically detects the OS and builds both binaries.

# --- Common Configuration ---
CC = cc
GCLI_TARGET_NAME = gcli
GCOMMIT_TARGET_NAME = gcommit
GCMD_TARGET_NAME = gcmd

# Source files
GCLI_SRC_COMMON = gcli.c cJSON.c
GCOMMIT_SRC = gcommit.c
GCMD_SRC = gcmd.c

# Common compiler and linker flags
CFLAGS = -Wall -Wextra -g -O2 -I. -std=c99
LDFLAGS = 

# --- Platform-Specific Configuration ---

# Default to POSIX (Linux, macOS)
OS_TYPE = POSIX

# Check if we are on Windows
ifeq ($(OS),Windows_NT)
	OS_TYPE = WINDOWS
endif

# --- Windows Build ---
ifeq ($(OS_TYPE),WINDOWS)
	GCLI_TARGET = $(GCLI_TARGET_NAME).exe
	GCOMMIT_TARGET = $(GCOMMIT_TARGET_NAME).exe
	GCMD_TARGET = $(GCMD_TARGET_NAME).exe
	# On Windows, we compile linenoise.c directly into gcli
	GCLI_SRC = $(GCLI_SRC_COMMON) linenoise.c
	# On Windows, libcurl often needs the sockets and crypto libraries
	GCLI_LIBS = -lcurl -lz -lws2_32 -lbcrypt
	GCOMMIT_LIBS = 
	GCMD_LIBS = 
	RM = del /Q
	STRIP = strip

# --- POSIX Build ---
else
	GCLI_TARGET = $(GCLI_TARGET_NAME)
	GCOMMIT_TARGET = $(GCOMMIT_TARGET_NAME)
	GCMD_TARGET = $(GCMD_TARGET_NAME)
	# On POSIX, we don't compile linenoise.c into gcli
	GCLI_SRC = $(GCLI_SRC_COMMON)
	# On POSIX, we link against the installed readline library for gcli
	GCLI_LIBS = -lcurl -lz -lreadline
	GCOMMIT_LIBS = 
	GCMD_LIBS = 
	RM = rm -f
	STRIP = strip
endif

# Object files
GCLI_OBJ = $(GCLI_SRC:.c=.o)

# --- Build Rules ---

all: $(GCLI_TARGET) $(GCOMMIT_TARGET) $(GCMD_TARGET)

# Build gcli (main Gemini CLI)
$(GCLI_TARGET): $(GCLI_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(GCLI_LIBS)

# Build gcommit (commit message generator)
$(GCOMMIT_TARGET): $(GCOMMIT_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(GCOMMIT_LIBS)

# Build gcmd (shell command generator)
$(GCMD_TARGET): $(GCMD_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(GCMD_LIBS)

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Individual targets
build-gcli: $(GCLI_TARGET)
build-gcommit: $(GCOMMIT_TARGET)
build-gcmd: $(GCMD_TARGET)

# Release builds (with stripping)
release: clean $(GCLI_TARGET) $(GCOMMIT_TARGET) $(GCMD_TARGET)
	@echo "Creating release builds..."
	@if [ -f "$(GCLI_TARGET)" ]; then $(STRIP) -s "$(GCLI_TARGET)" 2>/dev/null || true; fi
	@if [ -f "$(GCOMMIT_TARGET)" ]; then $(STRIP) -s "$(GCOMMIT_TARGET)" 2>/dev/null || true; fi
	@if [ -f "$(GCMD_TARGET)" ]; then $(STRIP) -s "$(GCMD_TARGET)" 2>/dev/null || true; fi
	@echo "Release builds complete"

# Clean targets
clean:
ifeq ($(OS_TYPE),WINDOWS)
	$(RM) *.o *.exe
else
	$(RM) *.o $(GCLI_TARGET) $(GCOMMIT_TARGET) $(GCMD_TARGET)
endif

clean-gcli:
ifeq ($(OS_TYPE),WINDOWS)
	$(RM) *.o $(GCLI_TARGET_NAME).exe
else
	$(RM) *.o $(GCLI_TARGET)
endif

clean-gcommit:
ifeq ($(OS_TYPE),WINDOWS)
	$(RM) $(GCOMMIT_TARGET_NAME).exe
else
	$(RM) $(GCOMMIT_TARGET)
endif

clean-gcmd:
ifeq ($(OS_TYPE),WINDOWS)
	$(RM) $(GCMD_TARGET_NAME).exe
else
	$(RM) $(GCMD_TARGET)
endif

# Installation targets
install: $(GCLI_TARGET) $(GCOMMIT_TARGET) $(GCMD_TARGET)
	@echo "Installing gcli, gcommit, and gcmd..."
ifeq ($(OS_TYPE),WINDOWS)
	@echo "Manual installation required on Windows"
	@echo "Copy $(GCLI_TARGET), $(GCOMMIT_TARGET), and $(GCMD_TARGET) to a directory in your PATH"
else
	@if [ -w /usr/local/bin ]; then \
		cp $(GCLI_TARGET) /usr/local/bin/; \
		cp $(GCOMMIT_TARGET) /usr/local/bin/; \
		cp $(GCMD_TARGET) /usr/local/bin/; \
		echo "Installed to /usr/local/bin/"; \
	elif [ -d "$(HOME)/bin" ] || mkdir -p "$(HOME)/bin" 2>/dev/null; then \
		cp $(GCLI_TARGET) $(HOME)/bin/; \
		cp $(GCOMMIT_TARGET) $(HOME)/bin/; \
		cp $(GCMD_TARGET) $(HOME)/bin/; \
		echo "Installed to $(HOME)/bin/ - make sure it's in your PATH"; \
	else \
		echo "Cannot install automatically. Copy binaries to a directory in your PATH"; \
	fi
endif

uninstall:
	@echo "Uninstalling gcli, gcommit, and gcmd..."
ifeq ($(OS_TYPE),WINDOWS)
	@echo "Manual uninstallation required on Windows"
else
	@for target in $(GCLI_TARGET) $(GCOMMIT_TARGET) $(GCMD_TARGET); do \
		if [ -f /usr/local/bin/$$target ]; then \
			rm -f /usr/local/bin/$$target; \
			echo "Removed /usr/local/bin/$$target"; \
		elif [ -f $(HOME)/bin/$$target ]; then \
			rm -f $(HOME)/bin/$$target; \
			echo "Removed $(HOME)/bin/$$target"; \
		fi; \
	done
endif

# Test targets
test: $(GCLI_TARGET) $(GCOMMIT_TARGET) $(GCMD_TARGET)
	@echo "Testing gcli..."
	@./$(GCLI_TARGET) --help > /dev/null && echo "OK: gcli help works" || echo "FAIL: gcli help failed"
	@echo "Testing gcommit..."
	@./$(GCOMMIT_TARGET) --help > /dev/null && echo "OK: gcommit help works" || echo "FAIL: gcommit help failed"
	@echo "Testing gcmd..."
	@./$(GCMD_TARGET) --help > /dev/null && echo "OK: gcmd help works" || echo "FAIL: gcmd help failed"
	@if git rev-parse --git-dir > /dev/null 2>&1; then \
		if ! git diff --staged --quiet; then \
			echo "Testing gcommit with staged changes..."; \
			./$(GCOMMIT_TARGET) -g ./$(GCLI_TARGET) -f > /dev/null && echo "OK: gcommit generation works" || echo "FAIL: gcommit generation failed"; \
		else \
			echo "No staged changes to test gcommit with"; \
		fi; \
	else \
		echo "Not in git repository - cannot test gcommit"; \
	fi
	@echo "Testing gcmd command generation..."
	@./$(GCMD_TARGET) -g ./$(GCLI_TARGET) -f --dry-run "list files" > /dev/null && echo "OK: gcmd generation works" || echo "FAIL: gcmd generation failed"

# Help target
help:
	@echo "Available targets:"
	@echo "  all         - Build all three tools (default)"
	@echo "  build-gcli  - Build only gcli"
	@echo "  build-gcommit - Build only gcommit"
	@echo "  build-gcmd  - Build only gcmd"
	@echo "  release     - Build optimized release versions (with stripping)"
	@echo "  clean       - Remove all built files"
	@echo "  clean-gcli  - Remove only gcli build files"
	@echo "  clean-gcommit - Remove only gcommit build files"
	@echo "  clean-gcmd  - Remove only gcmd build files"
	@echo "  install     - Install all binaries to system PATH"
	@echo "  uninstall   - Remove all binaries from system"
	@echo "  test        - Test all binaries"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Built targets:"
	@echo "  gcli        - Main Gemini CLI client"
	@echo "  gcommit     - AI-powered git commit message generator"
	@echo "  gcmd        - Natural language to shell command generator"

.PHONY: all build-gcli build-gcommit build-gcmd release clean clean-gcli clean-gcommit clean-gcmd install uninstall test help
