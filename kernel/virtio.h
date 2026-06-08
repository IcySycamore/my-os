// kernel/virtio.h — VirtIO MMIO register definitions (subset)
#ifndef _VIRTIO_H_
#define _VIRTIO_H_

// Try 0x10008000 first (older QEMU), then 0x10001000 (newer QEMU)
#define VIRTIO0            0x10008000L
#define VIRTIO_MMIO_MAGIC_VALUE       0x000
#define VIRTIO_MMIO_VERSION           0x004
#define VIRTIO_MMIO_DEVICE_ID         0x008
#define VIRTIO_MMIO_VENDOR_ID         0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES   0x010
#define VIRTIO_MMIO_DRIVER_FEATURES   0x020
#define VIRTIO_MMIO_QUEUE_SEL         0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX     0x034
#define VIRTIO_MMIO_QUEUE_NUM         0x038
#define VIRTIO_MMIO_QUEUE_READY       0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY      0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS  0x060
#define VIRTIO_MMIO_INTERRUPT_ACK     0x064
#define VIRTIO_MMIO_STATUS            0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW    0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH   0x084
#define VIRTIO_MMIO_DRIVER_DESC_LOW   0x090
#define VIRTIO_MMIO_DRIVER_DESC_HIGH   0x094
#define VIRTIO_MMIO_DEVICE_DESC_LOW   0x0a0
#define VIRTIO_MMIO_DEVICE_DESC_HIGH  0x0a4

#define VIRTIO_CONFIG_S_ACKNOWLEDGE    1
#define VIRTIO_CONFIG_S_DRIVER         2
#define VIRTIO_CONFIG_S_DRIVER_OK      4
#define VIRTIO_CONFIG_S_FEATURES_OK    8

#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

#define NUM 8

// virtio_blk request types
#define VIRTIO_BLK_T_IN   0
#define VIRTIO_BLK_T_OUT  1

struct virtq_desc {
    unsigned long addr;
    unsigned int  len;
    unsigned short flags;
    unsigned short next;
};

struct virtq_avail {
    unsigned short flags;
    unsigned short idx;
    unsigned short ring[NUM];
};

struct virtq_used_elem {
    unsigned int  id;
    unsigned int  len;
};

struct virtq_used {
    unsigned short flags;
    unsigned short idx;
    struct virtq_used_elem ring[NUM];
};

struct virtio_blk_req {
    unsigned int  type;
    unsigned int  reserved;
    unsigned long sector;
};

// API
void virtio_blk_init(void);
int  virtio_blk_read(unsigned long sector, char *buf);

#endif
