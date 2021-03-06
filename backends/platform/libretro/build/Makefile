##LIBRETRO
DEBUG=0

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
else ifneq ($(findstring win,$(shell uname -a)),)
   platform = win
endif
endif

ifeq ($(platform), unix)
   TARGET  := libretro.so
   DEFINES += -fPIC
   LDFLAGS += -shared -Wl,--version-script=../link.T -fPIC
else ifeq ($(platform), osx)
   TARGET  := libretro.dylib
   DEFINES += -fPIC
   LDFLAGS += -dynamiclib -fPIC
else ifeq ($(platform), ios)
   TARGET  := scummvm_libretro.dylib
   DEFINES += -fPIC -DCOTHREAD_PTHREAD
   LDFLAGS += -dynamiclib -fPIC
   CXX      = clang++ -arch armv7 -isysroot $(IOSSDK)
else ifeq ($(platform), qnx)
   TARGET  := libretro.so
   DEFINES += -fPIC -DCOTHREAD_PTHREAD -DSYSTEM_NOT_SUPPORTING_D_TYPE
   LDFLAGS += -shared -Wl,--version-script=../link.T -fPIC
   CC = qcc -Vgcc_ntoarmv7le
   CXX = QCC -Vgcc_ntoarmv7le
   LD = QCC -Vgcc_ntoarmv7le
   AR = qcc -Vgcc_ntoarmv7le -A
   RANLIB="${QNX_HOST}/usr/bin/ntoarmv7-ranlib"
else ifeq ($(platform), android)
   TARGET  := libretro_scummvm.so
   DEFINES += -fPIC -Wno-multichar
   LDFLAGS += -shared -Wl,--version-script=../link.T -fPIC
   TOOLSET = arm-linux-androideabi-
endif

ifeq ($(DEBUG), 1)
   DEFINES += -O0 -g
else
   DEFINES += -O3
endif

###SCUMM VM
srcdir = ../../../..
retrodir = backends/platform/libretro/
VPATH = $(srcdir)

# Define toolset
ifdef TOOLSET
    CC        = $(TOOLSET)gcc
    CXX       = $(TOOLSET)g++
    LD        = $(TOOLSET)g++
    AR        = $(TOOLSET)ar cru
    RANLIB    = $(TOOLSET)ranlib
else
ifneq ($(platform), qnx)
    LD        = $(CXX)
    AR        = ar cru
    RANLIB    = ranlib
endif
endif

MKDIR         = mkdir -p
RM            = rm -f
RM_REC        = rm -rf

# Define build flags
DEFINES       += -D__LIBRETRO__ -DNONSTANDARD_PORT -DUSE_RGB_COLOR -DUSE_OSD -DDISABLE_COMMAND_LINE -DDISABLE_TEXT_CONSOLE -DFRONTEND_SUPPORTS_RGB565
INCLUDES      += -I$(srcdir)/$(retrodir) -I. -I$(srcdir) -I$(srcdir)/engines
EXECUTABLE    = $(TARGET)
DEPDIR        = .deps
HAVE_GCC3     = true
USE_RGB_COLOR = true

# Define enabled modules
ENABLED=STATIC_PLUGIN
ENABLE_SCUMM = $(ENABLED)
ENABLE_SCUMM_7_8 = $(ENABLED)
ENABLE_SKY = $(ENABLED)
ENABLE_AGOS = $(ENABLED)
ENABLE_AGI = $(ENABLED)
ENABLE_SCI = $(ENABLED)
ENABLE_MOHAWK = $(ENABLED)
ENABLE_MYST = $(ENABLED)
ENABLE_TINSEL = $(ENABLED)

#
OBJS :=	$(retrodir)libretro.o \
        $(retrodir)os.o \
        $(retrodir)libco/libco.o \
        backends/fs/posix/posix-fs.o \
        backends/fs/posix/posix-fs-factory.o

MODULE_DIRS += $(retrodir) $(retrodir)libco

BACKEND := libretro

include $(srcdir)/Makefile.common
	
