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

#include "config.h"
#include "../config.h"
#include "main.h"
#include "debug.h"
#include "misc.h"

struct kl_config config = CONFIG_DEFAULTS_DEFINE;

static FILE* cfg_handle = NULL;

/* Open configuration file */
static void config_open(void) {
	while(cfg_handle == NULL) {
		cfg_handle = fopen(CONFIG_FILE, "r");
		
		if(cfg_handle == NULL && errno != EINTR) {
			fatal("Can't open configuration file: %s", strerror(errno));
		}
	}
}

/* Close configuration file */
static void config_close(void) {
	if(cfg_handle == NULL) {
		return;
	}
	
	while(fclose(cfg_handle) != 0) {
		if(errno == EINTR) {
			continue;
		}
		
		debug("Can't close config file: %s", strerror(errno));
		debug("Dropping handle %p!", cfg_handle);
		break;
	}
	cfg_handle = NULL;
}

/* Read next line from configuration file */
static char* config_readline(void) {
	config_open();
	
	char* line = allocate(1024);
	if(fgets(line, 1024, cfg_handle) == NULL) {
		if(feof(cfg_handle)) {
			config_close();
			
			free(line);
			return(NULL);
		}
		
		fatal("Can't read config file: %s", strerror(errno));
	}
	
	/* Remove newline and carridge return characters from the end of the
	 * line.
	*/
	size_t end = strlen(line)-1;
	while(line[end] == '\n' || line[end] == '\r') {
		line[end] = '\0';
		end--;
	}
	
	return(line);
}

/* Load configuration from file */
void config_load(void) {
	char* line = NULL;
	size_t len = 0;
	size_t count = 0;
	size_t lnum = 0;
	
	char* name = NULL;
	char* value = NULL;
	
	int validcfg = 0;
	kl_target target = TARGET_DEFAULTS_DEFINE;
	
	while((line = config_readline()) != NULL) {
		lnum++;
		len = strlen(line);
		value = NULL;
		
		/* Remove leading spaces/tabs from line */
		count = 0;
		while(line[count] == ' ' || line[count] == '\t') {
			count++;
		}
		if(count > 0) {
			memmove(line, line+count, (len+1)-count);
			len -= count;
		}
		
		/* Skip line if it's a comment, or empty */
		if(line[0] == '#' || line[0] == '\0') {
			free(line);
			continue;
		}
		
		/* Copy name from line and set value pointer to line */
		count = 0;
		while(line[count] != ' ' && line[count] != '\t') {
			count++;
		}
		name = strclone(line, count);
		value = line+count;
		
		/* Remove leading spaces/tabs from value */
		count = 0;
		while(value[count] == ' ' || value[count] == '\t') {
			count++;
		}
		if(count > 0) {
			memmove(value, value+count, (strlen(value)+1)-count);
		}
		
		/* Remove trailing spaces/tabs from value */
		count = strlen(value)-1;
		while(value[count] == ' ' || value[count] == '\t') {
			value[count] = '\0';
			count--;
		}
		
		validcfg = 0;
		if(str_compare(name, "timeout", STR_NOCASE)) {
			config.timeout = strtoul(value, NULL, 10);
			debug("timeout='%s' (%u)", value, config.timeout);
			
			validcfg = 1;
		}
		if(str_compare(name, "title", STR_NOCASE)) {
			if(target.name[0] != '\0') {
				if(target_add(&(config.targets), &target) == NULL) {
					fatal("Can't load config: %s", strerror(errno));
				}
				
				TARGET_DEFAULTS(&target);
			}
			
			strncpy(target.name, value, 63);
		}
		if(str_compare(name, "kernel", STR_NOCASE)) {
			strncpy(target.kernel, value, 1023);
		}
		if(str_compare(name, "initrd", STR_NOCASE)) {
			strncpy(target.initrd, value, 1023);
		}
		if(str_compare(name, "append", STR_NOCASE)) {
			strncpy(target.append, value, 511);
		}
		if(!validcfg) {
			printf("Unknown configuration variable '%s' at line %u\n", name, lnum);
		}
		
		free(name);
		free(line);
	}
	
	if(target.name[0] != '\0') {
		if(target_add(&(config.targets), &target) == NULL) {
			fatal("Can't load config: %s", strerror(errno));
		}
		
		TARGET_DEFAULTS(&target);
	}
}
