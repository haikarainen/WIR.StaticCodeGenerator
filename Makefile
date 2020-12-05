DEBUG 		?= 0
PREFIX		:= /usr
CXX		:= g++
CXXFLAGS	:= -std=c++2a -Wall -Wpedantic -Wno-unused-parameter -fPIC
LDFLAGS		:= -ldl -pthread 
REQLIBS		:= libwirframework glm
LIBS		:= $(shell pkg-config --libs $(REQLIBS))
DEPFLAGS	:= $(shell pkg-config --cflags $(REQLIBS))
LLVMINCLUDE 	:= $(shell llvm-config --includedir)
LLVMLIB		:= -L$(shell llvm-config --libdir) $(shell llvm-config --libs) -lclang
BUILDDIR	:= build
OUT_BINARY	:= wircodegen
SOURCEDIR	:= src
INCLUDEDIR	:= include

SOURCES 	:= $(shell find $(SOURCEDIR) -name '*.cpp')
OBJECTS 	:= $(addprefix $(BUILDDIR)/,$(SOURCES:%.cpp=%.o))

ifeq ($(DEBUG), 1)
	CXXFLAGS += -DWIR_DEBUG -g -O0
else
	CXXFLAGS += -O2 -g
endif


$(OUT_BINARY): $(OBJECTS)
	$(shell mkdir bin)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(LDFLAGS) $(LIBS) $(LLVMLIB) $(OBJECTS) -o bin/$(OUT_BINARY) -lstdc++fs

$(BUILDDIR)/%.o: %.cpp
	@echo 'Building ${notdir $@} ...'
	$(shell mkdir -p "${dir $@}")
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -I$(INCLUDEDIR) -I$(LLVMINCLUDE) -c $< -o $@

install:
	cp bin/$(OUT_BINARY) $(PREFIX)/bin/

uninstall:
	rm $(PREFIX)/lib/$(OUT_BINARY)

clean:
	$(shell rm -rf ./build)
	$(shell rm -rf ./bin)
	$(shell rm -f $(OBJECTS) lib/$(OUT_BINARY))
