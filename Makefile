PRUSSDRV=/usr/lib
CFLAGS=-Wall -pedantic -g
PASM=/usr/local/bin/pasm
OBJCOPY=objcopy

all: prutest usbsniff USBSniffer-00A0.dtbo pru1.fw pru0.fw

prutest: prutest.o pru0_prg.bin
	gcc $< -o $@ -L $(PRUSSDRV) -lprussdrv

usbsniff: usbsniff.o
	gcc $< -o $@

%.o: %.c
	gcc $(CFLAGS) -c $< 

%.bin: %.p
	$(PASM) -V3 -I. -bdl $<

%.o: %.bin
	$(OBJCOPY) -O elf32-littlearm --rename-section .data=.text -I binary -B arm $< $@

USBSniffer-00A0.dtbo: USBSniffer.dts
	dtc -O dtb -o $@ -b 0 -@ $<

resource_table_*.o: resource_table_*.c
	gcc -c $<

pru1.fw: resource_table_1.o pru1_prg.o
	ld -T firmware.ld $^ -o $@

pru0.fw: resource_table_0.o pru0_prg.o
	ld -T firmware.ld $^ -o $@

clean:
	-rm *.o
	-rm prutest

install: install_fw

install_fw: pru1.fw pru0.fw
	install pru1.fw /lib/firmware/am335x-pru1-fw
	install pru0.fw /lib/firmware/am335x-pru0-fw

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