# This makefile has been modified to accept
# external environment variables

OS ?= LINUX
#OS = MACOSX
#OS = WINDOWS
#OS = WINDOWS64



ifeq ($(OS), LINUX)
#  sudo apt-get install libudev-dev
CC ?= gcc
INC += -ISDK/CHeaders/XPLM
#CFLAGS = -Wall -O0 -g -D$(OS) $(INC)
CFLAGS += -Wall -O2 -fPIC -D$(OS) $(INC) -fvisibility=hidden
LDFLAGS += -Wl,--version-script=linkscript.lnk
LDDYNFLAGS += -shared
LIBS += -ludev -lpthread -lrt
TARGET ?= lin.xpl

else ifeq ($(OS), MACOSX)
CC ?= gcc-4.2
SDK ?= /Developer/SDKs/MacOSX10.5.sdk
#ARCH = -arch i386 -mmacosx-version-min=10.5
ARCH ?= -arch i386 -arch x86_64 -mmacosx-version-min=10.5
INC += -ISDK/CHeaders/XPLM
CFLAGS += -Wall -O2 $(ARCH) -D$(OS) -isysroot $(SDK) $(INC) -fvisibility=hidden
LDFLAGS += $(ARCH) -isysroot $(SDK)
LDDYNFLAGS += -bundle -undefined dynamic_lookup -single_module
LIBS += -framework IOKit -framework CoreFoundation
TARGET ?= mac.xpl

else ifeq ($(OS), WINDOWS)
#  sudo apt-get install mingw32 mingw32-binutils mingw32-runtime
#  sudo apt-get install mono-devel wine
CC ?= i586-mingw32msvc-gcc
INC += -ISDK/CHeaders/XPLM
CFLAGS += -Wall -O2 -D$(OS) $(INC)
LDFLAGS += -Wl,--version-script=linkscript.lnk
LDDYNFLAGS += -shared
LIBS += SDK/Libraries/Win/XPLM.lib -lhid -lsetupapi -lwsock32
TARGET ?= win.xpl

else ifeq ($(OS), WINDOWS64)
#  sudo apt-get install  gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64
#  x86_64-w64-mingw32-gcc
CC ?= x86_64-w64-mingw32-gcc
INC += -ISDK/CHeaders/XPLM
CFLAGS += -Wall -O2 -D$(OS) $(INC)
LDFLAGS += -Wl,--version-script=linkscript.lnk
LDDYNFLAGS += -shared
LIBS += SDK/Libraries/Win/XPLM_64.lib -lhid -lsetupapi -lwsock32
TARGET ?= win.xpl
endif




all: linktest TeensyControls.xpl copy

OBJS = TeensyControls.o printf.o usb.o memory.o io.o display.o thread.o

TeensyControls.xpl: $(OBJS)
	$(CC) $(LDFLAGS) $(LDDYNFLAGS) $(OBJS) -o TeensyControls.xpl $(LIBS)
	cp TeensyControls.xpl $(TARGET)

linktest: linktest.o $(OBJS)
	$(CC) $(LDFLAGS) linktest.o $(OBJS) -o linktest $(LIBS)

copy:
ifeq ($(OS), WINDOWS)
	./cp_win32.sh TeensyControls.xpl
endif

%.o: %.c TeensyControls.h Makefile
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o TeensyControls.xpl linktest
