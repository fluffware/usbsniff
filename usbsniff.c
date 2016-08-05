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

int
main(int argc, char *argv[])
{
  uint8_t *pru = NULL;
  uint8_t *buffer = NULL;
  map_pru(&pru, &buffer);

  {
    unsigned long time = 0;
    uint8_t data[16];
    size_t len;
    clear_buffer((struct RingBuffer *)buffer);
    write_vcd_header(stdout);
    while(1) {
      while((len = read_block((struct RingBuffer *)buffer, data, sizeof(data)))
	  == 0) {
	fprintf(stderr,"Wait\n");
	usleep(10000);
      }
      
      write_vcd_sample(stdout, (struct USBSamples *)data, &time);
    }
  }
  return EXIT_SUCCESS;
}
