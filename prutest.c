#include <prussdrv.h>
#include <pruss_intc_mapping.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>

static const tpruss_intc_initdata intc_initdata =
  {
    {PRU0_ARM_INTERRUPT, -1},
    {{PRU0_ARM_INTERRUPT, CHANNEL2},{-1,-1}},
    {{CHANNEL2, PRU_EVTOUT_0},{-1,-1}},
    PRU_EVTOUT0_HOSTEN_MASK
  };

static int
read_sys_string(const char *path, char *buffer, unsigned int len)
{
  char *end;
  int r;
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Failed to open path '%s': %s\n", path, strerror(errno));
    return -1;
  }
  r = read(fd, buffer, len);
  if (r < 0) {
    fprintf(stderr, "Failed to read path '%s': %s\n", path, strerror(errno));
    return -1;
  }
  close(fd);
  if (r < len) r++;
  buffer[r-1] = '\0';
  end = index(buffer, '\n');
  if (end) *end = '\0';
  return 0;
}

static int
read_sys_ulong(const char *path, unsigned long *value)
{
  char *end;
  char buffer[20];
  if (read_sys_string(path, buffer, sizeof(buffer))) return -1;
  *value = strtoul(buffer, &end, 0);
  if (buffer == end) {
    fprintf(stderr, "Illegal integer at '%s'\n", path);
    return -1;
  }
  return 0;
}

static uint8_t *
map_drm(void)
{
  uint8_t *ddr;
  int mem;
  unsigned long addr;
  unsigned long size;
  char name[20];
  if (read_sys_string("/sys/class/uio/uio0/name", name, sizeof(name)))
    return NULL;
  if (strcmp(name , "pruss_evt0") != 0) {
    fprintf(stderr, "Wrong uio name (expected 'pruss_evt0', got '%s')\n", name);
    return NULL;
  }
  if (read_sys_ulong("/sys/class/uio/uio0/maps/map1/addr", &addr))
    return NULL;
  if (read_sys_ulong("/sys/class/uio/uio0/maps/map1/size", &size))
    return NULL;
  fprintf(stderr, "0x%08lx 0x%08lx\n", addr, size);
  prussdrv_pru_write_memory(PRUSS0_SHARED_DATARAM, 0, (unsigned int*)&addr, 4);
  prussdrv_pru_write_memory(PRUSS0_SHARED_DATARAM, 4, (unsigned int*)&size, 4);
  mem = open("/dev/mem", O_RDWR);
  if (mem < 0) {
    fprintf(stderr, "Failed to open /dev/mem: %s\n", strerror(errno));
    return NULL;
  }
  ddr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mem, addr);
  return ddr;
}

static void
dump_shared(unsigned int addr, unsigned int length)
{
  
}

int
main(int argc,const char *argv[])
{
  int res;
  prussdrv_init();
  if (prussdrv_open(PRU_EVTOUT_0) != 0) {
    fprintf(stderr," Failed to open event 0: %s\n",strerror(errno));
     exit(EXIT_FAILURE);
  }
  /* prussdrv_pru_disable(0); */
  /* prussdrv_pru_disable(1); */
  res = prussdrv_pruintc_init(&intc_initdata);
  if (res != 0) {
    fprintf(stderr," Failed to map event 0\n");
    exit(EXIT_FAILURE);
  }
  if (map_drm() == NULL) return EXIT_FAILURE;
  res = prussdrv_exec_program(0,"pru0_prg.bin");
  if (res != 0) {
    fprintf(stderr," Failed to execute program on PRU 0: %d\n",res);
  
  }
  
  /* Wait until PRU0 has finished execution */
  printf("\tINFO: Waiting for HALT command.\r\n");
  prussdrv_pru_wait_event (PRU_EVTOUT_0);
  printf("\tINFO: PRU completed transfer.\r\n");
  prussdrv_pru_clear_event (PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);
  prussdrv_exit();
  return EXIT_FAILURE;
}
