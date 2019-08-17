#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <time.h>


#include <usb_ringbuffer.h>
#include <usb_packet_decoder.h>



static int
write_vcd_header(FILE *out)
{
  time_t now;
  char now_str[26];
  now = time(NULL);
  ctime_r(&now, now_str);
  fprintf(out,"$date %s $end\n", now_str);
  fputs("$version usbsniff $end\n", out);
  fputs("$timescale 1 ns $end\n"
	"$scope module top $end\n"
	"$var wire 1 + DP $end\n"
	"$var wire 1 - DM $end\n"
	"$upscope $end\n"
	"$enddefinitions $end\n",
	out);
  return 0;
}



static int
write_vcd_sample(FILE *out, const struct USBSamples *samples,
		 timestamp_t time) {
  uint32_t b = 1;
  uint32_t dp = samples->dp_bits;
  uint32_t dp_chg = dp ^ (dp << 1);
  uint32_t dm = samples->dm_bits;
  uint32_t dm_chg = dm ^ (dm << 1);
  fprintf(out, "#%lld\n", time);

  fprintf(out, "%d+\n%d-\n", dp & 1, dm & 1); 
  for (b = 1; b != 0; b <<= 1) {
    if ((dp_chg | dm_chg) & b) {
      fprintf(out, "#%lld\n", time);
    }
    if (dp_chg & b) {
      fprintf(out, "%c+\n", (dp & b) ? '1' : '0');
    }
     if (dm_chg & b) {
      fprintf(out, "%c-\n", (dm & b) ? '1' : '0');
    }
    time += NS_PER_BIT;
  }
  return 0;
}



static void
usage(void) {
  fprintf(stderr, 
	  "usage: usbsniff [options]\n"
	  "\t-V FILE     VCD file\n"
	  "\t-D	FILE     Decoded USB packets\n"
	  "\t-i FILE     Use this dum file as input instead of hardware\n"
	  );
  
}

int
main(int argc, char *argv[])
{
  FILE *vcd_out = NULL;
  FILE *decoded_out = NULL;
  FILE *input = NULL;
  char *vcd_filename = NULL;
  char *decoded_filename = NULL;
  char *input_filename = NULL;
  struct USBRingBuffer *buffer = NULL;
  USBLogger logger;
  struct USBDecoder decoder = {0};
  int opt;
  timestamp_t time = 0;
  uint8_t data[16];
  size_t len;
  int32_t next_sequence = -1;
  
  while ((opt = getopt(argc, argv, "V:D:i:")) != -1) {
    switch (opt) {
    case 'V':
      vcd_filename = optarg;
      break;
    case 'D':
      decoded_filename = optarg;
      break;
    case 'i':
      input_filename = optarg;
      break;
      
    default: /* '?' */
      usage();
      exit(EXIT_FAILURE);
    }
  }
  

  if (input_filename) {
    input = fopen(input_filename,"rb");
    if (!input) {
      fprintf(stderr, "Failed to open file %s for reading: %s\n",
	      input_filename, strerror(errno));
      exit(EXIT_FAILURE);
    }
  } else { 
    buffer = usb_ringbuffer_init();
    if (!buffer) exit(EXIT_FAILURE);
  }
  
  decoder.n_buf_bits = 0;
  decoder.bit_count = -8;
  decoder.one_count = 0;

  decoder.packet_handler = decode_packet;
  decoder.packet_handler_user_data = &logger;
  decoder.logger = &logger;

  if (decoded_filename) {
    if (decoded_filename[0] == '-') {
      decoded_out = stdout;
    } else {
      decoded_out = fopen(decoded_filename,"w");
      if (!decoded_out) {
	fprintf(stderr, "Failed to open file %s for writing: %s\n",
		decoded_filename, strerror(errno));
      }
    }
  }

  log_init(&logger, decoded_out);

  if (vcd_filename) {
    if (vcd_filename[0] == '-') {
      vcd_out = stdout;
    } else {
      vcd_out = fopen(vcd_filename,"w");
      if (!vcd_out) {
	fprintf(stderr, "Failed to open file %s for writing: %s\n",
		vcd_filename, strerror(errno));
      }
    }
  }

  
  if (vcd_out) {
    write_vcd_header(vcd_out);
  }
  while(1) {
    if (buffer) {
      while((len = usb_ringbuffer_read(buffer, data, sizeof(data)))
	    == 0) {
	/* fprintf(stderr,"Wait\n"); */
	usleep(10000);
      }
    } else {
      len = sizeof(struct USBSamples);
      fread(data, len, 1, input);
      if (ferror(input)) {
	fprintf(stderr, "Failed to read input file: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
      }
      if (feof(input)) break;
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

      /* fprintf(stderr, "Time: %ld %ld\n", time, samples->count);  */
      if (decoded_out) {
	
	decode_block(&decoder,samples, time);
      }
      if (vcd_out) {
	write_vcd_sample(vcd_out, samples, time);
      }
      time += samples->count * NS_PER_BIT;
    }
  }
  log_close(&logger);

return EXIT_SUCCESS;
}
