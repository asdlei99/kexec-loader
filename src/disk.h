/* kexec-loader - Disk header
 * Copyright (C) 2007-2009 Daniel Collins <solemnwarning@solemnwarning.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef KL_DISK_H
#define KL_DISK_H

#define INIT_DISK(ptr) \
	(ptr)->next = NULL; \
	(ptr)->name[0] = '\0'; \
	(ptr)->major = -1; \
	(ptr)->minor = -1; \
	(ptr)->label[0] = '\0'; \
	(ptr)->uuid[0] = '\0'; \
	(ptr)->fstype[0] = '\0';

typedef struct kl_disk {
	struct kl_disk *next;
	
	char name[32];
	int major;
	int minor;
	
	char label[256];
	char uuid[256];
	char fstype[32];
} kl_disk;

kl_disk *get_disks(void);
kl_disk *find_disk(char const *id);
char const *mount_disk(kl_disk *disk, char const *mpoint);
void unmount_all(void);

#endif /* !KL_DISK_H */
