CC = clang++
CFLAGS = --config ../.clang -O2
INC = -iquote ../

APP0 = snn
SRC0 = snn.cc
OBJ0 = $(SRC0:.cc=.o)
LIB0 =

# Suffixes (how to build objects).
# First line deletes all previously specified suffixes.
.SUFFIXES:
.SUFFIXES: .cc .o
.cc.o:
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

all: $(APP0)

$(APP0): ${OBJ0}
	$(CC) $(CFLAGS) -o $(APP0) $(OBJ0) $(LIB0)

clean:
	rm -f $(APP0) $(OBJ0)

.PHONY: all clean
