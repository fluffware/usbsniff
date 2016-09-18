#include "usb_ringbuffer.h"
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>

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


struct USBRingBuffer {
  uint32_t start;
  uint32_t end;
  uint32_t read;
  uint32_t write;
};

void
usb_ringbuffer_clear(struct USBRingBuffer *buf)
{
  buf->read = buf->write;
}

size_t
usb_ringbuffer_read(struct USBRingBuffer *buf, uint8_t *data, size_t len)
{
  uint8_t l;
  if (buf->read != buf->write) {
    uint8_t *start = ((uint8_t*)buf) + sizeof(struct USBRingBuffer);
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

struct USBRingBuffer *
usb_ringbuffer_init()
{
  struct USBRingBuffer *buffer = NULL;
  uint8_t *pru = NULL;
  if (map_pru(&pru, (uint8_t**)&buffer) < 0) return NULL;
  usb_ringbuffer_clear(buffer);
  return buffer;
}

