#include "resource_table.h"

/*
 * Sizes of the virtqueues (expressed in number of buffers supported,
 * and must be power of 2)
 */
#define PRU_RPMSG_VQ0_SIZE	256
#define PRU_RPMSG_VQ1_SIZE	256

/* flip up bits whose indices represent features we support */
#define RPMSG_PRU_C0_FEATURES	1

struct pru_resource_table am335x_pru_remoteproc_ResourceTable __attribute((section (".resource_table"), used));

#if 0
struct pru_resource_table {
	struct resource_table base;

	uint32_t offset[1]; /* Should match 'num' in actual definition */

	/* rpmsg vdev entry */
  	struct fw_rsc_hdr rpmsg_vdev_hdr;
  	struct fw_rsc_vdev rpmsg_vdev;
	struct fw_rsc_vdev_vring rpmsg_vring0;
	struct fw_rsc_vdev_vring rpmsg_vring1;

	
};


struct pru_resource_table am335x_pru_remoteproc_ResourceTable = {
  {
    1,	/* we're the first version that implements this */
    1,	/* number of entries in the table */
    {0, 0},	/* reserved, must be zero */
  },
  {		/* offset[0] */
    offsetof(struct pru_resource_table, rpmsg_vdev_hdr),
  },
  /* rpmsg vdev entry */

  {RSC_VDEV}, 
  {
    VIRTIO_ID_RPMSG, 0,
    RPMSG_PRU_C0_FEATURES, 0, 0, 0, 2, { 0, 0 }
    /* no config data */
  },
  /* the two vrings */
  /* TODO: What to do with vring da? */
  { 0, 16, PRU_RPMSG_VQ0_SIZE, 1, 0 },
  { 0, 16, PRU_RPMSG_VQ1_SIZE, 2, 0 }
  
};

#else
struct pru_resource_table {
  struct resource_table base;
  
  uint32_t offset[1]; /* Should match 'num' in actual definition */
  
  /* prut intc entry */
  struct fw_rsc_hdr pru_mem_hdr;
  struct fw_rsc_carveout pru_mem;
};


struct pru_resource_table am335x_pru_remoteproc_ResourceTable = {
  {
    1,	/* we're the first version that implements this */
    1,	/* number of entries in the table */
    {0, 0},	/* reserved, must be zero */
  },
  {		/* offset[0] */
    offsetof(struct pru_resource_table, pru_mem_hdr),
  },
  /* rpmsg vdev entry */

  {RSC_CARVEOUT}, 
  {
    FW_RSC_ADDR_ANY,
    FW_RSC_ADDR_ANY,
    1024*1024,
    0, /* flags */
    0,
    "PRU1"
  },
};
#endif
