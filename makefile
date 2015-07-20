# Define source files
CPP_SRCS = ping.cpp demo.cpp 

###CPP_SRCS = 

BIN = demo

DYNAMIC_LIB = 

STATIC_LIB = 

INSTALL_LIB_PATH = 

CXXFLAGS  = -Wall -g -fPIC -ggdb3 -Wno-deprecated -D__DEBUG

# Define header file paths
INCPATH = -I../

CXX = g++

# Define the -L library path(s)
LDFLAGS = 
##LDFLAGS = 

# Define the -l library name(s)
LIBS = -lpthread -lcrypto -lssl

LIB_FLAGS = $(CXXFLAGS) $(LDFLAGS) $(LIBS) -shared -Wl,-soname,$(DYNAMIC_LIB)

# Only in special cases should anything be edited below this line
OBJS      = $(CPP_SRCS:.cpp=.o)

.PHONY = all clean distclean

# Main entry point
#

AR       = ar
ARFLAGS  = -ruv
RANLIB	 = ranlib

all:  $(BIN) $(STATIC_LIB) $(DYNAMIC_LIB)

# For linking object file(s) to produce the executable

$(BIN): $(OBJS)
	@echo Linking $@
	$(CXX) $^ $(LDFLAGS) $(LIBS)  $(CXXFLAGS) $(INCPATH) -o $@

${STATIC_LIB}: ${OBJS}
	@if [ ! -d ${INSTALL_LIB_PATH} ]; then mkdir -p ${INSTALL_LIB_PATH}; fi
	$(AR) ${ARFLAGS} $@ $(OBJS)
	$(RANLIB) $@

${DYNAMIC_LIB} : ${OBJS}
	@if [ ! -d ${INSTALL_LIB_PATH} ]; then mkdir -p ${INSTALL_LIB_PATH}; fi
	$(CXX) $(LIB_FLAGS) -o $(DYNAMIC_LIB) $(OBJS)

# For compiling source file(s)
#
.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) $(LDFLAGS)  $<

install:
	@if [ "x${STATIC_LIB}" != "x" ]; then mkdir -p ${INSTALL_LIB_PATH}; cp $(STATIC_LIB) $(INSTALL_LIB_PATH); fi
	@if [ "x${DYNAMIC_LIB}" != "x" ]; then mkdir -p ${INSTALL_LIB_PATH}; cp $(DYNAMIC_LIB) $(INSTALL_LIB_PATH); fi
# For cleaning up the project
#
clean:
	$(RM) $(OBJS) core.* $(BIN)

distclean: clean
	$(RM) $(BIN) 


