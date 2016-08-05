#include "resource_table.h"


/* flip up bits whose indices represent features we support */
#define RPMSG_PRU_C0_FEATURES	1

struct pru_resource_table am335x_pru_remoteproc_ResourceTable __attribute((section (".resource_table"), used));


struct pru_resource_table {
	struct resource_table base;
};


struct pru_resource_table am335x_pru_remoteproc_ResourceTable = {
  {
    1,	/* we're the first version that implements this */
    0,	/* number of entries in the table */
    {0, 0},	/* reserved, must be zero */
  }
};

