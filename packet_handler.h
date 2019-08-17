
#include <stdint.h>
#include <timestamp.h>

typedef void (*USBPacketHandler)(uint32_t *bits, uint32_t n_bits, 
				 timestamp_t ts, 
				 void *user_data);
