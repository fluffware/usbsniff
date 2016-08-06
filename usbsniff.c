#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <time.h>

#define MEM "/dev/mem"
#define PRU_BASE 0x4a300000
static int
map_pru(uint8_t **pru, uint8_t **mem)
{
  uint32_t mem_pa;
  uint32_t mem_len;
  int fd = open(MEM, O_RDWR | O_SYNC);
  if (fd < 0) {
    fprintf(stderr,"Failed to open %s: %s\n", MEM, strerror(errno));
    return -1;
  }
  *pru = mmap(NULL, 0x80000, PROT_READ, MAP_SHARED, fd, PRU_BASE);
  if (*pru == MAP_FAILED) {
    fprintf(stderr,"Failed to map PRU: %s\n", strerror(errno));
    return -1;
  }

  mem_pa = *(uint32_t*)&(*pru)[0x12c1c];
  mem_len = *(uint32_t*)&(*pru)[0x12c20];
  *mem = mmap(NULL, mem_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, mem_pa);
  if (*pru == MAP_FAILED) {
    fprintf(stderr,"Failed to map buffer: %s\n", strerror(errno));
    return -1;
  }
  
  return 0;
}

struct RingBuffer {
  uint32_t start;
  uint32_t end;
  uint32_t read;
  uint32_t write;
};

static void
clear_buffer(struct RingBuffer *buf)
{
  buf->read = buf->write;
}

size_t
read_block(struct RingBuffer *buf, uint8_t *data, size_t len)
{
  uint8_t l;
  if (buf->read != buf->write) {
    uint8_t *start = ((uint8_t*)buf) + sizeof(struct RingBuffer);
    uint8_t *r = start + (buf->read - buf->start);
    l = *r++;
    if (l == 0) {
      r = start;
      l = *r++;
    }
    if (l > len) l = len;
    memcpy(data, r, l);
    r += l;
    buf->read = buf->start + (r - start);
    return l;
  } else {
    return 0;
  }
}

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


struct USBSamples
{
  uint16_t count;
  uint16_t sequence;
  uint32_t dp_bits;
  uint32_t dm_bits;
};

static int
write_vcd_sample(FILE *out, const struct USBSamples *samples,
		 unsigned long *time) {
  uint32_t b = 1;
  uint32_t dp = samples->dp_bits;
  uint32_t dp_chg = dp ^ (dp << 1);
  uint32_t dm = samples->dm_bits;
  uint32_t dm_chg = dm ^ (dm << 1);
  fprintf(out, "#%ld\n", *time);

  fprintf(out, "%d+\n%d-\n", dp & 1, dm & 1); 
  for (b = 1; b != 0; b <<= 1) {
    if ((dp_chg | dm_chg) & b) {
      fprintf(out, "#%ld\n", *time);
    }
    if (dp_chg & b) {
      fprintf(out, "%c+\n", (dp & b) ? '1' : '0');
    }
     if (dm_chg & b) {
      fprintf(out, "%c-\n", (dm & b) ? '1' : '0');
    }
    *time += 85;
  }
  *time += 85 * (samples->count - 32);
  return 0;
}

#define USB_BUF_LEN (1024 / sizeof(uint32_t))
struct USBDecoder
{
  uint8_t dp_prev; /* Last bit from previous block */
  unsigned int one_count;
  int bit_count;
  uint32_t buffer[USB_BUF_LEN];
  unsigned int n_buf_bits;
};

#define BIT31 0x80000000

static int
decode_packet(struct USBDecoder *decode, uint32_t packet)
{
  uint8_t pid = packet & 0xff;
  switch(pid) {
  case 0xa5:
    fprintf(stderr, "SOF %3d\n", (packet >> 8) & 0x7ff);
    break;
  case 0x69:
    fprintf(stderr, "IN %3d.%01d\n", (packet >> 8) & 0x7f,
		  (packet >> 15) & 0x0f);
    break;
  case 0xe1:
    fprintf(stderr, "OUT %3d.%01d\n", (packet >> 8) & 0x7f,
	    (packet >> 15) & 0x0f);
    break;
	case 0x2d:
	  fprintf(stderr, "SETUP %3d.%01d\n", (packet >> 8) & 0x7f,
		  (packet >> 15) & 0x0f);
	  break;
  case 0xd2:
    fprintf(stderr, "ACK\n");
	  break;
  case 0x5a:
    fprintf(stderr, "NACK\n");
    break;
  case 0x1e:
    fprintf(stderr, "STALL\n");
    break;

  case 0xc3:
    fprintf(stderr, "DATA0\n");
    break;
    
  case 0x4b:
    fprintf(stderr, "DATA1\n");
    break;
    
  default:
    fprintf(stderr, "Invalid PID %02x\n",pid);
    break;
  }
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
decode_block(struct USBDecoder *decode, const struct USBSamples *samples)
{
  uint32_t se0 = ~(samples->dp_bits | samples->dm_bits);
  uint32_t dp = samples->dp_bits;
  uint32_t decoded = (~dp ^ ((dp << 1) | decode->dp_prev));
  uint32_t bits_end = 32;
  uint32_t data_bits_end;
  uint32_t bits_pos = 0; 
  decode->dp_prev = dp >> 31;
  
  /* fprintf(stderr, "%08x %08x %08x\n", dp, se0, decoded); */
  while(bits_end > bits_pos) {
    int b;
    /* fprintf(stderr, "%d\n",  bits_pos); */
    if (se0 & (1<<bits_pos)) {
      /* fprintf(stderr, "Got %d bits\n", decode->n_buf_bits); */
      /* fprintf(stderr, "EOP\n"); */
      if (decode->n_buf_bits >= 8) {
	decode_packet(decode, decode->buffer[0]);
      } else if (decode->n_buf_bits != 0) {
	fprintf(stderr, "Short packet\n");
      }
      bits_pos++;
      decode->bit_count = -8;
      decode->n_buf_bits = 0;
      continue;
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
	decode->n_buf_bits = 0;
      }
      for (b = bits_pos; b < data_bits_end; b ++) {
	if (decoded & (1<<b)) {
	  decode->one_count++;
	} else {
	  if (decode->one_count > 6) {
	    fprintf(stderr,"Bit stuff error\n");
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
	  fprintf(stderr, "Reset\n");
	  decode->bit_count = -8;
	}
      } else {
	decode->bit_count += (b - bits_pos) + 1;
	if (decode->bit_count > 0) {
	  fprintf(stderr, "Reset\n");
	  decode->bit_count = -8;
	} else if (decode->bit_count < 0) {
	  fprintf(stderr, "Short sync\n");
	  decode->bit_count = -8;
	} else {
	  decode->one_count = 1;
	  /* fprintf(stderr,"Sync end\n"); */
	  b++;
	}
      }
      bits_pos = b;
    }
  }
  if (samples->count > 32) {
    unsigned long extra = samples->count - 32;
    if ((samples->dp_bits | samples->dm_bits) & BIT31) {
      if (decode->bit_count >= 0) {
	decode->one_count += extra;
	if (decode->one_count <= 6) {
	  add_bits(decode, ~(uint32_t)0, extra);
	} else {
	  decode->bit_count = -8;
	}
      }
    }
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  uint8_t *pru = NULL;
  uint8_t *buffer = NULL;
  map_pru(&pru, &buffer);

  {
    struct USBDecoder decoder = {0};
    
    unsigned long time = 0;
    uint8_t data[16];
    size_t len;
    
    decoder.n_buf_bits = 0;
    decoder.bit_count = -8;
    decoder.one_count = 0;
    
    clear_buffer((struct RingBuffer *)buffer);
    write_vcd_header(stdout);
    while(1) {
      while((len = read_block((struct RingBuffer *)buffer, data, sizeof(data)))
	  == 0) {
	/* fprintf(stderr,"Wait\n"); */
	usleep(10000);
      }
      /* fprintf(stderr, "Time: %ld\n", time); */
      decode_block(&decoder,(struct USBSamples *)data);
      write_vcd_sample(stdout, (struct USBSamples *)data, &time);
    }
  }
  return EXIT_SUCCESS;
}
