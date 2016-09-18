#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <crc5.h>
#include <crc16.h>
#include <usb_ringbuffer.h>

int
main(int argc, char *argv[])
{
  FILE *dump_out = NULL;
  struct USBRingBuffer *buffer = NULL;
  timestamp_t time = 0;
  uint8_t data[16];
  size_t len;
  int32_t next_sequence = -1;
  
  if (argc < 2) {
    fprintf(stderr,"usage: usbdump <dumpfile>\n");
    exit(EXIT_FAILURE);
  }
  
  buffer = usb_ringbuffer_init();
  if (!buffer) exit(EXIT_FAILURE);

  if (argv[1]) {
    if (argv[1][0] == '-') {
      dump_out = stdout;
    } else {
      dump_out = fopen(argv[1],"w");
      if (!dump_out) {
	fprintf(stderr, "Failed to open file %s for writing: %s\n",
		argv[1], strerror(errno));
	exit(EXIT_FAILURE);
      }
    }
  }

   usb_ringbuffer_clear(buffer);
   while(1) {
    while((len = usb_ringbuffer_read(buffer, data, sizeof(data)))
	    == 0) {
      /* fprintf(stderr,"Wait\n"); */
      usleep(10000);
    }
    if (len == sizeof(struct USBSamples)) {
      struct USBSamples *samples = (struct USBSamples *)data;
      if (samples->sequence != next_sequence && next_sequence != -1) {
	fprintf(stderr, "Packet sequence error expected %d, got %d\n",
		next_sequence, samples->sequence);
	usb_ringbuffer_clear(buffer);
	next_sequence = -1;
      } else {
	next_sequence = (samples->sequence + 1) & 0xffff;
      }

      time += samples->count * NS_PER_BIT;

      fwrite(samples, sizeof(struct USBSamples), 1, dump_out);
      if (ferror(dump_out)) {
	fprintf(stderr, "Failed to write to file: %s", strerror(errno));
	exit(EXIT_FAILURE);
      }
    }
  }
return EXIT_SUCCESS;
}
