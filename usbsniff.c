#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <crc5.h>
#include <crc16.h>
#include <stdarg.h>
#include <usb_ringbuffer.h>



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

#define USB_BUF_LEN (1024 / sizeof(uint32_t))
struct USBDecoder
{
  uint8_t dp_prev; /* Last bit from previous block */
  uint8_t pid_prev; /* PID of previous token */
  unsigned int one_count; /* Number of consecutive ones seen so far */
  int bit_count; /* Bits of packet so far. -8 means no packet detected.
		    Negative when decoding sync sequence. */
  unsigned int se0_count; /* SE0 count */
  uint32_t buffer[USB_BUF_LEN]; /* Decoded bits of packet */
  unsigned int n_buf_bits; /* Number of bits in buffer */
  FILE *log;

};


static void
log_error(struct USBDecoder *decode, const char *format,  ...)
{
  va_list ap;
  va_start(ap, format);
  fputs("! ",decode->log);
  vfprintf(decode->log, format, ap);
  fputc('\n', decode->log);
  va_end(ap);
}

static void
log_packet(struct USBDecoder *decode, const char *format,  ...)
{
  va_list ap;
  va_start(ap, format);
  vfprintf(decode->log, format, ap);
  fputc('\n', decode->log);
  va_end(ap);
}

static void
log_packet_start(struct USBDecoder *decode)
{
}

static void
log_packet_text(struct USBDecoder *decode, const char *format,  ...)
{
  va_list ap;
  va_start(ap, format);
  vfprintf(decode->log, format, ap);
  va_end(ap);
}

static void
log_packet_end(struct USBDecoder *decode)
{
  fputc('\n', decode->log);
}

static void
log_time(struct USBDecoder *decode, timestamp_t time)
{
  fprintf(decode->log, "# %lld ns\n", time);
}

#define BIT31 0x80000000

static int
check_short_crc(struct USBDecoder *decode, uint16_t data)
{
  uint8_t crc = 0x1f;
  crc = crc5_update(crc, data);
  crc = crc5_update(crc, data >> 8);
  if (crc != 0x06) {
    log_error(decode, "CRC error\n");
    return 0;
  }
  return 1;
}

static int
check_crc16(struct USBDecoder *decode, uint8_t *data, unsigned int len)
{
  uint16_t crc = 0xffff;
  while(len -- > 0) {
    crc = crc16_update(crc, *data++);
  }
  if (crc != 0xb001) {
    log_error(decode, "CRC error\n");
    return 0;
  }
  return 1;
}

static int
decode_data_packet(struct USBDecoder *decode, 
		   const uint32_t *bits, uint32_t n_bits,
		   const char *name)
{
  if (check_crc16(decode, ((uint8_t*)bits) + 1, n_bits / 8 - 1)) {
    int i;
    log_packet_start(decode);
    log_packet_text(decode, "%s", name);
    for (i = 1; i < n_bits/8 - 2; i++) {
      log_packet_text(decode, " %02x", ((uint8_t*)bits)[i]);
    }
    log_packet_end(decode);
  } else {
    log_packet(decode, "%s", name);
  }
  return 0;
}

static int
decode_packet(struct USBDecoder *decode, uint32_t *bits, uint32_t n_bits)
{
  uint32_t packet = bits[0];
  uint8_t pid = packet & 0xff;
#if 0
  {
    int i;
    for (i = 0; i < n_bits / 32; i++) {
      fprintf(stderr, "%08x ", bits[i]);
    }
    if ((n_bits & 0x1f) > 0) {
      fprintf(stderr, "%08x /%d\n", bits[i] & ~((~(uint32_t)0)<< n_bits & 0x1f), n_bits & 0x1f);
    } else {
      fputc('\n', stderr);
    }
  }
#endif
  switch(pid) {
  case 0xa5:
    check_short_crc(decode, packet >> 8);
    log_packet(decode, "SOF %3d", (packet >> 8) & 0x7ff);
    break;
  case 0x69:
    check_short_crc(decode, packet >> 8);
    log_packet(decode, "IN %3d.%01d", (packet >> 8) & 0x7f,
	       (packet >> 15) & 0x0f);
    break;
  case 0xe1:
    check_short_crc(decode, packet >> 8);
    log_packet(decode, "OUT %3d.%01d", (packet >> 8) & 0x7f,
	       (packet >> 15) & 0x0f);
    break;
  case 0x2d:
    check_short_crc(decode, packet >> 8);
    log_packet(decode, "SETUP %3d.%01d", (packet >> 8) & 0x7f,
	    (packet >> 15) & 0x0f);
    break;
  case 0xd2:
    log_packet(decode, "ACK");
	  break;
  case 0x5a:
    log_packet(decode, "NACK");
    break;
  case 0x1e:
    log_packet(decode, "STALL");
    break;

  case 0xc3:
    decode_data_packet(decode, bits, n_bits, "DATA0");
    break;
    
  case 0x4b:
    decode_data_packet(decode, bits, n_bits, "DATA1");
    break;
    
  default:
    log_error(decode, "Invalid PID %02x",pid);
    break;
  }
  decode->pid_prev = pid;
  return 0;
}

inline void
add_bits(struct USBDecoder *decode, uint32_t bits, unsigned long n_bits)
{
  unsigned long offset = decode->n_buf_bits >> 5;
  unsigned int shift = decode->n_buf_bits & 0x1f;
  uint32_t mask = (1<<n_bits) - 1;

  decode->buffer[offset] = ((decode->buffer[offset] & ~(mask<<shift)) 
			    | ((bits & mask) << shift));

  if (shift + n_bits > 32) {
    decode->buffer[offset + 1] = ((decode->buffer[offset + 1] 
				   & ~(mask>>(32 - shift))) 
				  | ((bits & mask) >> (32 - shift)));
  }
  decode->n_buf_bits += n_bits;
}


static int
find_lowest_one_from(uint32_t w, int from)
{
  int shift = 0;
  w &= (~(uint32_t)0) << from;
  if (w == 0) return 32;
  if (!(w & 0x0000ffff)) {
    shift += 16;
  }
  if (!(w & (0x00ff << shift))) {
    shift += 8;
  }
  if (!(w & (0x0f << shift))) {
    shift += 4;
  }
  if (!(w & (0x03 << shift))) {
    shift += 2;
  }
  if (!(w & (0x01 << shift))) {
    shift += 1;
  }
  return shift;
}
  



static int
decode_block(struct USBDecoder *decode, const struct USBSamples *samples, 
	     timestamp_t time)
{
  uint32_t se0 = ~(samples->dp_bits | samples->dm_bits);
  uint32_t dp = samples->dp_bits;
  uint32_t decoded = (~dp ^ ((dp << 1) | decode->dp_prev));
  uint32_t bits_end = 32;
  uint32_t data_bits_end; /* Next SE0 position */
  uint32_t bits_pos = 0; /* Current bit position */
  decode->dp_prev = dp >> 31;
  
  /* fprintf(stderr, "%08x %08x %08x\n", dp, se0, decoded); */
  while(bits_end > bits_pos) {
    int b;
    /* fprintf(stderr, "%d\n",  bits_pos); */
    if (se0 & (1<<bits_pos)) {
      decode->se0_count++;
      bits_pos++;
      continue;
    } else {
      if (decode->se0_count >=30) {
	log_packet(decode, "RESET");
	decode->bit_count = -8;
	decode->n_buf_bits = 0;
      } else if (decode->se0_count > 0) {
	/* fprintf(stderr, "Got %d bits\n", decode->n_buf_bits); */
	/* fprintf(stderr, "EOP\n"); */
	if (decode->n_buf_bits >= 8) {
	decode_packet(decode, decode->buffer, decode->n_buf_bits);
	} else if (decode->n_buf_bits != 0) {
	  log_error(decode,"Short packet");
	}
	decode->bit_count = -8;
	decode->n_buf_bits = 0;
      }
      decode->se0_count = 0;
    }
    b = find_lowest_one_from(se0, bits_pos);
    if (b < bits_end) {
      data_bits_end = b;
    } else {
      data_bits_end = bits_end;
    }
    if (decode->bit_count >= 0) {
      /* Collecting data */
      if (decode->bit_count == 0) {
      }
      for (b = bits_pos; b < data_bits_end; b ++) {
	if (decoded & (1<<b)) {
	  decode->one_count++;
	} else {
	  if (decode->one_count > 6) {
	    log_error(decode,"Bit stuff error");
	    bits_pos = b;
	    decode->bit_count = -8;
	    decode->one_count = 0;
	    break;
	  } else if (decode->one_count == 6) {
	    add_bits(decode, decoded >>bits_pos, b - bits_pos);
	    bits_pos = b + 1;
	    decode->one_count = 0;
	    break;
	  }
	  decode->one_count = 0;
	}
	decode->bit_count++;
      }
      if (b == data_bits_end) {
	add_bits(decode, decoded >> bits_pos, b - bits_pos);
	bits_pos = data_bits_end;
      }
    } else if (decode->bit_count == -8) {
      /* Look for packet start */
      b = find_lowest_one_from(~dp, bits_pos);
      if (b >= data_bits_end) {
	bits_pos = data_bits_end;
      } else {
	bits_pos = b + 1; /* remove first zero */
	decode->bit_count++;
	/* fprintf(stderr,"Sync start\n"); */
      }
    } else {
      /* Inside sync */
      b = find_lowest_one_from(decoded, bits_pos);
      if (b >= data_bits_end) {
	b = data_bits_end;
	decode->bit_count += data_bits_end - bits_pos;
	if (decode->bit_count >= 0) {
	  log_error(decode, "Long sync");
	  decode->bit_count = -8;
	}
      } else {
	decode->bit_count += (b - bits_pos) + 1;
	if (decode->bit_count > 0) {
	  log_error(decode, "Long sync");
	  decode->bit_count = -8;
	} else if (decode->bit_count < 0) {
	  log_error(decode, "Short sync\n");
	  decode->bit_count = -8;
	} else {
	  decode->one_count = 1;
	  /* fprintf(stderr,"Sync end\n"); */
	  b++;
	  decode->n_buf_bits = 0;
	  log_time(decode, time + bits_pos * NS_PER_BIT);
	}
      }
      bits_pos = b;
    }
  }
  if (samples->count > 32) {
    unsigned long extra = samples->count - 32;
    /* Extend the last bit for extra bits.
       Note that all extra bits are allways ones or SE0. */
    if ((samples->dp_bits | samples->dm_bits) & BIT31) {
      if (decode->bit_count == -1) {
	/* Last bit of sync */
	decode->one_count = 1;
	decode->n_buf_bits = 0;
	log_time(decode, time + bits_pos * NS_PER_BIT);
	extra--;
	decode->bit_count++;
      } else if (decode->bit_count < -1 && decode->bit_count > -8) {
	log_error(decode, "Short sync\n");
	decode->bit_count = -8;
      }
      if (extra > 0 && decode->bit_count >= 0) {
	decode->one_count += extra;
	if (decode->one_count <= 6) {
	  add_bits(decode, ~(uint32_t)0, extra);
	} else {
	  decode->bit_count = -8;
	}
      }
	  
    } else {
      decode->se0_count += extra;
    }
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
	decoder.log = decoded_out;
	decode_block(&decoder,samples, time);
      }
      if (vcd_out) {
	write_vcd_sample(vcd_out, samples, time);
      }
      time += samples->count * NS_PER_BIT;
    }
  }
return EXIT_SUCCESS;
}
