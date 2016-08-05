#include <stdint.h>
#include <stddef.h>
typedef uint32_t u32;
typedef uint8_t u8;

#define __packed __attribute((packed))

/* Definitions copied from remoteproc.h in the Linux kernel. */
struct resource_table {
        u32 ver;
        u32 num;
        u32 reserved[2];
        u32 offset[0];
} __packed;

struct fw_rsc_hdr {
        u32 type;
        u8 data[0];
} __packed;

enum fw_resource_type {
        RSC_CARVEOUT    = 0,
        RSC_DEVMEM      = 1,
        RSC_TRACE       = 2,
        RSC_VDEV        = 3,
        RSC_INTMEM      = 4,
        RSC_CUSTOM      = 5,
        RSC_LAST        = 6
};

#define FW_RSC_ADDR_ANY ((u32)(~0))

struct fw_rsc_carveout {
        u32 da;
        u32 pa;
        u32 len;
        u32 flags;
        u32 reserved;
        u8 name[32];
} __packed;


struct fw_rsc_vdev_vring {
        u32 da;
        u32 align;
        u32 num;
        u32 notifyid;
        u32 reserved;
} __packed;

struct fw_rsc_vdev {
        u32 id;
        u32 notifyid;
        u32 dfeatures;
        u32 gfeatures;
        u32 config_len;
        u8 status;
        u8 num_of_vrings;
        u8 reserved[2];
        struct fw_rsc_vdev_vring vring[0];
} __packed;

struct fw_rsc_custom {
        u32  sub_type;
        u32  size;
        u8   data[0];
} __packed;

/* End of definitions from remoteproc.h */

/* From virtio_ids.h */
#define VIRTIO_ID_RPMSG         7 /* virtio remote processor messaging */
