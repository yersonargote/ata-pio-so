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

// Decidifica los valores S y C de la particion.
void decodeSC(unsigned char s, unsigned char c, unsigned int *s_decode,
              unsigned int *c_decode) {
  *s_decode = (s & 0x3F);
  *c_decode = ((s & 0xC0) << 2) | c;
}

// Obtener donde empieza la particion
unsigned int partition_start(partition part_tbl) {

  console_printf("-------------------------");
  console_printf("Partition table information");
  console_printf("-------------------------\n");

  // Variable que almacena el calculo del inicio de la particion.
  unsigned int start_value;

  // Variables que almacenan los valores de C/H/S de inicio y de fin.
  unsigned char h_start = part_tbl.h_start;
  unsigned char h_end = part_tbl.h_end;
  unsigned char s_start = part_tbl.s_start;
  unsigned char s_end = part_tbl.s_end;
  unsigned char c_start = part_tbl.c_start;
  unsigned char c_end = part_tbl.c_end;

  // Validacion que determina si la particion inicia o termina por fuera
  // del cilindro 1024.
  if ((c_start == 0xFE && h_start == 0xFF && s_start == 0xFF) ||
      (c_end == 0xFE && h_end == 0xFF && s_end == 0xFF)) {
    start_value = part_tbl.lba;
  } else {
    // Variables que almacenan los valores de S y C decodificados.
    unsigned int s_start_dec;
    unsigned int c_start_dec;
    unsigned int s_end_dec;
    unsigned int c_end_dec;

    decodeSC(s_start, c_start, &s_start_dec, &c_start_dec);
    decodeSC(s_end, c_end, &s_end_dec, &c_end_dec);

    console_printf("Start: ");
    console_printf("c=%d h=%d s=%d\n", c_start_dec, h_start, s_start_dec);
    console_printf("End: ");
    console_printf("c=%d h=%d s=%d\n", c_end_dec, h_end, s_end_dec);

    start_value = (c_start_dec * 63 * 16) + (h_start * 63) + (s_start_dec - 1);
  }

  console_printf("Active: %x\n", part_tbl.active);
  console_printf("Type: %x\n", part_tbl.type);
  console_printf("LBA start: %d\n", part_tbl.lba);
  console_printf("LBA size: %d\n", part_tbl.size_lba);

  console_printf("Partition start in sector: %d\n", start_value);
  return start_value;
}

// Imprimir la informacion del superbloque
void print_superblock_info(superblock *sblock) {
  console_printf("----------------------------");
  console_printf("Superblock information");
  console_printf("----------------------------\n");
  console_printf("Superblock size: %d\n", sizeof(*sblock));
  console_printf("s_inodes_count: %d - ", sblock->s_inodes_count);
  console_printf("s_blocks_count: %d - ", sblock->s_blocks_count);
  console_printf("s_r_blocks_count: %d\n", sblock->s_r_blocks_count);
  console_printf("s_free_blocks_count: %d - ", sblock->s_free_blocks_count);
  console_printf("s_log_block_size: %d - ", sblock->s_log_block_size);
  console_printf("block size: %d\n", 1024 << sblock->s_log_block_size);
  console_printf("s_log_frag_size: %d - ", sblock->s_log_frag_size);
  console_printf("s_inodes_per_group: %d - ", sblock->s_inodes_per_group);
  console_printf("s_max_mnt_count: %d\n", sblock->s_max_mnt_count);
  console_printf("s_state: %d - ", sblock->s_state);
  console_printf("s_creator_os: %d\n", sblock->s_creator_os);
  console_printf("s_first_ino: %d - ", sblock->s_first_ino);
  console_printf("s_inode_size: %d\n", sblock->s_inode_size);
}

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
    partition part_tbl = (partition)bs->partition_table[i];

    if (part_tbl.active != NO_ACTIVE) {
      if (part_tbl.type == LINUX_TYPE) {

        unsigned int start_value;

        start_value = partition_start(part_tbl);

        // Casting del superbloque
        superblock *sblock = (superblock *)super;

        res = ata_read(dev, super, start_value + 2, 2);

        if (res == -1) {
          console_printf("Unable to read from ATA Device!\n");
          return;
        }

        print_superblock_info(sblock);
      }
    }
  }
}
