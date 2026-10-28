# Qu33ph Wii — builds boot.dol with devkitPPC + libogc (the GitHub Action does this for you)
.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error DEVKITPPC is not set: install devkitPro's devkitPPC (wii-dev), or let the GitHub Action build it)
endif
include $(DEVKITPPC)/wii_rules

TARGET   := boot
BUILD    := build
SOURCES  := source
INCLUDES := source

CFLAGS   = -O2 -Wall -Wno-unused-function -fno-strict-aliasing -DQU_WII $(MACHDEP) $(INCLUDE)
LDFLAGS  = $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS     := -lfat -lasnd -lwiiuse -lbte -logc -lm
LIBDIRS  :=

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT  := $(CURDIR)/$(TARGET)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
CFILES         := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export LD      := $(CC)
export OFILES  := $(CFILES:.c=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD) -I$(LIBOGC_INC)
export LIBPATHS := -L$(LIBOGC_LIB) $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).dol $(OUTPUT).elf.map

else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
