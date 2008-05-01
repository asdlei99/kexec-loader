/* kexec-loader - Configuration functions
 * Copyright (C) 2007, Daniel Collins <solemnwarning@solemnwarning.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *
 *	* Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 *
 *	* Neither the name of the software author nor the names of any
 *	  contributors may be used to endorse or promote products derived from
 *	  this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE SOFTWARE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE SOFTWARE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>

#include "config.h"
#include "../config.h"
#include "misc.h"
#include "mount.h"
#include "console.h"

struct kl_config config = CONFIG_DEFAULTS_DEFINE;
static struct kl_target target = TARGET_DEFAULTS_DEFINE;

/* Add a mount
 * This changes the device string passed to it
*/
static void config_add_mount(unsigned int lnum, char* device, char* mpoint) {
	char* fstype = "auto";
	
	if(strchr(device, ':')) {
		fstype = device;
		device = strchr(device, ':');
		device[0] = '\0';
		device++;
	}
	
	kl_mount *nptr = malloc(sizeof(kl_mount));
	if(!nptr) {
		printD("config:%u: Can't allocate memory", lnum);
		
		return;
	}
	MOUNT_DEFAULTS(nptr);
	
	strncpy(nptr->device, device, DEVICE_SIZE-1);
	strncpy(nptr->mpoint, "/mnt/", MPOINT_SIZE-1);
	strncpy(nptr->fstype, fstype, 63);
	
	char *mptok = strtok(mpoint, "/");
	while(mptok) {
		if(strlen(mptok) == 0) {
			goto NTOK;
		}
		
		strncat(nptr->mpoint, mptok, MPOINT_SIZE);
		strncat(nptr->mpoint, "/", MPOINT_SIZE);
		nptr->depth++;
		
		NTOK:
		mptok = strtok(NULL, "/");
	}
	
	debug(
		"Adding mount '%s' => '%s', depth %d\n",
		nptr->device, nptr->mpoint, nptr->depth
	);
	
	nptr->next = target.mounts;
	target.mounts = nptr;
}

/* Add a target to config.targets */
static void cfg_add_target(void) {
	if(target.kernel[0] == '\0') {
		printD("Target %s has no kernel, not adding", target.name);
		goto END;
	}
	if(!target.mounts) {
		printD("Target %s has no mounts, not adding", target.name);
		goto END;
	}
	
	kl_target *nptr = malloc(sizeof(kl_target));
	if(!nptr) {
		printD("Can't malloc() %u bytes\n", sizeof(kl_target));
		goto END;
	}
	
	TARGET_DEFAULTS(nptr);
	
	strcpy(nptr->name, target.name);
	nptr->flags = target.flags;
	
	strcpy(nptr->kernel, target.kernel);
	strcpy(nptr->initrd, target.initrd);
	strcpy(nptr->append, target.append);
	nptr->mounts = target.mounts;
	
	if(!config.targets) {
		config.targets = nptr;
	}else{
		kl_target *eptr = config.targets;
		while(eptr && eptr->next) {
			eptr = eptr->next;
		}
		
		eptr->next = nptr;
	}
	
	END:
	TARGET_DEFAULTS(&target);
}

static char *next_value(char *value) {
	char *rval = value+strcspn(value, " \t\r\n");
	
	if(rval[0] != '\0') {
		rval[0] = '\0';
		rval++;
		
		rval = rval+strspn(rval, " \t");
		rval[strcspn(rval, "\r\n")] = '\0';
	}
	
	return rval;
}

/* Read configuration file, one line at a time and call config_parse() for each
 * individual line, in the event of an error an incomplete configuration may,
 * or may not be loaded.
*/
void config_load(void) {
	if(!mount_config()) {
		return;
	}
	
	TARGET_DEFAULTS(&target);
	
	FILE* cfg_handle = fopen("/mnt/" CONFIG_FILE, "r");
	if(!cfg_handle) {
		printD("Can't open " CONFIG_FILE ": %s", strerror(errno));
		
		goto UMOUNT;
	}
	
	config.timeout = 0;
	config.grub_config = NULL;
	config.grub_devices = NULL;
	config.grub_first = hdx;
	config.targets = NULL; /* BUG: Memory leak! */
	
	unsigned int lnum = 1;
	char line[STACK_BUF] = {'\0'};
	while(fgets(line, STACK_BUF, cfg_handle) != NULL) {
		config_parse(line, lnum++);
	}
	
	config_finish();
	
	while(fclose(cfg_handle) != 0) {
		if(errno == EINTR) {
			continue;
		}
		
		debug("Can't close " CONFIG_FILE ": %s\n", strerror(errno));
		debug("Discarding cfg_handle!\n");
		return;
	}
	
	UMOUNT:
	if(umount("/mnt") == -1) {
		debug("Can't unmount /mnt: %s\n", strerror(errno));
	}
}

/* Parse a single line from the configuration file, handles newlines, spaces,
 * tabs and carridge-return characters.
*/
void config_parse(char* line, unsigned int lnum) {
	char *name = line+strspn(line, " \t\r\n");
	char *value = next_value(name);
	
	/* Skip line if it's a comment, or empty */
	if(name[0] == '#' || name[0] == '\0') {
		return;
	}
	
	debug("config:%u: '%s' = '%s'\n", lnum, name, value);
	
	if(str_compare(name, "timeout", STR_NOCASE)) {
		config.timeout = strtoul(value, NULL, 10);
		
		return;
	}
	if(str_compare(name, "title", STR_NOCASE)) {
		if(target.name[0] != '\0') {
			cfg_add_target();
		}
		
		if(value[0] == '\0') {
			printD("config:%u: Title requires an argument", lnum);
			return;
		}
		
		strncpy(target.name, value, NAME_SIZE-1);
		
		return;
	}
	if(str_compare(name, "kernel", STR_NOCASE)) {
		if(value[0] == '\0') {
			printD("config:%u: Kernel requires an argument", lnum);
			return;
		}
		
		snprintf(target.kernel, KERNEL_SIZE, "/mnt/%s", value);
		
		return;
	}
	if(str_compare(name, "initrd", STR_NOCASE)) {
		if(value[0] == '\0') {
			printD("config:%u: initrd requires an argument", lnum);
			return;
		}
		
		snprintf(target.initrd, INITRD_SIZE, "/mnt/%s", value);
		
		return;
	}
	if(str_compare(name, "append", STR_NOCASE)) {
		if(value[0] == '\0') {
			printD("config:%u: Append requires an argument", lnum);
			return;
		}
		
		strncpy(target.append, value, APPEND_SIZE-1);
		
		return;
	}
	if(str_compare(name, "default", STR_NOCASE)) {
		target.flags |= TARGET_DEFAULT;
		return;
	}
	if(str_compare(name, "rootfs", STR_NOCASE)) {
		if(value[0] == '\0') {
			printD("config:%u: RootFS requires an argument", lnum);
			return;
		}
		
		config_add_mount(lnum, value, "/");
		return;
	}
	if(str_compare(name, "mount", STR_NOCASE)) {
		char* mpoint = strchr(value, ' ');
		if(mpoint == NULL) {
			printD("config:%u: Invalid mount", lnum);
			return;
		}
		mpoint[0] = '\0';
		mpoint += (strspn(mpoint+1, " \t")+1);
		
		if(value[0] == '\0' || mpoint[0] == '\0') {
			printD("config:%u: Invalid mount", lnum);
			return;
		}
		
		config_add_mount(lnum, value, mpoint);
		return;
	}
	if(str_compare(name, "grub_config", STR_NOCASE)) {
		config.grub_config = strclone(value, 9999);
	}
	if(str_compare(name, "grub_devices", STR_NOCASE)) {
		config.grub_devices = strclone(value, 9999);
	}
	if(str_compare(name, "grub_first", STR_NOCASE)) {
		if(str_compare(value, "hdx", 0)) {
			config.grub_first = hdx;
		}else if(str_compare(value, "sdx", 0)) {
			config.grub_first = sdx;
		}else{
			printD("config:%u: Value must be hdx or sdx", lnum);
		}
		
		return;
	}
	
	printD("config:%u: Unknown directive '%s'", lnum, name);
}

/* Add the remaining target, if it exists */
void config_finish(void) {
	if(target.name[0] != '\0') {
		cfg_add_target();
	}
}
