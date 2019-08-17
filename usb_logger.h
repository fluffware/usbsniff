#include <stdio.h>
#include <timestamp.h>

typedef struct _USBLogger
{
   FILE *log;
} USBLogger;

void
log_error(USBLogger *logger, const char *format,  ...);

void
log_packet(USBLogger *logger, const char *format,  ...);

void
log_packet_start(USBLogger *logger);

void
log_packet_text(USBLogger *logger, const char *format,  ...);

void
log_packet_end(USBLogger *logger);

void
log_time(USBLogger *logger, timestamp_t time);

void
log_init(USBLogger *logger, FILE *file);

void
log_close(USBLogger *logger);
