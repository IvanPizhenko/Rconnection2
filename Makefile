# Target definition
TARGET:=libRconnection2.a

C_SOURCES:=sisocks.c
CXX_SOURCES:=Rconnection2.cpp

OBJECTS:=$(patsubst %.c,%.o,$(C_SOURCES))
OBJECTS+=$(patsubst %.cpp,%.o,$(CXX_SOURCES))

# Toolchain options
CC=gcc
CXX:=g++
AR:=ar
DEFS:=-Dunix -DNO_CONFIG_H -DHAVE_NETINET_TCP_H  -DHAVE_NETINET_IN_H 
CFLAGS:=-m64 -pthread -g3 -ggdb -std=c99 -Wall -Werror -Wextra -Wpedantic -fmax-errors=3
CXXFLAGS:=-m64 -pthread -g3 -ggdb -std=c++11 -Wall -Werror -Wextra -Wpedantic -fmax-errors=3
ARFLAGS:=

ifeq ("$(DEBUG)", "1")
CFLAGS+=-Og -DDEBUG -D_DEBUG
CXXFLAGS+=-Og -DDEBUG -D_DEBUG
else
CFLAGS+=-O2 -DNDEBUG
CXXFLAGS+=-O2 -DNDEBUG
ifeq ("$(LTO)", "1")
CFLAGS+=-flto
CXXFLAGS+=-flto
endif
endif

ifeq ("$(WEFFCXX)", "1")
CXXFLAGS+=-Weffc++ -Wno-error=effc++
endif


# Target building riles

.SUFFIXES:

.PHONY: all build clean rebuild

all: build

build: $(TARGET)

clean:
	echo Cleaning up $(TARGET)...
	-rm -f $(TARGET)
	-rm -f *.o

rebuild:
	echo Rebuilding $(TARGET)...
	$(MAKE) clean
	$(MAKE) build

	
$(TARGET): $(OBJECTS)
	echo Creating library $@...
	echo $(OBJECTS)
	$(AR) rvs $(ARFLAGS) $@ $(OBJECTS)

%.o: %.c
	$(CC) -o $@ $(CFLAGS) $(DEFS) -c $<

%.o: %.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(DEFS) -c $<
