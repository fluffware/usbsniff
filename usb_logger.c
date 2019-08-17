#include "usb_logger.h"
#include <stdarg.h>

void
log_error(USBLogger *logger, const char *format,  ...)
{
  va_list ap;
  va_start(ap, format);
  fputs("! ",logger->log);
  vfprintf(logger->log, format, ap);
  fputc('\n', logger->log);
  va_end(ap);
}

void
log_packet(USBLogger *logger, const char *format,  ...)
{
  va_list ap;
  va_start(ap, format);
  vfprintf(logger->log, format, ap);
  fputc('\n', logger->log);
  va_end(ap);
}

void
log_packet_start(USBLogger *logger)
{
}

void
log_packet_text(USBLogger *logger, const char *format,  ...)
{
  va_list ap;
  va_start(ap, format);
  vfprintf(logger->log, format, ap);
  va_end(ap);
}

void
log_packet_end(USBLogger *logger)
{
  fputc('\n', logger->log);
}

void
log_time(USBLogger *logger, timestamp_t time)
{
  fprintf(logger->log, "# %lld ns\n", time);
}

void
log_init(USBLogger *logger, FILE *file)
{
  logger->log = file;
}

void
log_close(USBLogger *logger)
{
}
