GCCFLAGS=-g -Os -Wall -mcall-prologues -mmcu=attiny24a -DF_CPU=8000000
LINKFLAGS=-Wl,-Map,${PROGNAME}.map
AVRDUDEFLAGS=-c usbtiny -p attiny24

LFUSE = 0xe2
HFUSE = 0xdf
EFUSE = 0xff
LOCK  = 0xff

PROGNAME=IOExpander

all:	${PROGNAME}.hex

program: ${PROGNAME}-upload

fuse:
	avrdude ${AVRDUDEFLAGS} -u -U hfuse:w:$(HFUSE):m -U lfuse:w:$(LFUSE):m \
    -U efuse:w:$(EFUSE):m -U lock:w:$(LOCK):m

${PROGNAME}.hex: ${PROGNAME}.obj
	avr-objcopy -j .text -j .data -O ihex ${PROGNAME}.o ${PROGNAME}.hex

${PROGNAME}.obj: ${PROGNAME}.c
	avr-gcc ${GCCFLAGS} ${LINKFLAGS} -o ${PROGNAME}.o ${PROGNAME}.c
   
${PROGNAME}-upload:	${PROGNAME}.hex
	avrdude ${AVRDUDEFLAGS} -U flash:w:${PROGNAME}.hex:i

