#include <packet_handler.h>
#include <usb_logger.h>
#include <usb_ringbuffer.h>


#define USB_BUF_BYTES 1024
#define USB_BUF_LEN (USB_BUF_BYTES / sizeof(uint32_t))

#define USB_DECODER_BUFFER_OVERFLOW 0x1

struct USBDecoder
{
  unsigned int flags;
  uint8_t dp_prev; /* Last bit from previous block */
  unsigned int one_count; /* Number of consecutive ones seen so far */
  int bit_count; /* Bits of packet so far. -8 means no packet detected.
		    Negative when decoding sync sequence. */
  unsigned int se0_count; /* SE0 count */
  uint32_t buffer[USB_BUF_LEN]; /* Decoded bits of packet */
  unsigned int n_buf_bits; /* Number of bits in buffer */
  timestamp_t sync_ts; /* Timestamp of end of last sync sequence */
  USBLogger *logger;
  USBPacketHandler packet_handler;
  void *packet_handler_user_data;
};

typedef struct USBDecoder USBDecoder;

int
decode_block(USBDecoder *decode, const struct USBSamples *samples, 
	     timestamp_t time);

void
decode_packet(uint32_t *bits, uint32_t n_bits,  timestamp_t ts,void *user_data);
