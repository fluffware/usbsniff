#include "usb_packet_decoder.h"
#include <crc5.h>
#include <crc16.h>


#define BIT31 0x80000000

static int
check_short_crc(USBLogger *logger, uint16_t data)
{
  uint8_t crc = 0x1f;
  crc = crc5_update(crc, data);
  crc = crc5_update(crc, data >> 8);
  if (crc != 0x06) {
    log_error(logger, "CRC error\n");
    return 0;
  }
  return 1;
}

static int
check_crc16(USBLogger *logger, uint8_t *data, unsigned int len)
{
  uint16_t crc = 0xffff;
  while(len -- > 0) {
    crc = crc16_update(crc, *data++);
  }
  if (crc != 0xb001) {
    log_error(logger, "CRC error\n");
    return 0;
  }
  return 1;
}

static int
decode_data_packet(USBLogger *logger, 
		   const uint32_t *bits, uint32_t n_bits,
		   const char *name)
{
  if (check_crc16(logger, ((uint8_t*)bits) + 1, n_bits / 8 - 1)) {
    int i;
    log_packet_start(logger);
    log_packet_text(logger, "%s", name);
    for (i = 1; i < n_bits/8 - 2; i++) {
      log_packet_text(logger, " %02x", ((uint8_t*)bits)[i]);
    }
    log_packet_end(logger);
  } else {
    log_packet(logger, "%s", name);
  }
  return 0;
}

void
decode_packet(uint32_t *bits, uint32_t n_bits, timestamp_t ts, void *user_data)
{
  USBLogger *logger = user_data;
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
  log_time(logger, ts);
  switch(pid) {
  case 0xa5:
    check_short_crc(logger, packet >> 8);
    log_packet(logger, "SOF %3d", (packet >> 8) & 0x7ff);
    break;
  case 0x69:
    check_short_crc(logger, packet >> 8);
    log_packet(logger, "IN %3d.%01d", (packet >> 8) & 0x7f,
	       (packet >> 15) & 0x0f);
    break;
  case 0xe1:
    check_short_crc(logger, packet >> 8);
    log_packet(logger, "OUT %3d.%01d", (packet >> 8) & 0x7f,
	       (packet >> 15) & 0x0f);
    break;
  case 0x2d:
    check_short_crc(logger, packet >> 8);
    log_packet(logger, "SETUP %3d.%01d", (packet >> 8) & 0x7f,
	    (packet >> 15) & 0x0f);
    break;
  case 0xd2:
    log_packet(logger, "ACK");
	  break;
  case 0x5a:
    log_packet(logger, "NACK");
    break;
  case 0x1e:
    log_packet(logger, "STALL");
    break;

  case 0xc3:
    decode_data_packet(logger, bits, n_bits, "DATA0");
    break;
    
  case 0x4b:
    decode_data_packet(logger, bits, n_bits, "DATA1");
    break;
    
  default:
    log_error(logger, "Invalid PID %02x",pid);
    break;
  }
}

inline void
add_bits(struct USBDecoder *decode, uint32_t bits, unsigned long n_bits)
{
  unsigned long offset = decode->n_buf_bits >> 5;
  unsigned int shift = decode->n_buf_bits & 0x1f;
  uint32_t mask;
  if ((decode->n_buf_bits + n_bits) > USB_BUF_LEN * 32) {
    n_bits = USB_BUF_LEN * 32 - decode->n_buf_bits;
    if (!(decode->flags & USB_DECODER_BUFFER_OVERFLOW)) {
      log_error(decode->logger,"Bits lost due to buffer overflow");
    }
    decode->flags |= USB_DECODER_BUFFER_OVERFLOW;
  } else {
    decode->flags &= ~USB_DECODER_BUFFER_OVERFLOW;
  }
  mask = (1<<n_bits) - 1;
  if (offset < USB_BUF_LEN) {
    decode->buffer[offset] = ((decode->buffer[offset] & ~(mask<<shift)) 
			      | ((bits & mask) << shift));
    
    if (shift + n_bits > 32) {
      decode->buffer[offset + 1] = ((decode->buffer[offset + 1] 
				     & ~(mask>>(32 - shift))) 
				    | ((bits & mask) >> (32 - shift)));
    }
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
  



int
decode_block(USBDecoder *decode, const struct USBSamples *samples, 
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
	log_packet(decode->logger, "RESET");
	decode->bit_count = -8;
	decode->n_buf_bits = 0;
      } else if (decode->se0_count > 0) {
	/* fprintf(stderr, "Got %d bits\n", decode->n_buf_bits); */
	/* fprintf(stderr, "EOP\n"); */
	if (decode->n_buf_bits >= 8) {
	  decode->packet_handler(decode->buffer, decode->n_buf_bits,
				 decode->sync_ts,
				 decode->packet_handler_user_data);
	} else if (decode->n_buf_bits != 0) {
	  log_error(decode->logger,"Short packet");
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
	    log_error(decode->logger,"Bit stuff error");
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
	  log_error(decode->logger, "Long sync");
	  decode->bit_count = -8;
	}
      } else {
	decode->bit_count += (b - bits_pos) + 1;
	if (decode->bit_count > 0) {
	  log_error(decode->logger, "Long sync");
	  decode->bit_count = -8;
	} else if (decode->bit_count < 0) {
	  log_error(decode->logger, "Short sync\n");
	  decode->bit_count = -8;
	} else {
	  decode->one_count = 1;
	  /* fprintf(stderr,"Sync end\n"); */
	  b++;
	  decode->n_buf_bits = 0;
	  decode->sync_ts = time + bits_pos * NS_PER_BIT;
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
	decode->sync_ts = time + bits_pos * NS_PER_BIT;
	extra--;
	decode->bit_count++;
      } else if (decode->bit_count < -1 && decode->bit_count > -8) {
	log_error(decode->logger, "Short sync\n");
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
