/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2014 sTeeL<steel.mental@gmail.com> - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include <stdio.h>
#include <console.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getkey.h>
#include <disk/common.h>
#include <disk/geom.h>
#include <disk/util.h>
#include <disk/errno_disk.h>
#include <disk/error.h>
#include <syslinux/reboot.h>

#define __DWIPE_VERSION__ "1.0.0.0"

char boot_code[512] = {
0xeb,0x4f,0x64,0x69,0x73,0x6b,0x20,0x68,0x61,0x73,0x20,0x62,0x65,0x65,0x6e,0x20,
0x77,0x69,0x70,0x70,0x65,0x64,0x20,0x77,0x69,0x74,0x68,0x20,0x64,0x77,0x69,0x70,
0x65,0x3c,0x73,0x74,0x65,0x65,0x6c,0x2e,0x6d,0x65,0x6e,0x74,0x61,0x6c,0x40,0x67,
0x6d,0x61,0x69,0x6c,0x2e,0x63,0x6f,0x6d,0x3e,0xd,0xa,0x68,0x69,0x74,0x20,0x61,
0x6e,0x79,0x20,0x6b,0x65,0x79,0x20,0x74,0x6f,0x20,0x72,0x65,0x62,0x6f,0x6f,0x74,
0x0,0x31,0xc0,0x8e,0xd8,0xfc,0xbe,0x2,0x7c,0xe9,0x7,0x0,0xbb,0x1,0x0,0xb4,
0xe,0xcd,0x10,0xac,0x3c,0x0,0x75,0xf4,0xb8,0x0,0x0,0xcd,0x16,0xb0,0xfe,0xe6,
0x64,
};


/*
 * 100%|>>>>>>>>    |
 *
 */
static void print_progress(int progress, int cols)
{
    int pc1 = progress * (cols - 14) / 100; /* how many >? */
    int pc2 = (cols - 14) - pc1; /* how many =? */
    int i;

    printf("\r");
    printf("WIPING %3d%%|", progress);
    for(i = 0 ; i < pc1; i ++) {
        printf(">");
    }
    for(i = 0 ; i < pc2; i ++) {
        printf(" ");
    }
    printf("|");
}

/** write_sectors in write.c has BUG!!!
 * write_sectors - write several sectors from disk
 * @drive_info:		driveinfo struct describing the disk
 * @lba:		Position to write
 * @data:		Buffer to write
 * @size:		Size of the buffer (number of sectors)
 *
 * Return the number of sectors write on success or -1 on failure.
 * errno_disk contains the error number.
 **/
static int dwipe_write_sectors(const struct driveinfo *drive_info, const unsigned int lba,
		  const void *data, const int size)
{
    com32sys_t inreg, outreg;
    struct ebios_dapa *dapa = __com32.cs_bounce;
    void *buf = (char *)__com32.cs_bounce + size * SECTOR;

    memcpy(buf, data, size * SECTOR);
    memset(&inreg, 0, sizeof inreg);

    if (drive_info->ebios) {
	dapa->len = sizeof(*dapa);
	dapa->count = size;
	dapa->off = OFFS(buf);
	dapa->seg = SEG(buf);
	dapa->lba = lba;

	inreg.esi.w[0] = OFFS(dapa);
	inreg.ds = SEG(dapa);
	inreg.edx.b[0] = drive_info->disk;
	inreg.eax.w[0] = 0x4300;	/* Extended write */
    } else {
	unsigned int c, h, s;

	if (!drive_info->cbios) {	// XXX errno
	    /* We failed to get the geometry */
	    if (lba)
		return -1;	/* Can only write MBR */

	    s = 1;
	    h = 0;
	    c = 0;
	} else
	    lba_to_chs(drive_info, lba, &s, &h, &c);

	// XXX errno
	if (s > 63 || h > 256 || c > 1023)
	    return -1;

	inreg.eax.w[0] = 0x0301;	/* Write one sector */
	inreg.ecx.b[1] = c & 0xff;
	inreg.ecx.b[0] = s + (c >> 6);
	inreg.edx.b[1] = h;
	inreg.edx.b[0] = drive_info->disk;
	inreg.ebx.w[0] = OFFS(buf);
	inreg.es = SEG(buf);
    }

    /* Perform the write */
    if (int13_retry(&inreg, &outreg)) {
	errno_disk = outreg.eax.b[1];
	return -1;		/* Give up */
    } else
	return size;
}

int main(int argc, char *argv[])
{
	struct driveinfo drive;
	struct driveinfo *d = &drive;
    char buffer[4096] = {0};
    unsigned int index = 0;
    int ret, progress, progress_old;
    int rows, cols;

	(void)argc;
	(void)argv;

	openconsole(&dev_stdcon_r, &dev_stdcon_w);

    if (getscreensize(1, &rows, &cols)) {
        /* Unknown screen size? */
        rows = 24;
        cols = 80;
    }
    memset(buffer, 'A', 4096);

    memcpy(buffer, boot_code, sizeof(boot_code));  
    buffer[510] = 0x55;
    buffer[511] = 0xaa;
    
    printf("DWIPE version %s, by sTeeL <steel.mental@gmail.com>\n", __DWIPE_VERSION__);

    printf("remove USB DISK and hit ENTER key\n");

    get_key(stdin, 0);

	for (int disk = 0x80; disk < 0xff; disk++) {
		memset(d, 0, sizeof(struct driveinfo));
		d->disk = disk;
		get_drive_parameters(d);

		/* Do not print output when drive does not exists */
		if (errno_disk == -1 || !d->cbios)
			continue;

		if (errno_disk) {
			get_error("reading disk");
			continue;
		}

		printf("DISK 0x%X:\n", d->disk);
		printf("  C/H/S: %d heads, %d cylinders\n",
			d->legacy_max_head + 1, d->legacy_max_cylinder + 1);
		printf("         %d sectors/track, %d drives\n",
			d->legacy_sectors_per_track, d->legacy_max_drive);
		printf("  EDD:   ebios=%d, EDD version: %X\n",
			d->ebios, d->edd_version);
		printf("         %d heads, %d cylinders\n",
			(int) d->edd_params.heads, (int) d->edd_params.cylinders);
		printf("         %d sectors, %d bytes/sector, %d sectors/track\n",
			(int) d->edd_params.sectors, (int) d->edd_params.bytes_per_sector,
			(int) d->edd_params.sectors_per_track);
		printf("         Host bus: %s, Interface type: %s\n\n",
			d->edd_params.host_bus_type, d->edd_params.interface_type);

        progress_old = 0;

        print_progress(0, cols);
        for(index = 0; index < d->edd_params.sectors ; index += 8 ) {
            ret = dwipe_write_sectors(d, index, buffer, 8);
            if(ret == -1) {
                printf("ERROR!\n");
                continue;
            } else {
                progress = index * 100 / d->edd_params.sectors;
                if(progress != progress_old) {
                    progress_old = progress;
                    print_progress(progress, cols);
                }
            }
        }
        print_progress(100, cols);
        printf("\nDONE\n\n");

        
	}
    syslinux_reboot(0);
	return 0;
}
