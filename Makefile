.POSIX:
# just in case FreeBSD's make implementation screws up the current directory
.OBJDIR: .
CC = gcc
NAME = lexre
PEG_MODULE = re
BIN_DIR = bin
INC_DIR = include
LIB_DIR = lib
SRC_DIR = src
TEST_DIR = test
STDC_DIALECT = -std=c99
GRAMMAR = $(NAME).grmr
BLD_LOG_LEVEL = LOG_LEVEL_WARN
TEST_LOG_LEVEL = LOG_LEVEL_TRACE
DEFAULT_LOG_LEVEL = LOG_LEVEL_WARN
CFLAGS_COMMON = -Wall -Werror -Wextra -pedantic -Wno-missing-field-initializers -Wno-unused -Wno-unused-parameter $(STDC_DIALECT) -g -DDEFAULT_LOG_LEVEL=$(DEFAULT_LOG_LEVEL) -DMAX_LOGGING_LEVEL=$(TEST_LOG_LEVEL) -fPIC
CFLAGS = $(CFLAGS_COMMON) -O2 
CFLAGS_DBG = $(CFLAGS_COMMON) -O0
IFLAGS = -I$(INC_DIR) -I$(LIB_DIR)
LFLAGS = '-Wl,-rpath,$$ORIGIN/.' '-Wl,-rpath,$$ORIGIN/$(BIN_DIR)' -L$(BIN_DIR) -lpeggy
LFLAGS_TEST = $(LFLAGS) -llexred

LIB_OBJS = $(SRC_DIR)/$(PEG_MODULE).o $(SRC_DIR)/reparser.o $(SRC_DIR)/nfa.o $(SRC_DIR)/thompson.o $(SRC_DIR)/dfa.o $(SRC_DIR)/lexre.o $(SRC_DIR)/fa.o
LIB_OBJS_DBG = $(SRC_DIR)/$(PEG_MODULE).do $(SRC_DIR)/reparser.do $(SRC_DIR)/nfa.do $(SRC_DIR)/thompson.do $(SRC_DIR)/dfa.do $(SRC_DIR)/lexre.do $(SRC_DIR)/fa.do
TEST_SRCS = $(TEST_DIR)/test.do $(TEST_DIR)/test_utils.do $(TEST_DIR)/test_re_utils.do

all: $(BIN_DIR)/lib$(NAME).so $(BIN_DIR)/lib$(NAME)d.so $(BIN_DIR)/test

.MAIN: all

$(INC_DIR)/$(PEG_MODULE).h: $(GRAMMAR)
	$(BIN_DIR)/peggy $(GRAMMAR) $(GRAMMAR).log $(BLD_LOG_LEVEL)
	mv $(PEG_MODULE).h $(INC_DIR)/$(PEG_MODULE).h
	mv $(PEG_MODULE).c $(SRC_DIR)/$(PEG_MODULE).c

$(BIN_DIR)/lib$(NAME).so: $(INC_DIR)/$(PEG_MODULE).h $(LIB_OBJS)
	$(CC) -shared -fPIC $(LIB_OBJS) -o $@ $(LFLAGS)

$(BIN_DIR)/lib$(NAME)d.so: $(INC_DIR)/$(PEG_MODULE).h $(LIB_OBJS_DBG)
	@if [ -n "$(SANITIZE)" ] ; then export DBGOPT="-fsanitize=address,undefined"; else export DBGOPT="" ; fi ; \
	$(CC) -shared -fPIC $(LIB_OBJS_DBG) -o $@ $(LFLAGS_TEST)

$(BIN_DIR)/test: $(BIN_DIR)/lib$(NAME)d.so $(TEST_SRCS)
	$(CC) $(CFLAGS_TEST) $$DBGOPT $(IFLAGS) $(TEST_SRCS) -o $@ $(LFLAGS_TEST)

check: $(BIN_DIR)/test
	$(BIN_DIR)/test --verbose

clean:
	rm -rf $(BIN_DIR)/lib$(NAME).so $(BIN_DIR)/lib$(NAME)d.so $(BIN_DIR)/test
	rm -f $(INC_DIR)/re.h $(SRC_DIR)/re.c *.log
	rm -f $(SRC_DIR)/*.o $(SRC_DIR)/*.do

.SUFFIXES: .c .o .do
# should build in a compile flag to use PCRE2 if USE_PCRE2 is set
.c.o:
	$(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

.c.do:
	@if [ -n "$(SANITIZE)" ] ; then export DBGOPT="-fsanitize=address,undefined"; else export DBGOPT="" ; fi ; \
	$(CC) $(CFLAGS_DBG) $$DBGOPT $(IFLAGS) -c $< -o $@
