PRUSSDRV=/usr/lib
CFLAGS=-Wall -pedantic -g -Wno-long-long
CPPFLAGS=-I.
PASM=/usr/local/bin/pasm
OBJCOPY=objcopy
CC=gcc
LD=gcc

all: prutest usbsniff usbdump USBSniffer-00A0.dtbo pru1.fw pru0.fw

prutest: prutest.o pru0_prg.bin
	$(LD) $< -o $@ -L $(PRUSSDRV) -lprussdrv

usbsniff: usbsniff.o usb_ringbuffer.o crc5.o crc16.o usb_packet_decoder.o usb_logger.o
	$(LD) $^ -o $@

usbdump: usbdump.o usb_ringbuffer.o
	$(LD) $^ -o $@



%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< 

%.bin: %.p
	$(PASM) -V3 -I. -bdl $<

%.o: %.bin
	$(OBJCOPY) -O elf32-littlearm --rename-section .data=.text -I binary -B arm $< $@

USBSniffer-00A0.dtbo: USBSniffer.dts
	dtc -O dtb -o $@ -b 0 -@ $<

resource_table_*.o: resource_table_*.c
	$(CC) -c $<

pru1.fw: resource_table_1.o pru1_prg.o
	ld -T firmware.ld $^ -o $@

pru0.fw: resource_table_0.o pru0_prg.o
	ld -T firmware.ld $^ -o $@

clean:
	-rm *.o
	-rm prutest

install: install_fw install_overlay

install_fw: pru1.fw pru0.fw
	install pru1.fw /lib/firmware/am335x-pru1-fw
	install pru0.fw /lib/firmware/am335x-pru0-fw

install_overlay: USBSniffer-00A0.dtbo
	install  $^ /lib/firmware/

PRU0_ID = 4a334000.pru0
PRU1_ID = 4a338000.pru1

PRU_RPROC_DIR = /sys/bus/platform/drivers/pru-rproc

reload: reload_pru0 reload_pru1

reload_pru0:
	-echo $(PRU0_ID) > $(PRU_RPROC_DIR)/unbind
	echo $(PRU0_ID) > $(PRU_RPROC_DIR)/bind

reload_pru1:
	-echo $(PRU1_ID) > $(PRU_RPROC_DIR)/unbind
	echo $(PRU1_ID) > $(PRU_RPROC_DIR)/bind

.SUFFIXES:
