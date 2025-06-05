CC = clang++
CFLAGS = --config ../.clang -O2
INC = -iquote ../
LINK = -L/usr/local/lib/

.ifdef .MAKE.JOBS
CFLAGS += -fcolor-diagnostics
.endif

APP0 = snn
SRC0 = snn.cc
OBJ0 = $(SRC0:.cc=.o)
LIB0 =

# Suffixes (how to build object files).
# First line deletes all previously specified suffixes.
.SUFFIXES:
.SUFFIXES: .cc .o
.cc.o:
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

all: $(APP0)

$(APP0): ${OBJ0}
	$(CC) $(CFLAGS) -o $(APP0) $(OBJ0) $(LINK) $(LIB0)

clean-executables:
	rm -f $(APP0)

clean-object-files:
	rm -f $(OBJ0)

clean: clean-object-files clean-executables

run: all
	./$(APP0)

.PHONY: all clean-executables clean-object-files clean run
