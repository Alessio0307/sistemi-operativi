# === Variabili principali ===
CC = gcc
CFLAGS = -Wall -g -std=c11 -fPIC
LIBS = -lallegro -lallegro_primitives -lallegro_font -lallegro_ttf -lpthread -lm

# Directory
SRCDIR = src
INCDIR = include
LIBDIR = lib
OBJDIR = obj

# Files
LIB_SOURCES = $(SRCDIR)/bouncing_balls.c $(SRCDIR)/time0.c
LIB_OBJECTS = $(LIB_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
LIB_NAME = libbouncing_balls.so
STATIC_LIB = libbouncing_balls.a

# Main program
MAIN_SOURCE = examples/main.c
MAIN_OBJECT = $(OBJDIR)/main.o
EXECUTABLE = pallina

# Default target
all: directories $(LIBDIR)/$(LIB_NAME) $(LIBDIR)/$(STATIC_LIB) $(EXECUTABLE)

# Create directories
directories:
	@mkdir -p $(OBJDIR) $(LIBDIR) examples

# Compile library source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

# Create shared library
$(LIBDIR)/$(LIB_NAME): $(LIB_OBJECTS)
	$(CC) -shared -o $@ $^ $(LIBS)

# Create static library
$(LIBDIR)/$(STATIC_LIB): $(LIB_OBJECTS)
	ar rcs $@ $^

# Compile main program
$(MAIN_OBJECT): $(MAIN_SOURCE)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

# Link main program with shared library
$(EXECUTABLE): $(MAIN_OBJECT) $(LIBDIR)/$(LIB_NAME)
	$(CC) -o $@ $< -L$(LIBDIR) -lbouncing_balls $(LIBS)

# Install library (optional)
install: all
	sudo cp $(LIBDIR)/$(LIB_NAME) /usr/local/lib/
	sudo cp $(LIBDIR)/$(STATIC_LIB) /usr/local/lib/
	sudo cp $(INCDIR)/*.h /usr/local/include/
	sudo ldconfig

# Uninstall library
uninstall:
	sudo rm -f /usr/local/lib/$(LIB_NAME)
	sudo rm -f /usr/local/lib/$(STATIC_LIB)
	sudo rm -f /usr/local/include/bouncing_balls.h
	sudo rm -f /usr/local/include/time0.h
	sudo ldconfig

# Test with shared library
test: $(EXECUTABLE)
	LD_LIBRARY_PATH=./$(LIBDIR) ./$(EXECUTABLE)

# Clean build files
clean:
	rm -rf $(OBJDIR) $(LIBDIR) $(EXECUTABLE)

# Clean everything including directories
distclean: clean
	rm -rf examples

.PHONY: all directories install uninstall test clean distclean
