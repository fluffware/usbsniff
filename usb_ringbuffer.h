#include <stdint.h>
#include <stdlib.h>

typedef unsigned long long timestamp_t;
#define NS_PER_BIT 85

struct USBRingBuffer;

struct USBSamples
{
  uint16_t count;
  uint16_t sequence;
  uint32_t dp_bits;
  uint32_t dm_bits;
};

struct USBRingBuffer *
usb_ringbuffer_init();


void
usb_ringbuffer_clear(struct USBRingBuffer *buf);

size_t
usb_ringbuffer_read(struct USBRingBuffer *buf, uint8_t *data, size_t len);
