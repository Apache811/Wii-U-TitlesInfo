# Wii U Title Info
# Build intermediates and output artifacts are generated in build/.

.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "DEVKITPRO is not set. Set DEVKITPRO to your devkitPro installation.")
endif
ifeq ($(strip $(DEVKITPPC)),)
$(error "DEVKITPPC is not set. Set DEVKITPPC to your devkitPro installation.")
endif

TOPDIR ?= $(CURDIR)

#-------------------------------------------------------------------------------
# APP_NAME sets the long name of the application
# APP_SHORTNAME sets the short name of the application
# APP_AUTHOR sets the author of the application
#-------------------------------------------------------------------------------
APP_NAME       := Wii U Titles Info
APP_SHORTNAME  := TitlesInfo
APP_AUTHOR     := Apache81 + ChatGPT

include $(DEVKITPRO)/wut/share/wut_rules

#-------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA contains files embedded into the executable.
# INCLUDES is a list of directories containing header files
#-------------------------------------------------------------------------------
TARGET   := TitlesInfo
BUILD    := build
SOURCES  := source
DATA     := fonts resources
INCLUDES := include source

LDFLAGS  = -g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)

# Use the rebuilt Mocha library installed as the default in WUT.
LIBS     := -lSDL2_image -lwebp -ljpeg -lSDL2_ttf -lfreetype -lharfbuzz -lpng -lbz2 -lbrotlidec -lbrotlicommon -lSDL2 -lmocha -lz -lwut
LIBDIRS  := $(PORTLIBS) $(WUT_ROOT) $(WUT_ROOT)/usr

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(BUILD)/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD) -I$(DEVKITPRO)/portlibs/wiiu/include/SDL2
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

# Define compiler flags after INCLUDE so the include paths are expanded correctly.
export CFLAGS   := -g -Wall -Wextra -O2 -ffunction-sections $(MACHDEP)
export CFLAGS   += $(INCLUDE) -D__WIIU__ -D__WUT__
export CXXFLAGS := $(CFLAGS) -std=gnu++20 -fno-exceptions -fno-rtti
export ASFLAGS  := -g $(ARCH)

#-------------------------------------------------------------------------------
# wuhb packaging
#-------------------------------------------------------------------------------
WUHB_ICON		:=	meta/iconTex.tga
WUHB_TV_SPLASH	:=	meta/bootTvTex.tga
WUHB_DRC_SPLASH	:=	meta/bootDrcTex.tga

export APP_ICON			:=	$(TOPDIR)/$(WUHB_ICON)
export APP_TV_SPLASH	:=	$(TOPDIR)/$(WUHB_TV_SPLASH)
export APP_DRC_SPLASH	:=	$(TOPDIR)/$(WUHB_DRC_SPLASH)

.PHONY: all clean $(BUILD)

all: $(BUILD)

$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@mkdir -p $(BUILD)/release
	@cp $(BUILD)/$(TARGET).wuhb $(BUILD)/release/$(TARGET).wuhb
	@cp resources/title_database.json $(BUILD)/release/title_database.json
	@echo "Release artifacts copied to $(BUILD)/release/"

# Keep clean behavior explicit: remove the generated build directory and its contents.
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD)

else

DEPENDS := $(OFILES:.o=.d)

.PHONY: all
all: $(OUTPUT).wuhb

$(OUTPUT).wuhb: $(OUTPUT).rpx
$(OUTPUT).rpx: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

$(OFILES_SRC): $(HFILES_BIN)

%.ttf.o %_ttf.h: %.ttf
	@echo "Embedding $(notdir $<)"
	@$(bin2o)

%.png.o %_png.h: %.png
	@echo "Embedding $(notdir $<)"
	@$(bin2o)

%.json.o %_json.h: %.json
	@echo "Embedding $(notdir $<)"
	@$(bin2o)

-include $(DEPENDS)

endif
