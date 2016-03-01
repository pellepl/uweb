BINARY = linux_userver_test

############
#
# Paths
#
############

sourcedir = ./src/
builddir = build


#############
#
# Build tools
#
#############

CC = gcc $(COMPILEROPTIONS)
LD = ld
GDB = gdb
OBJCOPY = objcopy
OBJDUMP = objdump
MKDIR = mkdir -p

###############
#
# Files and libs
#
###############

RUN_SERVER ?= 0
CFLAGS = $(FLAGS)
ifeq (1, $(strip $(RUN_SERVER)))
CFILES_TEST = main.c uweb_sockserv.c
CFLAGS += -DRUN_SERVER
else
CFILES_TEST = main.c \
	test_uweb.c \
	testsuites.c \
	testrunner.c
endif

CFILES = uweb.c uweb_codec.c

INCLUDE_DIRECTIVES = -I./${sourcedir} -I./${sourcedir}/test  -I./${sourcedir}/default 
COMPILEROPTIONS = $(INCLUDE_DIRECTIVES)

COMPILEROPTIONS_APP = \
-Wall -Wno-format-y2k -W -Wstrict-prototypes -Wmissing-prototypes \
-Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch \
-Wshadow -Wcast-align -Wchar-subscripts -Winline -Wnested-externs\
-Wredundant-decls
		
############
#
# Tasks
#
############

vpath %.c ${sourcedir} ${sourcedir}/test

OBJFILES = $(CFILES:%.c=${builddir}/%.o)
OBJFILES_TEST = $(CFILES_TEST:%.c=${builddir}/%.o)

DEPFILES = $(CFILES:%.c=${builddir}/%.d) $(CFILES_TEST:%.c=${builddir}/%.d)

ALLOBJFILES += $(OBJFILES) $(OBJFILES_TEST)

DEPENDENCIES = $(DEPFILES) 

# link object files, create binary
$(BINARY): $(ALLOBJFILES)
	@echo "... linking"
	@${CC} $(LINKEROPTIONS) -o ${builddir}/$(BINARY) $(ALLOBJFILES) $(LIBS)
ifeq (1, $(strip $(RUN_SERVER)))
	@echo "size: `du -b ${builddir}/${BINARY} | sed 's/\([0-9]*\).*/\1/g '` bytes"
endif


-include $(DEPENDENCIES)	   	

# compile c files
$(OBJFILES) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@${CC} $(COMPILEROPTIONS_APP) $(CFLAGS) -g -c -o $@ $<

$(OBJFILES_TEST) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@${CC} $(CFLAGS) -g -c -o $@ $<

# make dependencies
#		@echo "... depend $@"; 
$(DEPFILES) : ${builddir}/%.d:%.c
		@rm -f $@; \
		${CC} $(COMPILEROPTIONS) -M $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*, ${builddir}/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

all: mkdirs $(BINARY) 

mkdirs:
	-@${MKDIR} ${builddir}
	-@${MKDIR} test_data

FILTER ?=

test: $(BINARY)
ifdef $(FILTER)
		./build/$(BINARY)
else
		./build/$(BINARY) -f $(FILTER)
endif

runserver: $(BINARY)
		./build/$(BINARY)

test_failed: $(BINARY)
		./build/$(BINARY) _tests_fail
	
clean:
	@echo ... removing build files in ${builddir}
	@rm -f ${builddir}/*.o
	@rm -f ${builddir}/*.d
	@rm -f ${builddir}/*.elf
	
server:
	$(MAKE) clean && $(MAKE) all RUN_SERVER=1 && $(MAKE) runserver RUN_SERVER=1
	
