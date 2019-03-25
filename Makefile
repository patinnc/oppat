
ifeq ($(OS),Windows_NT)
  $(info windows)
    #Windows stuff
  # The C program compiler.
  CC     = cl
  # The C++ program compiler.
  CXX    = cl
  # Un-comment the following line to compile C programs as C++ ones.
  #CC     = $(CXX)
  # The command used to delete file.
  RM     = rm -f
  ZLIB   = zlib-1.2.11/zlib.lib
  ZLIB_CMD = cmd.exe /c mk_zlib.bat
  LIBXLSWRITER   = libxlswriter/src/libxlswriter.lib
  LIBXLSWRITER_CMD = cmd.exe /c mk_xls.bat
  GCC_ARGS = /EHsc
  MY_LIBS   = 
  OBJ_EXT = obj
  EXE_EXT = exe
  CBASE = /Zi /Ox /nologo
  MK_DEPENDS_FILE = depends_win.mk
  OFILE = /Fo:
  OEXE = /Fe:
  PLAT_LDFLAGS =
  #OFILE = -o
else
  #Linux stuff
  $(info linux)
  # The C program compiler.
  CC     = gcc
  # The C++ program compiler.
  CXX    = g++
  # Un-comment the following line to compile C programs as C++ ones.
  #CC     = $(CXX)
  # The command used to delete file.
  RM     = rm -f
  LIBXLSWRITER   = libxlswriter/src/libxlsxwriter.a
  LIBXLSWRITER_CMD = $(MAKE) -C libxlswriter
  ZLIB   = zlib-1.2.11/libz.a
  #ZLIB_CMD = $(shell cd zlib-1.2.11 && make && cd ..)
  ZLIB_CMD = $(shell cd zlib-1.2.11; $(MAKE);)
  GCC_ARGS = -std=c++14 -Wno-write-strings
  MY_LIBS   = -lpthread -lz -ldl
  MY_LIBS   = -lpthread -ldl
  OBJ_EXT = o
  EXE_EXT = x
  CBASE = -g -O3
  MK_DEPENDS_FILE = depends_lnx.mk
  OFILE = -o
  OEXE = -o
  PLAT_LDFLAGS = -static
  #SUBDIRS := zlib-1.2.11

endif

# The pre-processor and compiler options.
MY_CFLAGS =

# The linker options.

# The pre-processor options used by the cpp (man cpp for more).
CPPFLAGS  = -Wall
CPPFLAGS  = 


# The options used in linking as well as in any direct use of ld.
LDFLAGS   = $(PLAT_LDFLAGS)

# The directories in which source files reside.
# If not specified, only the current directory will be serached.
SRCDIRS   = src src/lua

OBJ_DIR =obj
$(shell   mkdir -p $(OBJ_DIR)/lua)
BIN_DIR =bin
$(shell   mkdir -p $(BIN_DIR))

# The executable file name.
# If not specified, current directory name or `a.out' will be used.
PROGRAM   = bin/oppat.$(EXE_EXT)

## Implicit Section: change the following only when necessary.
##==========================================================================

# The source file types (headers excluded).
# .c indicates C source files, and others C++ ones.
SRCEXTS = .c .C .cc .cpp .CPP .c++ .cxx .cp

# The header file types.
HDREXTS = .h .H .hh .hpp .HPP .h++ .hxx .hp

# The pre-processor and compiler options.
# Users can override those variables from the command line.
CW_SSL = -DNO_SSL -DNO_SSL_DL
CW_SSL = -DNO_SSL 
CW_FLAGS = -DUSE_WEBSOCKET 
CW_FLAGS = -DUSE_WEBSOCKET $(CW_SSL)


CFLAGS  = $(CBASE) -Iinc -Ilibxlswriter/include -Izlib-1.2.11/contrib  -Izlib-1.2.11 -Isrc/lua ${CW_FLAGS}  -DWITH_LUA_VERSION=503 -DUSE_LUA=1 -DWITH_DUKTAPE=1 -DWITH_DUKTAPE_VERSION=108 -DWITH_WEBSOCKET=1 -DWITH_ZLIB=1 -DWITH_CPP=1
#CXXFLAGS= -Wno-strict-aliasing -g -O2 -Iinc -std=c++11 -g -Wno-write-strings -DNO_SSL -DWITH_LUA_VERSION=503 -DWITH_LUA=1 -DWITH_DUKTAPE=1 -DWITH_DUKTAPE_VERSION=108 -DWITH_WEBSOCKET=1 -DWITH_ZLIB=1 -DWITH_CPP=1
CXXFLAGS=  $(CBASE) -Iinc -Ilibxlswriter/include -Izlib-1.2.11 -Isrc/lua $(GCC_ARGS) $(CW_FLAGS) -DWITH_LUA_VERSION=503 -DUSE_LUA=1 -DWITH_DUKTAPE=1 -DWITH_DUKTAPE_VERSION=108 -DWITH_WEBSOCKET=1 -DWITH_ZLIB=1 -DWITH_CPP=1


ETAGS = etags
ETAGSFLAGS =

CTAGS = ctags
CTAGSFLAGS =

## Stable Section: usually no need to be changed. But you can add more.
##==========================================================================
SHELL   = /bin/sh
EMPTY   =
SPACE   = $(EMPTY) $(EMPTY)
ifeq ($(PROGRAM),)
  CUR_PATH_NAMES = $(subst /,$(SPACE),$(subst $(SPACE),_,$(CURDIR)))
  PROGRAM = $(word $(words $(CUR_PATH_NAMES)),$(CUR_PATH_NAMES))
  ifeq ($(PROGRAM),)
    PROGRAM = a.out
  endif
endif
ifeq ($(SRCDIRS),)
  SRCDIRS = .
endif
SOURCES = $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCEXTS))))
HEADERS = $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(HDREXTS))))
SRC_CC  = $(filter-out %.cpp,$(SOURCES))
SRC_CXX = $(filter-out %.c,$(SOURCES))
OBJS2    = $(addsuffix .$(OBJ_EXT), $(basename $(SOURCES)))
#OBJS    = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.${OBJ},$(CPPSRC_FILES))
#OBJS    = $(patsubst $(SRCDIRS)/%.o,$(OBJ_DIR)/%.o,$(OBJS2))
OBJS  = $(subst src/,obj/,$(OBJS2))
#$(info OBJS2= $(OBJS2))
#$(info OBJS= $(OBJS))
#OBJS    = $(OBJS2)
DEPS    = $(OBJS:.$(OBJ_EXT)=.d)

## Define some useful variables.
DEP_OPT = $(shell if `$(CC) --version | grep "GCC" >/dev/null`; then \
                  echo "-MM -MP"; else echo "-M"; fi )
DEPEND      = $(CC)  $(DEP_OPT) -std=c++11  $(MY_CFLAGS) $(CFLAGS) $(CPPFLAGS)
DEPEND.d    = $(subst -g ,,$(DEPEND))
COMPILE.c   = $(CC)  $(MY_CFLAGS) $(CFLAGS)   $(CPPFLAGS) -c
COMPILE.cxx = $(CXX) $(MY_CFLAGS) $(CXXFLAGS) $(CPPFLAGS) -c
LINK.c      = $(CC)  $(MY_CFLAGS) $(CFLAGS)   $(CPPFLAGS) $(LDFLAGS)
LINK.cxx    = $(CXX) $(MY_CFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS)

.PHONY: all objs tags ctags clean distclean help show

VPATH = $(SRCDIRS)

# Delete the default suffixes
.SUFFIXES:

all: $(PROGRAM)

# Rules for creating dependency files (.d).
#------------------------------------------

#$(OBJ_DIR)/%.d:%.c $(OBJ_DIR)/%.d
#	@echo -n $(dir $<) > $@
#	@$(DEPEND.d) $< >> $@
#
#$(OBJ_DIR)/%.d:%.C $(OBJ_DIR)/%.d
#	@echo -n $(dir $<) > $@
#	@$(DEPEND.d) $< >> $@
#
#%.d:%.cc
#	@echo -n $(dir $<) > $@
#	@$(DEPEND.d) $< >> $@
#
#$(OBJ_DIR)/%.d:%.cpp $(OBJ_DIR)/%.d
#	@echo -n $(dir $<) > $@
#	@$(DEPEND.d) $< >> $@
#
#%.d:%.CPP
#	@echo -n $(dir $<) > $@
#	@$(DEPEND.d) $< >> $@
#
#%.d:%.c++
#	@echo -n $(dir $<) > $@
#	@$(DEPEND.d) $< >> $@
#
#%.d:%.cp
#	@echo -n $(dir $<) > $@
#	@$(DEPEND.d) $< >> $@
#
#%.d:%.cxx
#	@echo -n $(dir $<) > $@
#	@$(DEPEND.d) $< >> $@
#
# Rules for generating object files (.o).
#----------------------------------------
objs:$(OBJS)

$(OBJ_DIR)/%.$(OBJ_EXT): %.c
	$(COMPILE.c) $< $(OFILE) $@

$(OBJ_DIR)/%.$(OBJ_EXT):%.C
	$(COMPILE.cxx) $< $(OFILE) $@

$(OBJ_DIR)/%.$(OBJ_EXT):%.cc
	$(COMPILE.cxx) $< $(OFILE) $@

$(OBJ_DIR)/%.$(OBJ_EXT): %.cpp
	$(COMPILE.cxx) $< $(OFILE) $@

#%.$(OBJ_EXT):%.CPP
#	$(COMPILE.cxx) $< $(OFILE) $@

%.$(OBJ_EXT):%.c++
	$(COMPILE.cxx) $< $(OFILE) $@

%.$(OBJ_EXT):%.cp
	$(COMPILE.cxx) $< $(OFILE) $@

%.$(OBJ_EXT):%.cxx
	$(COMPILE.cxx) $< $(OFILE) $@

# Rules for generating the tags.
#-------------------------------------
tags: $(HEADERS) $(SOURCES)
	$(ETAGS) $(ETAGSFLAGS) $(HEADERS) $(SOURCES)

ctags: $(HEADERS) $(SOURCES)
	$(CTAGS) $(CTAGSFLAGS) $(HEADERS) $(SOURCES)

# Rules for generating the executable.
#-------------------------------------
$(PROGRAM):$(OBJS) $(ZLIB) $(LIBXLSWRITER)
ifeq ($(SRC_CXX),)              # C program
	$(LINK.c)   $(OBJS) $(MY_LIBS) $(ZLIB) $(LIBXLSWRITER) $(OEXE) $@
	@echo Type ./$@ to execute the program.
else                            # C++ program
	$(LINK.cxx) $(OBJS) $(MY_LIBS) $(ZLIB) $(LIBXLSWRITER) $(OEXE) $@
	@echo Type ./$@ to execute the program.
endif

ifndef NODEP
ifneq ($(DEPS),)
  sinclude $(DEPS)
endif
endif

$(ZLIB):
	echo try to make zlib $(ZLIB) with cmd= $(ZLIB_CMD)
	$(ZLIB_CMD)

$(LIBXLSWRITER):
	@echo try to make libxlswriter $(LIBXLSWRITER) with cmd= $(LIBXLSWRITER_CMD)
	$(LIBXLSWRITER_CMD)

clean:
	$(RM) $(OBJS) $(PROGRAM) $(PROGRAM).exe

distclean: clean
	$(RM) $(DEPS) TAGS

# lnx run: sh ./mk_depends.sh
# win run: mk_depends.bat
# generates depends.mk file
sinclude $(MK_DEPENDS_FILE)

# Show help.
help:
	@echo 'Generic Makefile for C/C++ Programs (gcmakefile) version 0.5'
	@echo 'Copyright (C) 2007, 2008 whyglinux <whyglinux@hotmail.com>'
	@echo
	@echo 'Usage: make [TARGET]'
	@echo 'TARGETS:'
	@echo '  all       (=make) compile and link.'
	@echo '  NODEP=yes make without generating dependencies.'
	@echo '  objs      compile only (no linking).'
	@echo '  tags      create tags for Emacs editor.'
	@echo '  ctags     create ctags for VI editor.'
	@echo '  clean     clean objects and the executable file.'
	@echo '  distclean clean objects, the executable and dependencies.'
	@echo '  show      show variables (for debug use only).'
	@echo '  help      print this message.'
	@echo
	@echo 'Report bugs to <whyglinux AT gmail DOT com>.'

# Show variables (for debug use only.)
show:
	@echo 'PROGRAM     :' $(PROGRAM)
	@echo 'SRCDIRS     :' $(SRCDIRS)
	@echo 'HEADERS     :' $(HEADERS)
	@echo 'SOURCES     :' $(SOURCES)
	@echo 'SRC_CXX     :' $(SRC_CXX)
	@echo 'OBJS        :' $(OBJS)
	@echo 'DEPS        :' $(DEPS)
	@echo 'DEPEND      :' $(DEPEND)
	@echo 'COMPILE.c   :' $(COMPILE.c)
	@echo 'COMPILE.cxx :' $(COMPILE.cxx)
	@echo 'link.c      :' $(LINK.c)
	@echo 'link.cxx    :' $(LINK.cxx)

## End of the Makefile ##  Suggestions are welcome  ## All rights reserved ##
#############################################################################
