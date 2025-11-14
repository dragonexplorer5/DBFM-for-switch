#---------------------------------------------------------------------------------
# Makefile for DBFM
# Created by DragonExplorer5
#---------------------------------------------------------------------------------

DEVKITPRO    := $(DEVKITPRO)
LIBNX        := $(DEVKITPRO)/libnx
.SUFFIXES:
.DEFAULT_GOAL := all

include $(LIBNX)/switch_rules

#---------------------------------------------------------------------------------
# Environment setup
#---------------------------------------------------------------------------------
TOPDIR       := $(CURDIR)
DEVKITPRO    ?= C:/devkitPro
DEVKITARM    ?= $(DEVKITPRO)/devkitA64
LIBNX        ?= $(DEVKITPRO)/libnx
PATH         := $(DEVKITPRO)/tools/bin:$(DEVKITPRO)/devkitA64/bin:$(PATH)

#---------------------------------------------------------------------------------
# Options for code generation
#---------------------------------------------------------------------------------

# If user requests libcurl but the devkitPro portlibs don't contain curl headers,
# automatically disable USE_LIBCURL so the build continues. This keeps the
# default no-libcurl fallback active while avoiding a hard error when users do
# "make USE_LIBCURL=1" on systems without curl installed.
CURL_HEADER_PATH := $(DEVKITPRO)/portlibs/switch/include/curl/curl.h
ifeq ($(strip $(USE_LIBCURL)),1)
ifeq ($(wildcard $(CURL_HEADER_PATH)),)
    $(warning libcurl requested but curl/curl.h not found at $(CURL_HEADER_PATH); disabling USE_LIBCURL)
    USE_LIBCURL := 0
endif
endif


#---------------------------------------------------------------------------------
# DevKitPro Path Configuration
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
    $(error "Please set DEVKITPRO in your environment")
endif
export DEVKITPRO := $(subst \\,/,$(DEVKITPRO))

export DEVKITA64 := $(DEVKITPRO)/devkitA64
export LIBNX     := $(DEVKITPRO)/libnx
export PATH      := $(DEVKITPRO)/tools/bin:$(DEVKITA64)/bin:$(PATH)
export PORTLIBS  := $(DEVKITPRO)/portlibs/switch

#---------------------------------------------------------------------------------
# Configuration
#---------------------------------------------------------------------------------
TITLE       := DBFM
AUTHOR      := DragonExplorer5
VERSION     := 1.0.0

#---------------------------------------------------------------------------------
# Target and source setup
#---------------------------------------------------------------------------------
TARGET      := DBFM
BUILD       := build
SOURCES     := source \
               source/core \
               source/file \
               source/game \
               source/net \
               source/save \
               source/security \
               source/features \
               source/system \
               source/ui \
               source/util \
               source/applets \
               third_party \
               
INCLUDES    := include source source/security source/file source/core source/game source/net source/save source/system source/ui source/util source/features source/applets $(DEVKITPRO)/portlibs/switch/include
ROMFS       := romfs

#---------------------------------------------------------------------------------
# Files setup
#---------------------------------------------------------------------------------
CFILES      := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES    := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
SFILES      := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.s))
BINFILES    := $(foreach dir,$(DATA),$(wildcard $(dir)/*.*))

# If you don't want to build the browser (missing web applet wrappers), exclude it here
# This filters out the top-level source/browser.c from CFILES so the rest of the project can build.
CFILES      := $(filter-out source/browser.c,$(CFILES))

#---------------------------------------------------------------------------------
# Setup some vars
#---------------------------------------------------------------------------------
export APP_TITLE   := $(TITLE)
export APP_AUTHOR  := $(AUTHOR)
export APP_VERSION := $(VERSION)

#---------------------------------------------------------------------------------
# Options for code generation
#---------------------------------------------------------------------------------
ARCH        := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec

CFLAGS      := -g -Wall -O2 -ffunction-sections \
               $(ARCH) \
               $(BUILD_CFLAGS)

CFLAGS      += $(INCLUDE) -D__SWITCH__ -DVERSION=\"$(VERSION)\" \
               -I$(PORTLIBS)/include -L$(PORTLIBS)/lib

CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS     := -g $(ARCH)

LDFLAGS     := -specs=$(LIBNX)/switch.specs -g $(ARCH) \
               -Wl,-Map,$(notdir $*.map)

LIBS        := -lnx -lm

# Optional features: enable libcurl or jansson by passing USE_LIBCURL=1 or USE_JANSSON=1
ifeq ($(strip $(USE_LIBCURL)),1)
    CFLAGS += -DUSE_LIBCURL
    LIBS   += -lcurl
    $(info Building with libcurl support enabled)
endif

ifeq ($(strip $(USE_JANSSON)),1)
    CFLAGS += -DUSE_JANSSON
    LIBS   += -ljansson
    $(info Building with jansson support enabled)
endif

ifeq ($(strip $(USE_MBEDTLS)),1)
    CFLAGS += -DUSE_MBEDTLS
    # Link common mbedTLS libs; ensure your devkitPro/portlibs include mbedTLS for switch or provide paths
    LIBS   += -lmbedtls -lmbedx509 -lmbedcrypto
    $(info Building with mbedTLS support enabled)
endif

#---------------------------------------------------------------------------------
# List of directories containing libraries
#---------------------------------------------------------------------------------
LIBDIRS     := $(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
# No real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)
CFLAGS      += -include $(TOPDIR)/include/compat_libnx.h
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                  $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES          := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
## Exclude the top-level browser.c (we intentionally skip building the browser feature when its wrappers
## or headers are missing). This removes 'browser.c' from the notdir CFILES list.
CFILES          := $(filter-out browser.c,$(CFILES))
CPPFILES        := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES          := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

#---------------------------------------------------------------------------------
# Use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
    export LD   := $(CC)
else
    export LD   := $(CXX)
endif

export OFILES_BIN   := $(addsuffix .o,$(BINFILES))
export OFILES_SRC   := $(CPPFILES:%.cpp=%.o) $(CFILES:%.c=%.o) $(SFILES:%.s=%.o)
export OFILES       := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN   := $(addsuffix .h,$(subst .,_,$(BINFILES)))

#---------------------------------------------------------------------------------
# Rules for NRO file output
#---------------------------------------------------------------------------------
%.nro: %.elf
	@elf2nro $< $@ $(NROFLAGS)
	@echo Built ... $(notdir $@)

#---------------------------------------------------------------------------------
# Rules for ELF file output
#---------------------------------------------------------------------------------
%.elf: $(OFILES)
	@echo Linking $(notdir $@)...
	@$(LD) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@
	@$(NM) -CSn $@ > $(notdir $*.lst)

export INCLUDE      := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                      $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                      -I$(CURDIR)/$(BUILD) \
                      -I$(DEVKITPRO)/portlibs/switch/include

export LIBPATHS    := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).nro $(TARGET).elf $(TARGET).nacp $(TARGET).pfs0

#---------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS := $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# Main targets
#---------------------------------------------------------------------------------
all     : $(OUTPUT).nro

$(OUTPUT).nro   : $(OUTPUT).elf $(OUTPUT).nacp
	@elf2nro $< $@ $(NROFLAGS)
	@echo built ... $(notdir $@)

$(OUTPUT).elf   : $(OFILES)

$(OFILES_SRC)   : $(HFILES_BIN)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o %_bin.h : %.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
# Rules for building object files
#---------------------------------------------------------------------------------
%.o: %.cpp
	@echo $(notdir $<)
	@$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) -c $< -o $@ $(ERROR_FILTER)

%.o: %.c
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@ $(ERROR_FILTER)

%.o: %.s
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d -x assembler-with-cpp $(ASFLAGS) -c $< -o $@ $(ERROR_FILTER)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif