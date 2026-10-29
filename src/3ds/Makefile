# Nintendo 3DS build (devkitARM + libctru). Based on the devkitPro 3DS
# template; shares ../core with the DS build and uses ../../third_party.
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET   := discord-ds
BUILD    := build
SOURCES  := . ../core
INCLUDES := ../core ../../third_party
DATA     :=

APP_TITLE       := discord-ds
APP_DESCRIPTION := Discord via a relay
APP_AUTHOR      := discord-ds

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS   := -g -Wall -O2 -mword-relocations -ffunction-sections \
            $(ARCH) $(INCLUDE) -D__3DS__ -DPLATFORM_3DS
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS  := -g $(ARCH)
LDFLAGS   = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS    := -lctru -lm
LIBDIRS := $(CTRULIB)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export LD         := $(CC)
export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES     := $(OFILES_BIN) $(CFILES:.c=.o)
export HFILES     := $(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE    := $(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
                     $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS   := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifneq (,$(wildcard icon.png))
export APP_ICON := $(TOPDIR)/icon.png
else
export APP_ICON := $(CTRULIB)/default_icon.png
endif
export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh

.PHONY: $(BUILD) clean

$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).3dsx: $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf: $(OFILES)

%.bin.o %_bin.h: %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

endif
