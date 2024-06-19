BUILD_DIR := bin
OBJ_DIR := obj

# Should be set by caller
# ASSEMBLY := BTImage

DEFINES := -DBT_EXPORT
ADDL_INCLUDE_FLAGS += -I./BTcommon

ifeq($(OS),Windows_NT)
# Windows
	BUILD_PLATFORM := Windows
	EXTENSION := .dll
	COMPILER_FLAGS := -Wall -Wextra -Werror -Wvla -Wgnu-folding-constant -Wno-missing-braces -fdeclspec -Wstrict-prototypes -Wno-unused-parameter -Wno-missing-field-initializers
	INCLUDE_FLAGS := -I$(ASSEMBLY)\src $(ADDL_INCLUDE_FLAGS)
	LINKER_FLAGS := -shared -L$(OBJ_DIR)\$(ASSEMBLY) -L.\$(BUILD_DIR) $(ADDL_LINKER_FLAGS)
	DEFINES += -D_CRT_SECURE_NO_WARNINGS

# Recursive wildcard function for Windows
	rwildcard = $(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
	DIR := $(substr /,\,${CURDIR})

# .c files
	SRC_FILES := $(call rwildcard, $(ASSEMBLY)/, *.c)
# Directories with .h files
	DIRS := \$(ASSEMBLY)\src $(subst $(DIR),,$(shell dir $(ASSEMBLY)\src /S /AD /B | findstr /i src))
	OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o)
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
# Linux
		BUILD_PLATFORM := Linux
		EXTENSION := .so
		COMPILER_FLAGS := -fvisibility=hidden -fpic -Wall -Werror -Wvla -Wno-missing-braces -fdeclspec
		INCLUDE_FLAGS := -I./$(ASSEMBLY)/src -I$(VULKAN_SDK)/include $(ADDL_INCLUDE_FLAGS)
		LINKER_FLAGS := -Wl, --no-undefined, --no-allow-shlib-undefined -shared -lvulkan -lxcb -lX11 -lXrandr -lX11-xcb -lxkbcommon -lm -L$(VULKAN_SDK)/lib -L/usr/X11R6/lib -L./$(BUILD_DIR) $(ADDL_LINKER_FLAGS)
		
# .c files
		SRC_FILES := $(shell find $(ASSEMBLY) -name *.c)
# Directories with .h files
		DIRS := $(shell find $(ASSEMBLY) -type d)
		OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o)
	endif
	ifeq ($(UNAME_S),Darwin)
# OSX
		BUILD_PLATFORM := MacOS
		EXTENSION := .dylib
		COMPILER_FLAGS := -fvisibility=hidden -fpic -Wall -Werror -Wvla -Wgnu-folding-constant -Wno-missing-braces -fdeclspec -ObjC
		INCLUDE_FLAGS := -I./$(ASSEMBLY)/src $(ADDL_INCLUDE_FLAGS)
		LINKER_FLAGS := -Wl, -undefined,error -shared -dynamiclib -install_name @rpath/lib$(ASSEMBLY).dylib -lobjc -framework AppKit -framework QuartzCore -L./$(BUILD_DIR) $(ADDL_LINKER_FLAGS)
# .c and .m files
		SRC_FILES := $(shell find $(ASSEMBLY) type -f \( -name "*.c" -o -name "*.m" \))
# Directories with .h files
		DIRS := $(shell find $(ASSEMBLY) -type d)
		OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o)
	endif
endif

# Determine if debug or release build - default to debug
ifeq ($(TARGET),release)
	DEFINES += -DBT_RELEASE
	COMPILER_FLAGS += -MD -O2
else
	DEFINES += -DBT_DEBUG
	COMPILER_FLAGS += -g -MD -O0
	LINKER_FLAGS += -g
endif


.PHONY: all scaffold compile link clean
all: scaffold compile link

scaffold: # Create build directory
ifeq($(BUILD_PLATFORM),Windows)
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(addprefix $(OBJ_DIR), $(DIRS)) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(BUILD_DIR) 2>NUL || cd .
else
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(addprefix $(OBJ_DIR)/, $(DIRS))
endif

link: scaffold $(OBJ_FILES) # Link object files into shared library
	@echo "Linking $(ASSEMBLY)..."
ifeq($(BUILD_PLATFORM),Windows)
	@$(CC) $(OBJ_FILES) -o $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION) $(LINKER_FLAGS) -Xlinker /INCREMENTAL
else
	@$(CC) $(OBJ_FILES) -o $(BUILD_DIR)/lib$(ASSEMBLY)$(EXTENSION) $(LINKER_FLAGS)
endif

compile:
	@echo "Doing compilation for $(ASSEMBLY) $(TARGET) build..."
-include $(OBJ_FILES:.o=.d)

clean:
	@echo "Cleaning $(ASSEMBLY)..."
ifeq($(BUILD_PLATFORM),Windows)
	@if exist $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION) del $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION)
# Remove other Windows files
	@if exist $(BUILD_DIR)\$(ASSEMBLY).* del $(BUILD_DIR)\$(ASSEMBLY).*
	@if exist $(OBJ_DIR)\$(ASSEMBLY) rmdir /s /q $(OBJ_DIR)\$(ASSEMBLY)
else
	@rm -rf $(BUILD_DIR)/lib$(ASSEMBLY)$(EXTENSION)
	@rm -rf $(OBJ_DIR)/$(ASSEMBLY)
endif

# Compile .c to .o object files
$(OBJ_DIR)/%.c.o: %.c
	@echo "Compiling $<..."
	@clang $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)

# Compile .m to .o object files (OSX only)
ifeq ($(BUILD_PLATFORM),MacOS)
$(OBJ_DIR)/%.m.o: %.m
	@echo "Compiling $<..."
	@clang $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)
endif

-include $(OBJ_FILES:.o=.d)