include ../../../../makefile.inc

# TARGETS
TARGET = onu_hal_mock

# build destination directories
OUTPUTDIR = ../../../../output/$(MACHINE)/$(TARGET)
OBJDIR = $(OUTPUTDIR)

# directories
# source directories
SRCDIR = .
INCDIR_PRIV = ../include_priv ../../include_priv
INCDIRS = $(INCDIR_PRIV)  $(if $(STAGINGDIR), $(STAGINGDIR)/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include)
STAGING_LIBDIR = $(if $(STAGINGDIR), -L$(STAGINGDIR)/lib) $(if $(STAGINGDIR), -L$(STAGINGDIR)/usr/lib)

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))

# compilation and linking flags
CFLAGS += -Werror -Wall -Wextra \
          -Wformat=2 -Wshadow \
          -Wwrite-strings -Wredundant-decls \
          -Wpedantic -Wmissing-declarations -Wno-attributes \
          -Wno-format-nonliteral \
          -g3 $(addprefix -I ,$(INCDIRS))

ifeq ($(CC_NAME),g++)
CFLAGS += -std=c++2a
else
CFLAGS += -Wstrict-prototypes -Wold-style-definition -Wnested-externs -std=c11
endif

LDFLAGS += $(STAGING_LIBDIR) -lubox -lubus

# targets
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

-include $(OBJECTS:.o=.d)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/:
	$(MKDIR) -p $@

clean:
	rm -rf $(OUTPUTDIR) $(TARGET)

.PHONY: all clean
