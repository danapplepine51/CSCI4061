# Compiler Information
CC = gcc
LFLAGS = $(CC) -Wall -pthread
CFLAGS = $(CC) -Wall -pthread -c
# Build Directory Information
SRCDIR = .
BUILDDIR = ../build
OBJDIR = $(BUILDDIR)/obj
BINDIR = $(BUILDDIR)/bin

# Files
SRCFILES = $(wildcard $(SRCDIR)/*.c)
OBJFILES = $(notdir $(pathsubst %.c, %.o, $(SRCFILES)))
EXEFILE = $(BINDIR)/votecounter
MAINFILE = $(SRCDIR)/main.c

all: $(EXEFILE)

#unused
$(addprefix $(OBJDIR)/, $(OBJFILES)): | $(OBJDIR)

#unused
$(OBJDIR) $(BINDIR):
	@mkdir -p $@

$(EXEFILE): $(MAINFILE)
	@mkdir -p $(BINDIR)
	$(LFLAGS) -o $@ $<

# unused
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CFLAGS) -o $@ $<

clean:
	@rm -rf $(OBJDIR)
	@rm -rf $(BINDIR)
