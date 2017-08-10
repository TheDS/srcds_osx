BINARY = srcds_osx

OBJECTS = main.cpp hacks.cpp mm_util.cpp CDetour/detours.cpp asm/asm.c cocoa_helpers.mm GameLibPosix.cpp HSGameLib.cpp \
	  libudis86/decode.c libudis86/itab.c libudis86/syn-att.c libudis86/syn-intel.c libudis86/syn.c libudis86/udis86.c

CC = clang
CXX = clang++
CFLAGS = -pipe -fno-strict-aliasing -fvisibility=hidden -Wall -Werror -Wno-deprecated-declarations \
	-Wno-sometimes-uninitialized -Iamtl -DHAVE_STRING_H
OBJCFLAGS = 
CXXFLAGS = -fvisibility-inlines-hidden -std=c++14
OPTFLAGS = -O3
DBGFLAGS = -g3
LDFLAGS = -framework Foundation -framework AppKit

INCLUDE = -I.

ifeq "$(DEBUG)" "true"
	BIN_DIR = Debug.$(ENGINE)
	CFLAGS += $(DBGFLAGS)
else
	BIN_DIR = Release.$(ENGINE)
	CFLAGS += $(OPTFLAGS)
endif

# CS:GO is 64-bit, all other games are 32-bit
ifeq "$(ENGINE)" "csgo"
	CFLAGS += -m64
	LDFLAGS += -m64
else
	CFLAGS += -m32
	LDFLAGS += -m32
endif

# These games require 10.7
ifneq (,$(filter csgo ins doi,$(ENGINE)))
	CFLAGS += -mmacosx-version-min=10.7
	LDFLAGS += -mmacosx-version-min=10.7
endif

ifeq "$(ENGINE)" "obv"
	CFLAGS += -DENGINE_OBV
endif
ifeq "$(ENGINE)" "obv_sdl"
	CFLAGS += -DENGINE_OBV_SDL
endif
ifeq "$(ENGINE)" "gmod"
	CFLAGS += -DENGINE_GMOD
endif
ifeq "$(ENGINE)" "l4d"
	CFLAGS += -DENGINE_L4D
endif
ifeq "$(ENGINE)" "nd"
	CFLAGS += -DENGINE_ND
endif
ifeq "$(ENGINE)" "l4d2"
	CFLAGS += -DENGINE_L4D2
endif
ifeq "$(ENGINE)" "csgo"
	CFLAGS += -DENGINE_CSGO
endif
ifeq "$(ENGINE)" "ins"
	CFLAGS += -DENGINE_INS
endif
ifeq "$(ENGINE)" "doi"
	CFLAGS += -DENGINE_DOI
endif

OBJ := $(OBJECTS:%.cpp=$(BIN_DIR)/%.o)
OBJ := $(OBJ:%.c=$(BIN_DIR)/%.o)
OBJ := $(OBJ:%.mm=$(BIN_DIR)/%.o)

$(BIN_DIR)/%.o: %.cpp
	$(CXX) $(INCLUDE) $(CFLAGS) $(CXXFLAGS) -o $@ -c $<

$(BIN_DIR)/%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) -o $@ -c $<

$(BIN_DIR)/%.o: %.mm
	$(CXX) $(INCLUDE) $(CFLAGS) $(CXXFLAGS) -o $@ -c $<

.PHONY: all check clean cleanup obv obv_sdl gmod l4d nd l4d2 csgo ins doi srcds_osx

all:
	$(MAKE) obv
	$(MAKE) obv_sdl
	$(MAKE) gmod
	$(MAKE) l4d
	$(MAKE) nd
	$(MAKE) l4d2
	$(MAKE) csgo
	$(MAKE) ins
	$(MAKE) doi

check:
	mkdir -p $(BIN_DIR)/asm
	mkdir -p $(BIN_DIR)/CDetour
	mkdir -p $(BIN_DIR)/libudis86

obv:
	$(MAKE) srcds_osx ENGINE=obv

obv_sdl:
	$(MAKE) srcds_osx ENGINE=obv_sdl

gmod:
	$(MAKE) srcds_osx ENGINE=gmod

l4d:
	$(MAKE) srcds_osx ENGINE=l4d

nd:
	$(MAKE) srcds_osx ENGINE=nd

l4d2:
	$(MAKE) srcds_osx ENGINE=l4d2

csgo:
	$(MAKE) srcds_osx ENGINE=csgo

ins:
	$(MAKE) srcds_osx ENGINE=ins

doi:
	$(MAKE) srcds_osx ENGINE=doi

srcds_osx: check $(OBJ)
	$(CXX) $(OBJ) $(LDFLAGS) -o $(BIN_DIR)/$(BINARY)

debug:
	$(MAKE) all DEBUG=true

cleanup:
	rm -rf $(BIN_DIR)/*.o
	rm -rf $(BIN_DIR)/asm/*.o
	rm -rf $(BIN_DIR)/CDetour/*.o
	rm -rf $(BIN_DIR)/libudis86/*.o
	rm -rf $(BIN_DIR)/$(BINARY)

clean:
	make cleanup ENGINE=obv
	make cleanup ENGINE=obv_sdl
	make cleanup ENGINE=gmod
	make cleanup ENGINE=l4d
	make cleanup ENGINE=nd
	make cleanup ENGINE=l4d2
	make cleanup ENGINE=csgo
	make cleanup ENGINE=ins
	make cleanup ENGINE=doi
