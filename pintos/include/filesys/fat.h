#ifndef FILESYS_FAT_H
#define FILESYS_FAT_H

#include "devices/disk.h"
#include "filesys/file.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint32_t cluster_t;  /* FAT 안의 cluster index. */

#define FAT_MAGIC 0xEB3C9000 /* FAT disk를 식별하는 MAGIC 문자열. */
#define EOChain 0x0FFFFFFF   /* cluster chain의 끝. */

/* FAT 정보가 들어 있는 섹터. */
#define SECTORS_PER_CLUSTER 1 /* cluster당 섹터 수. */
#define FAT_BOOT_SECTOR 0     /* FAT boot sector. */
#define ROOT_DIR_CLUSTER 1    /* 루트 디렉터리용 cluster. */

void fat_init (void);
void fat_open (void);
void fat_close (void);
void fat_create (void);
void fat_close (void);

cluster_t fat_create_chain (
    cluster_t clst /* 늘릴 cluster 번호, 0이면 새 chain을 만든다. */
);
void fat_remove_chain (
    cluster_t clst, /* 제거할 cluster 번호. */
    cluster_t pclst /* clst의 이전 cluster, 0이면 clst가 chain의 시작이다. */
);
cluster_t fat_get (cluster_t clst);
void fat_put (cluster_t clst, cluster_t val);
disk_sector_t cluster_to_sector (cluster_t clst);

#endif /* filesys/fat.h */
