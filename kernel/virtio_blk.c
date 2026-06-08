// kernel/virtio_blk.c — Minimal polling virtio-blk driver
#include "virtio.h"

static unsigned long VIRTIO_BASE;

#define R(r) ((volatile unsigned int *)(VIRTIO_BASE + (r)))

static struct {
    struct virtq_desc  desc[NUM];
    struct virtio_blk_req ops[NUM];
    char status[NUM];
    char free[NUM];
    unsigned short used_idx;
} disk;

static struct virtq_avail disk_avail __attribute__((aligned(4096)));
static struct virtq_used  disk_used  __attribute__((aligned(4096)));

void uart_puts(const char *s);
void uart_puthex(unsigned long x);

void virtio_blk_init(void)
{
    VIRTIO_BASE = 0x10001000L;

    if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
        *R(VIRTIO_MMIO_VERSION) != 2 ||
        *R(VIRTIO_MMIO_DEVICE_ID) != 2) {
        return;
    }

    unsigned int status = 0;
    *R(VIRTIO_MMIO_STATUS) = 0;
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;
    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;

    unsigned long features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
    features &= ~0x1FFUL;
    *R(VIRTIO_MMIO_DRIVER_FEATURES) = (unsigned int)features;

    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    if (!(*R(VIRTIO_MMIO_STATUS) & VIRTIO_CONFIG_S_FEATURES_OK)) {
        return;
    }

    *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
    *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

    *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (unsigned int)(unsigned long)disk.desc;
    *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (unsigned int)((unsigned long)disk.desc >> 32);
    *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (unsigned int)(unsigned long)&disk_avail;
    *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (unsigned int)((unsigned long)&disk_avail >> 32);
    *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (unsigned int)(unsigned long)&disk_used;
    *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (unsigned int)((unsigned long)&disk_used >> 32);

    *R(VIRTIO_MMIO_QUEUE_READY) = 1;
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    for (int i = 0; i < NUM; i++)
        disk.free[i] = 1;
    disk.used_idx = 0;

    uart_puts("[virtio] disk ready\n");
}

int virtio_blk_read(unsigned long sector, char *buf)
{
    int idx[3];
    for (int i = 0; i < 3; i++) {
        int found = -1;
        for (int j = 0; j < NUM; j++)
            if (disk.free[j]) { disk.free[j] = 0; found = j; break; }
        if (found < 0) return -1;
        idx[i] = found;
    }

    disk.ops[idx[0]].type     = VIRTIO_BLK_T_IN;
    disk.ops[idx[0]].reserved = 0;
    disk.ops[idx[0]].sector   = sector;

    disk.desc[idx[0]].addr  = (unsigned long)&disk.ops[idx[0]];
    disk.desc[idx[0]].len   = sizeof(struct virtio_blk_req);
    disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
    disk.desc[idx[0]].next  = idx[1];

    disk.desc[idx[1]].addr  = (unsigned long)buf;
    disk.desc[idx[1]].len   = 512;
    disk.desc[idx[1]].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    disk.desc[idx[1]].next  = idx[2];

    disk.status[idx[0]] = 0xff;
    disk.desc[idx[2]].addr  = (unsigned long)&disk.status[idx[0]];
    disk.desc[idx[2]].len   = 1;
    disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
    disk.desc[idx[2]].next  = 0;

    disk_avail.ring[disk_avail.idx % NUM] = idx[0];
    asm volatile("fence w,w");
    disk_avail.idx++;
    asm volatile("fence w,w");
    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

    // Busy-wait with compiler barrier: disk_used.idx is DMA-written by QEMU
    while (disk.used_idx == disk_used.idx)
        asm volatile("" ::: "memory");
    asm volatile("fence iorw,iorw" ::: "memory");

    int ret = (disk.status[idx[0]] == 0) ? 512 : -1;

    for (int i = 0; i < 3; i++)
        disk.free[idx[i]] = 1;
    disk.used_idx++;

    return ret;
}
