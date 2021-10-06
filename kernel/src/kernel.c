/**
 * @file
 * @ingroup kernel_code
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License.
 *
 * @brief Código de inicialización del kernel en C
 *
 * Este codigo recibe el control de start.S y continúa con la ejecución.
 */
#include <asm.h>
#include <ata.h>

#include <console.h>

#include <ata.h>
#include <irq.h>
#include <kcache.h>
#include <kmem.h>
#include <pci.h>
#include <stdlib.h>
#include <string.h>

typedef struct __attribute__((packed)) {
  unsigned char active;
  unsigned char h_start;
  unsigned char s_start;
  unsigned char c_start;
  unsigned char type;
  unsigned char h_end;
  unsigned char s_end;
  unsigned char c_end;
  unsigned int lba;
  unsigned int size_lba;
} partition;

typedef struct __attribute__((packed)) {
  unsigned char loader[446];
  /* unsigned char partition_table[64]; */
  partition partition_table[4];
  unsigned short signature;
} bootsector;

typedef struct __attribute__((packed)) {
  unsigned int s_inodes_count;
  unsigned int s_blocks_count;
  unsigned int s_r_blocks_count;
  unsigned int s_free_blocks_count;
  unsigned int s_free_inodes_count;
  unsigned int s_first_data_block;
  unsigned int s_log_block_size;
  unsigned int s_log_frag_size;
  unsigned int s_blocks_per_group;
  unsigned int s_frags_per_group;
  unsigned int s_inodes_per_group;
  unsigned int s_mtime;
  unsigned int s_wtime;
  unsigned short s_mnt_count;
  unsigned short s_max_mnt_count;
  unsigned short s_magic;
  unsigned short s_state;
  unsigned short s_errors;
  unsigned short s_minor_rev_level;
  unsigned int s_lastcheck;
  unsigned int s_checkinterval;
  unsigned int s_creator_os;
  unsigned int s_rev_level;
  unsigned short s_def_resuid;
  unsigned short s_def_resgid;
  unsigned int s_first_ino;
  unsigned short s_inode_size;
  unsigned short s_block_group_nr;
  unsigned int s_feature_compat;
  unsigned int s_feature_incompat;
  unsigned int s_feature_ro_compat;
  char s_uuid[16];
  char s_volume_name[16];
  char s_last_mounted[64];
  unsigned int s_algo_bitmap;
  char s_prealloc_blocks;
  char s_prealloc_dir_blocks;
  unsigned short s_aligment;
  char s_journal_uuid[16];
  unsigned int s_journal_inum;
  unsigned int s_journal_dev;
  unsigned int s_last_orphan;
  char s_hash_seed[4][4];
  char s_def_hash_version;
  char s_padding[3];
  unsigned int s_default_mount_options;
  unsigned int s_first_meta_bg;
  char s_unused[760];
} superblock;

void cmain() {

  char buf[512];
  char super[1024];

  /* Inicializar y limpiar la consola console.c*/
  console_clear();

  /* Inicializar la estructura para gestionar la memoria física. physmem.c*/
  setup_physical_memory();

  /* Configura la IDT y el PIC.interrupt.c */
  setup_interrupts();

  /* Completa la configuración de la memoria virtual. paging.c*/
  setup_paging();

  /* Detecta los dispositivos PCI conectados. pci.c */
  pci_detect();

  /* Inicializar la memoria virtual para el kernel. kmem.c. */
  setup_kmem();

  /* Inicializar las estructuras de datos para dispostivos ATA. ata.c.*/
  setup_ata();

  console_printf("ATA device count: %d\n", ata_get_device_count());

  /* Suponer que el primer dispositivo ATA existe. */
  ata_device *dev = ata_get_device(0);

  if (!dev->present) {
    console_printf("ATA device not present!\n");
    return;
  }

  int res;

  /* Leer a buf del primer dispositivo el sector 0 (1 sector). */
  res = ata_read(dev, buf, 0, 1);

  if (res == -1) {
    console_printf("Unable to read from ATA Device!\n");
    return;
  } else {
    console_printf("ATA read successful.\n");
  }

  bootsector *bs = (bootsector *)buf;

  int i;

  for (i = 0; i < 4; i++) {
    if (bs->partition_table[i].active != 0x0) {
      if (bs->partition_table[i].type == 0x83) {

        unsigned char h_start = bs->partition_table[i].h_start;
        unsigned char h_end = bs->partition_table[i].h_end;
        unsigned char s_start = bs->partition_table[i].s_start;
        unsigned char s_end = bs->partition_table[i].s_end;
        unsigned char c_start = bs->partition_table[i].c_start;
        unsigned char c_end = bs->partition_table[i].c_end;

        unsigned int s_start_dec = (s_start & 111111);
        unsigned int c_start_dec = ((s_start >> 6) << 8) | c_start;

        console_printf("Decodificado s: %d\n", s_start_dec);
        console_printf("Decodificado c: %d\n", c_start_dec);

        unsigned int start_value;
        start_value = c_start_dec * 63 * 16 + s_start_dec * 63 + (h_start - 1);

        console_printf("%d\n", start_value);

        console_printf("h start: %x\n", bs->partition_table[i].h_start);
        console_printf("h end: %x\n", bs->partition_table[i].h_end);
        console_printf("s start: %x\n", bs->partition_table[i].s_start);
        console_printf("s end: %x\n", bs->partition_table[i].s_end);
        console_printf("c start: %x\n", bs->partition_table[i].c_start);
        console_printf("c end: %x\n", bs->partition_table[i].c_end);

        /* Leer a buf del primer dispositivo el sector 0 (1 sector). */
        res = ata_read(dev, super, start_value + 2, 2);

        superblock *sblock = (superblock *)super;

        console_printf("Super size: %d\n", sizeof(*sblock));

        if (res == -1) {
          console_printf("Unable to read from ATA Device!\n");
          return;
        } else {
          console_printf("ATA read successful.\n");
        }

        console_printf("s_inodes_count: %d\n", sblock->s_inodes_count);
        console_printf("s_blocks_count: %d\n", sblock->s_blocks_count);
        console_printf("s_r_blocks_count: %d\n", sblock->s_r_blocks_count);
        console_printf("s_free_blocks_count: %d\n",
                       sblock->s_free_blocks_count);
      }
    }
  }
}
