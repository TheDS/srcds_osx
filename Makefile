BINARY = srcds_osx

OBJECTS = main.cpp hacks.cpp mm_util.cpp CDetour/detours.cpp asm/asm.c cocoa_helpers.mm

CC = clang
CXX = clang++
CFLAGS = -pipe -fno-strict-aliasing -fvisibility=hidden -m32 -mmacosx-version-min=10.5 -Wall -Werror -Wno-deprecated-declarations
CXXFLAGS = -fvisibility-inlines-hidden
OPTFLAGS = -O3
DBGFLAGS = -g3
LDFLAGS = -m32 -mmacosx-version-min=10.5 -framework Foundation -framework AppKit

INCLUDE = -I.

ifeq "$(DEBUG)" "true"
	BIN_DIR = Debug.$(ENGINE)
	CFLAGS += $(DBGFLAGS)
else
	BIN_DIR = Release.$(ENGINE)
	CFLAGS += $(OPTFLAGS)
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
ifeq "$(ENGINE)" "dota"
	CFLAGS += -DENGINE_DOTA
endif

OBJ := $(OBJECTS:%.cpp=$(BIN_DIR)/%.o)
OBJ := $(OBJ:%.c=$(BIN_DIR)/%.o)
OBJ := $(OBJ:%.mm=$(BIN_DIR)/%.o)

$(BIN_DIR)/%.o: %.cpp
	$(CXX) $(INCLUDE) $(CFLAGS) $(CXXFLAGS) -o $@ -c $<

$(BIN_DIR)/%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) -o $@ -c $<

$(BIN_DIR)/%.o: %.mm
	$(CXX) $(INCLUDE) $(CFLAGS) $(CXXFLAGS) -m32 -o $@ -c $<

.PHONY: all check clean cleanup obv obv_sdl gmod l4d nd l4d2 csgo ins dota srcds_osx

all:
	$(MAKE) obv
	$(MAKE) obv_sdl
	$(MAKE) gmod
	$(MAKE) l4d
	$(MAKE) nd
	$(MAKE) l4d2
	$(MAKE) csgo
	$(MAKE) ins
	$(MAKE) dota

check:
	mkdir -p $(BIN_DIR)/asm
	mkdir -p $(BIN_DIR)/CDetour

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

dota:
	$(MAKE) srcds_osx ENGINE=dota

srcds_osx: check $(OBJ)
	$(CXX) $(OBJ) $(LDFLAGS) -o $(BIN_DIR)/$(BINARY)

debug:
	$(MAKE) all DEBUG=true

cleanup:
	rm -rf $(BIN_DIR)/*.o
	rm -rf $(BIN_DIR)/asm/*.o
	rm -rf $(BIN_DIR)/CDetour/*.o
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
	make cleanup ENGINE=dota

