NAME = dwmstatus
VERSION = 1.1

# Customize below to fit your system

DEBUGFLAGS=
#DEBUGFLAGS=-g

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# includes and libs
INCS = -I. -I/usr/include
LIBS = -L/usr/lib -lc -lX11 -lm

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\"
CFLAGS = ${DEBUGFLAGS} -std=c99 -pedantic -Wall -O0 ${INCS} ${CPPFLAGS}
LDFLAGS = ${DEBUGFLAGS} ${LIBS}

# compiler and linker
CC = cc


# Conditional stuff

ifdef NO_PULSE
	CFLAGS  += -DNO_PULSE=1
else
	LIBS += -lpulse
endif

ifdef NO_MPD
	CFLAGS  += -DNO_MPD=1
else
	LIBS += -lmpdclient
endif

ifdef NO_WIRELESS
	CFLAGS  += -DNO_WIRELESS=1
else
	LIBS += -liw
endif
