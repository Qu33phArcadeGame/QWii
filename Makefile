# Qu33ph Wii — builds boot.dol with devkitPPC + libogc (the GitHub Action does this for you)
# All the game files sit next to this Makefile. (Lines start with ">" instead of tabs, so this
# file still works after being pasted into GitHub's editor.)
.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error DEVKITPPC is not set: install devkitPro's devkitPPC (wii-dev), or let the GitHub Action build it)
endif
include $(DEVKITPPC)/wii_rules
.RECIPEPREFIX = >

TARGET   := boot
BUILD    := build

CFLAGS   = -O2 -Wall -Wno-unused-function -fno-strict-aliasing -DQU_WII $(MACHDEP) $(INCLUDE)
LDFLAGS  = $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS     := -lfat -lasnd -lwiiuse -lbte -logc -lm

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT   := $(CURDIR)/$(TARGET)
export VPATH    := $(CURDIR)
export DEPSDIR  := $(CURDIR)/$(BUILD)
CFILES          := $(notdir $(wildcard *.c))
export LD       := $(CC)
export OFILES   := $(CFILES:.c=.o)
export INCLUDE  := -iquote $(CURDIR) -I$(CURDIR)/$(BUILD) -I$(LIBOGC_INC)
export LIBPATHS := -L$(LIBOGC_LIB)

.PHONY: $(BUILD) clean
$(BUILD):
> @[ -d $@ ] || mkdir -p $@
> @$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
> rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).dol $(OUTPUT).elf.map

else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
