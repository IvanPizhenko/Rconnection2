# Target definition
TARGET:=libRconnection2.a
SOURCES:=Rconnection2.cpp sisocks.c

OBJECTS:=$(patsubst %.c,%.o,$(SOURCES))
OBJECTS+=$(patsubst %.cpp,%.o,$(SOURCES))

# Toolchain options
CC=gcc
CXX:=g++
AR:=ar
DEFS:=-Dunix -DNO_CONFIG_H -DHAVE_NETINET_TCP_H  -DHAVE_NETINET_IN_H 
CFLAGS:=-m64 -pthread -std=c99 -Wall -Werror -Wextra -Wpedantic -fmax-errors=3
CXXFLAGS:=-m64 -pthread -std=c++11 -Wall -Werror -Wextra -Wpedantic -fmax-errors=3
ARFLAGS:=

ifeq ("$(DEBUG)", "1")
CFLAGS+=-Og -g3 -ggdb  -DDEBUG -D_DEBUG
CXXFLAGS+=-Og -g3 -ggdb  -DDEBUG -D_DEBUG
else
CFLAGS+=-O2 -flto -DNDEBUG
CXXFLAGS+=-O2 -flto -DNDEBUG
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
	$(AR) rvs $(ARFLAGS) $@ $(OBJECTS)

%.o: %.c
	$(CC) -o $@ $(CFLAGS) $(DEFS) -c $<

%.o: %.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(DEFS) -c $<
