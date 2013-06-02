BINARY = srcds_osx

OBJECTS = main.cpp hacks.cpp mm_util.cpp CDetour/detours.cpp asm/asm.c cocoa_helpers.mm

CC = clang
CXX = clang++
CFLAGS = -pipe -fno-strict-aliasing -fvisibility=hidden -m32 -Wall -Werror -Wno-deprecated-declarations
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
ifeq "$(ENGINE)" "gmod"
	CFLAGS += -DENGINE_GMOD
endif
ifeq "$(ENGINE)" "l4d"
	CFLAGS += -DENGINE_L4D
endif
ifeq "$(ENGINE)" "l4d2"
	CFLAGS += -DENGINE_L4D2
endif
ifeq "$(ENGINE)" "l4d2b"
	CFLAGS += -DENGINE_L4D2BETA
endif
ifeq "$(ENGINE)" "csgo"
	CFLAGS += -DENGINE_CSGO
endif
ifeq "$(ENGINE)" "ins"
	CFLAGS += -DENGINE_INS
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

.PHONY: all check clean cleanup obv gmod l4d l4d2 l4d2b csgo ins srcds_osx

all:
	$(MAKE) obv
	$(MAKE) gmod
	$(MAKE) l4d
	$(MAKE) l4d2
	$(MAKE) l4d2b
	$(MAKE) csgo
	$(MAKE) ins

check:
	mkdir -p $(BIN_DIR)/asm
	mkdir -p $(BIN_DIR)/CDetour

obv:
	$(MAKE) srcds_osx ENGINE=obv

gmod:
	$(MAKE) srcds_osx ENGINE=gmod

l4d:
	$(MAKE) srcds_osx ENGINE=l4d

l4d2:
	$(MAKE) srcds_osx ENGINE=l4d2

l4d2b:
	$(MAKE) srcds_osx ENGINE=l4d2b

csgo:
	$(MAKE) srcds_osx ENGINE=csgo

ins:
	$(MAKE) srcds_osx ENGINE=ins

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
	make cleanup ENGINE=gmod
	make cleanup ENGINE=l4d
	make cleanup ENGINE=l4d2
	make cleanup ENGINE=l4d2b
	make cleanup ENGINE=csgo
	make cleanup ENGINE=ins

