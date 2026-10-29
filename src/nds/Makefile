# Nintendo DS / DSi build (devkitARM + libnds + dswifi + libfat).
# Based on the devkitPro arm9 template; shares ../core with the 3DS build.
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

TARGET   := dscord
BUILD    := build
SOURCES  := . ../core
INCLUDES := ../core
DATA     :=

GAME_TITLE     := DScord
GAME_SUBTITLE1 := Discord via a relay
GAME_SUBTITLE2 := github.com/Ramanathan0310

ARCH := -march=armv5te -mtune=arm946e-s -mthumb

CFLAGS   := -g -Wall -O2 -ffunction-sections -fdata-sections \
            $(ARCH) $(INCLUDE) -DARM9 -DPLATFORM_NDS
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS  := -g $(ARCH)
LDFLAGS   = -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS    := -lfat -ldswifi9 -lnds9
LIBDIRS := $(LIBNDS) $(PORTLIBS)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export LD           := $(CC)
export OFILES_BIN   := $(addsuffix .o,$(BINFILES))
export OFILES       := $(OFILES_BIN) $(CFILES:.c=.o)
export HFILES       := $(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE      := $(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
                       $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS     := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifneq (,$(wildcard icon.bmp))
export GAME_ICON := $(CURDIR)/icon.bmp
endif

.PHONY: $(BUILD) clean

$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).nds: $(OUTPUT).elf $(GAME_ICON)
$(OUTPUT).elf: $(OFILES)

%.bin.o %_bin.h: %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

endif
