// kernel/fat.c — FAT12/16/32 file system reader (disk-I/O decoupled)

void uart_puts(const char *s);
void uart_puthex(unsigned long x);
void uart_putdec(int n);
void uart_putc(char c);

static int (*fat_disk_read)(unsigned long sector, char *buf) = 0;
static int fat_ready = 0;

static unsigned short bytes_per_sec;
static unsigned char  sec_per_cluster;
static unsigned short reserved_sec;
static unsigned char  num_fats;
static unsigned short root_ent_cnt;
static unsigned short sec_per_fat_16;
static unsigned int   sec_per_fat_32;
static unsigned int   root_cluster;
static unsigned int   fat_start;
static unsigned int   data_start;
static unsigned int   total_sectors;
static int            fat_type;
static unsigned int   root_dir_sectors;

#define MAX_CLUSTER_CACHE 16
static unsigned long cluster_cache[MAX_CLUSTER_CACHE];
static int    cache_cluster[MAX_CLUSTER_CACHE];
static int    cache_used = 0;

static char secbuf[512] __attribute__((aligned(4)));
static unsigned long part_offset = 0;  // 0 = no partition, else MBR LBA

static void fat_cache_init(void)
{
    for (int i = 0; i < MAX_CLUSTER_CACHE; i++)
        cache_cluster[i] = -1;
    cache_used = 0;
}

static unsigned long fat_get(unsigned long cluster)
{
    for (int i = 0; i < cache_used; i++)
        if (cache_cluster[i] == (int)cluster)
            return cluster_cache[i];

    if (fat_type == 32) {
        unsigned long fat_offset = cluster * 4;
        fat_disk_read(fat_start + fat_offset / 512, secbuf);
        unsigned long val = *(unsigned int*)(secbuf + fat_offset % 512) & 0x0FFFFFFF;
        if (cache_used < MAX_CLUSTER_CACHE) {
            cache_cluster[cache_used] = (int)cluster;
            cluster_cache[cache_used] = val;
            cache_used++;
        }
        return val;
    } else if (fat_type == 16) {
        unsigned long fat_offset = cluster * 2;
        fat_disk_read(fat_start + fat_offset / 512, secbuf);
        unsigned long val = *(unsigned short*)(secbuf + fat_offset % 512);
        if (val >= 0xFFF8) val |= 0xFFFF0000;
        if (cache_used < MAX_CLUSTER_CACHE) {
            cache_cluster[cache_used] = (int)cluster;
            cluster_cache[cache_used] = val;
            cache_used++;
        }
        return val;
    } else {
        unsigned long fat_offset = cluster + cluster / 2;
        fat_disk_read(fat_start + fat_offset / 512, secbuf);
        unsigned short val = *(unsigned short*)(secbuf + fat_offset % 512);
        if (cluster & 1) val >>= 4;
        else val &= 0x0FFF;
        if (val >= 0xFF8) val |= 0xFFFFF000;
        return val;
    }
}

static void read_cluster(unsigned long cluster, char *buf)
{
    unsigned long sector = data_start + (cluster - 2) * sec_per_cluster;
    for (int i = 0; i < sec_per_cluster; i++)
        fat_disk_read(sector + i, buf + i * 512);
}

void fat_init(int (*read_fn)(unsigned long sector, char *buf))
{
    fat_disk_read = read_fn;
    fat_ready = 0;
    fat_cache_init();
    part_offset = 0;

    // Read sector 0: check MBR or direct BPB
    if (fat_disk_read(0, secbuf) < 0) {
        uart_puts("[fat] read sector 0 failed\n");
        return;
    }

    int bpb_ok = 0;

    // Case 1: Sector 0 IS the BPB (no-mbr / floppy)
    if (((unsigned char)secbuf[0] == 0xEB && (unsigned char)secbuf[2] == 0x90)
        || (unsigned char)secbuf[0] == 0xE9) {
        uart_puts("[fat] BPB at sector 0\n");
        bpb_ok = 1;
    }
    // Case 2: Sector 0 is MBR, get partition LBA
    else if (secbuf[510] == (char)0x55 && secbuf[511] == (char)0xAA) {
        unsigned int lba = *(unsigned int*)(secbuf + 0x1BE + 8);
        uart_puts("[fat] MBR, partition at LBA=");
        uart_putdec((int)lba); uart_puts("\n");
        if (lba > 0 && lba < 2048) {
            if (fat_disk_read(lba, secbuf) >= 0) {
                if (((unsigned char)secbuf[0] == 0xEB && (unsigned char)secbuf[2] == 0x90)
                    || (unsigned char)secbuf[0] == 0xE9) {
                    part_offset = lba;
                    bpb_ok = 1;
                }
            }
        }
    }

    if (!bpb_ok) {
        uart_puts("[fat] no BPB\n");
        return;
    }

    // Parse BPB from secbuf (already contains BPB sector data)

    bytes_per_sec  = *(unsigned short*)(secbuf + 11);
    sec_per_cluster = *(unsigned char*)(secbuf + 13);
    reserved_sec   = *(unsigned short*)(secbuf + 14);
    num_fats       = *(unsigned char*)(secbuf + 16);

    uart_puts("[fat] bps="); uart_putdec(bytes_per_sec);
    uart_puts(" spc="); uart_putdec(sec_per_cluster);
    uart_puts(" rsv="); uart_putdec(reserved_sec);
    uart_puts(" nf="); uart_putdec(num_fats);
    uart_puts("\n");
    root_ent_cnt   = *(unsigned short*)(secbuf + 17);
    sec_per_fat_16 = *(unsigned short*)(secbuf + 22);
    total_sectors  = *(unsigned short*)(secbuf + 19);
    if (total_sectors == 0)
        total_sectors = *(unsigned int*)(secbuf + 32);

    uart_puts("[fat] root_ent="); uart_putdec(root_ent_cnt);
    uart_puts(" total_sec="); uart_putdec((int)total_sectors);
    uart_puts("\n");

    if (sec_per_cluster == 0 || bytes_per_sec == 0) {
        uart_puts("[fat] BAD BPB (spc or bps is 0)\n");
        return;
    }

    unsigned int root_sectors = ((root_ent_cnt * 32) + (bytes_per_sec - 1)) / bytes_per_sec;
    root_dir_sectors = root_sectors;
    unsigned int fat_sz_16 = (sec_per_fat_16 != 0) ? sec_per_fat_16 : *(unsigned int*)(secbuf + 36);
    unsigned int data_sec = total_sectors - (reserved_sec + num_fats * fat_sz_16 + root_sectors);
    unsigned int count_clusters = data_sec / sec_per_cluster;

    uart_puts("[fat] data_sec="); uart_putdec((int)data_sec);
    uart_puts(" clusters="); uart_putdec((int)count_clusters);
    uart_puts("\n");

    if (count_clusters < 4085) fat_type = 12;
    else if (count_clusters < 65525) fat_type = 16;
    else fat_type = 32;

    if (fat_type == 32) {
        sec_per_fat_32 = *(unsigned int*)(secbuf + 36);
        root_cluster   = *(unsigned int*)(secbuf + 44);
        fat_start = part_offset + reserved_sec;
        data_start = part_offset + reserved_sec + num_fats * sec_per_fat_32;
    } else {
        fat_start = part_offset + reserved_sec;
        data_start = part_offset + reserved_sec + num_fats * fat_sz_16 + root_sectors;
    }

    fat_ready = 1;
    uart_puts("[fat] ready, type=FAT");
    uart_putdec(fat_type);
    uart_puts(" off="); uart_puthex(part_offset);
    uart_puts(" data="); uart_puthex(data_start);
    uart_puts("\n");
}

int fat_find(const char *name, unsigned long *start_cluster, unsigned int *file_size)
{
    if (!fat_ready) return -1;

    char fatname[11];
    for (int i = 0; i < 11; i++) fatname[i] = ' ';
    int ni = 0;
    for (int i = 0; name[i] && name[i] != '.' && ni < 8; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        fatname[ni++] = c;
    }
    const char *dot = 0;
    for (int i = 0; name[i]; i++) if (name[i] == '.') { dot = &name[i + 1]; break; }
    if (dot) {
        ni = 8;
        for (int i = 0; dot[i] && ni < 11; i++) {
            char c = dot[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            fatname[ni++] = c;
        }
    }

    if (fat_type == 32) {
        unsigned long cluster = root_cluster;
        int loops = 0;
        while (cluster >= 2 && cluster < 0x0FFFFFF8 && loops++ < 100) {
            static char fat_cluster_buf[32768];
            read_cluster(cluster, fat_cluster_buf);
            unsigned int clus_secs = (unsigned int)sec_per_cluster * 512;
            for (unsigned int off = 0; off < clus_secs; off += 32) {
                unsigned char attr = fat_cluster_buf[off + 11];
                if (attr == 0x0F || (attr & 0x08)) continue;
                if (fat_cluster_buf[off] == 0) { cluster = 0x0FFFFFFF; break; }
                if (fat_cluster_buf[off] == 0xE5) continue;

                int match = 1;
                for (int i = 0; i < 11; i++)
                    if (fatname[i] != '?' && fat_cluster_buf[off + i] != fatname[i])
                        { match = 0; break; }
                if (!match) continue;

                unsigned int hi = *(unsigned short*)(fat_cluster_buf + off + 20);
                unsigned int lo = *(unsigned short*)(fat_cluster_buf + off + 26);
                *start_cluster = ((unsigned long)hi << 16) | lo;
                *file_size = *(unsigned int*)(fat_cluster_buf + off + 28);
                return 0;
            }
            cluster = fat_get(cluster);
        }
    } else {
        unsigned int root_start = part_offset + reserved_sec + num_fats * sec_per_fat_16;

        for (unsigned int s = 0; s < root_dir_sectors; s++) {
            int ret = fat_disk_read(root_start + s, secbuf);
            if (ret < 0) {
                break;
            }
            for (unsigned int off = 0; off < 512; off += 32) {
                unsigned char attr = secbuf[off + 11];
                if (attr == 0x0F || (attr & 0x08)) continue;
                if (secbuf[off] == 0) { s = root_dir_sectors; break; }
                if (secbuf[off] == 0xE5) continue;

                int match = 1;
                for (int i = 0; i < 11; i++)
                    if (fatname[i] != '?' && secbuf[off + i] != fatname[i])
                        { match = 0; break; }
                if (!match) continue;

                *start_cluster = *(unsigned short*)(secbuf + off + 26);
                *file_size = *(unsigned int*)(secbuf + off + 28);
                return 0;
            }
        }
    }
    return -1;
}

// fat_list: walk root dir and print all entries (like ls, matches fs_list format)
void fat_list(void)
{
    if (!fat_ready) return;

    int count = 0;
    unsigned int root_start = part_offset + reserved_sec + num_fats * sec_per_fat_16;

    // Count pass
    if (fat_type == 32) {
        unsigned long cluster = root_cluster;
        int loops = 0;
        static char clus_buf[32768];
        while (cluster >= 2 && cluster < 0x0FFFFFF8 && loops++ < 100) {
            read_cluster(cluster, clus_buf);
            unsigned int limit = (unsigned int)sec_per_cluster * 512;
            for (unsigned int off = 0; off < limit; off += 32) {
                unsigned char attr = clus_buf[off + 11];
                if (attr == 0x0F || (attr & 0x08)) continue;
                if (clus_buf[off] == 0) { cluster = 0x0FFFFFFF; break; }
                if (clus_buf[off] == 0xE5) continue;
                count++;
            }
            cluster = fat_get(cluster);
        }
    } else {
        for (unsigned int s = 0; s < root_dir_sectors; s++) {
            if (fat_disk_read(root_start + s, secbuf) < 0) break;
            for (unsigned int off = 0; off < 512; off += 32) {
                unsigned char attr = secbuf[off + 11];
                if (attr == 0x0F || (attr & 0x08)) continue;
                if (secbuf[off] == 0) { s = root_dir_sectors; break; }
                if (secbuf[off] == 0xE5) continue;
                count++;
            }
        }
    }

    uart_puts("\n=== FAT Disk Files (");
    uart_putdec(count);
    uart_puts(" files) ===\n");

    // Print pass
    if (fat_type == 32) {
        unsigned long cluster = root_cluster;
        int loops = 0;
        static char clus_buf2[32768];
        while (cluster >= 2 && cluster < 0x0FFFFFF8 && loops++ < 100) {
            read_cluster(cluster, clus_buf2);
            unsigned int limit = (unsigned int)sec_per_cluster * 512;
            for (unsigned int off = 0; off < limit; off += 32) {
                unsigned char attr = clus_buf2[off + 11];
                if (attr == 0x0F || (attr & 0x08)) continue;
                if (clus_buf2[off] == 0) { cluster = 0x0FFFFFFF; break; }
                if (clus_buf2[off] == 0xE5) continue;
                uart_puts("  ");
                for (int i = 0; i < 8 && clus_buf2[off + i] != ' '; i++)
                    uart_putc(clus_buf2[off + i]);
                if (clus_buf2[off + 8] != ' ') {
                    uart_putc('.');
                    for (int i = 0; i < 3 && clus_buf2[off + 8 + i] != ' '; i++)
                        uart_putc(clus_buf2[off + 8 + i]);
                }
                unsigned int sz = *(unsigned int*)(clus_buf2 + off + 28);
                uart_puts("  ("); uart_putdec((int)sz); uart_puts(" bytes)\n");
            }
            cluster = fat_get(cluster);
        }
    } else {
        for (unsigned int s = 0; s < root_dir_sectors; s++) {
            if (fat_disk_read(root_start + s, secbuf) < 0) break;
            for (unsigned int off = 0; off < 512; off += 32) {
                unsigned char attr = secbuf[off + 11];
                if (attr == 0x0F || (attr & 0x08)) continue;
                if (secbuf[off] == 0) { s = root_dir_sectors; break; }
                if (secbuf[off] == 0xE5) continue;
                uart_puts("  ");
                for (int i = 0; i < 8 && secbuf[off + i] != ' '; i++)
                    uart_putc(secbuf[off + i]);
                if (secbuf[off + 8] != ' ') {
                    uart_putc('.');
                    for (int i = 0; i < 3 && secbuf[off + 8 + i] != ' '; i++)
                        uart_putc(secbuf[off + 8 + i]);
                }
                unsigned int sz = *(unsigned int*)(secbuf + off + 28);
                uart_puts("  ("); uart_putdec((int)sz); uart_puts(" bytes)\n");
            }
        }
    }
    uart_puts("===============================\n\n");
}

int fat_read(unsigned long cluster, unsigned int offset,
               char *buf, int maxlen, unsigned int file_size)
{
    if (!fat_ready) return -1;
    if (offset >= file_size) return 0;
    if (offset + (unsigned int)maxlen > file_size)
        maxlen = (int)(file_size - offset);

    unsigned long clus_size = (unsigned long)sec_per_cluster * 512;
    while (offset >= clus_size) {
        cluster = fat_get(cluster);
        if (cluster < 2 || cluster >= 0x0FFFFFF8) return -1;
        offset -= clus_size;
    }

    int total = 0;
    static char cluster_buf[32768];  // max 64 sectors/cluster
    while (total < maxlen && cluster >= 2 && cluster < 0x0FFFFFF8) {
        read_cluster(cluster, cluster_buf);
        unsigned int chunk = (unsigned int)clus_size - offset;
        if (chunk > (unsigned int)(maxlen - total))
            chunk = (unsigned int)(maxlen - total);
        for (unsigned int i = 0; i < chunk; i++)
            buf[total + i] = cluster_buf[offset + i];
        total += chunk;
        offset = 0;
        cluster = fat_get(cluster);
    }
    return total;
}
