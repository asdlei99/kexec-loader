# kexec-loader - Root makefile
# Copyright (C) 2007-2009 Daniel Collins <solemnwarning@solemnwarning.net>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# This should be "vN.N" releases
#
VERSION=r$(shell svn info | grep 'Revision:' | sed -e 's/Revision: //')

# kexec-tools
KT_VER := 2.0.0
KT_URL := http://www.kernel.org/pub/linux/kernel/people/horms/kexec-tools/kexec-tools-$(KT_VER).tar.gz
KT_CONFIGURE :=

# e2fsprogs
E2FS_VER := 1.41.8
E2FS_URL := http://surfnet.dl.sourceforge.net/sourceforge/e2fsprogs/e2fsprogs-$(E2FS_VER).tar.gz
E2FS_CONFIGURE :=

CC := gcc
LD := ld
CFLAGS := -Wall -DVERSION=\"$(VERSION)\"
INCLUDES := -Isrc/e2fsprogs-$(E2FS_VER)/lib/
LIBS := src/kexec.a src/libblkid.a src/libuuid.a -lz -llzmadec
export KLBASE := $(PWD)

FLOPPY ?= floppy.img

ifdef HOST
	CC := $(HOST)-gcc
	LD := $(HOST)-ld
	
	KT_CONFIGURE += --host=$(HOST)
	E2FS_CONFIGURE += --host=$(HOST) --with-cc=$(CC) --with-linker=$(LD)
endif

OBJS := src/misc.o src/disk.o src/console.o src/menu.o src/modprobe.o \
	src/boot.o src/grub.o src/shell.o src/globcmp.o src/keymap.o src/tar.o

all: kexec-loader kexec-loader.static

clean:
	rm -f src/*.o src/*.a
	rm -f kexec-loader kexec-loader.static
	rm -rf src/kexec-tools-$(KT_VER)/
	rm -rf src/e2fsprogs-$(E2FS_VER)/

distclean: clean
	rm -f kexec-tools-$(KT_VER).tar.gz
	rm -f e2fsprogs-$(E2FS_VER).tar.gz

kexec-loader: $(OBJS) src/kexec.a
	$(CC) $(CFLAGS) -o kexec-loader $(OBJS) $(LIBS)

floppy: syslinux.cfg initrd.img
ifeq ($(KERNEL),)
	@echo "Please set KERNEL to the Linux kernel source directory"
else
	mformat -i $(FLOPPY) -C -f 1440 -v kexecloader ::
	mmd -i $(FLOPPY) ::/syslinux
	syslinux -d /syslinux $(FLOPPY)
	mcopy -i $(FLOPPY) syslinux.cfg ::/syslinux
	mcopy -i $(FLOPPY) initrd.img ::/
	mcopy -i $(FLOPPY) README.html ::/
	mcopy -i $(FLOPPY) kexec-loader.conf ::/
	mmd -i $(FLOPPY) ::/modules
	mcopy -i $(FLOPPY) $(KERNEL)/.config ::/linux.cfg
	mcopy -i $(FLOPPY) $(KERNEL)/arch/x86/boot/bzImage ::/vmlinuz
endif

syslinux.cfg:
	echo "DEFAULT /vmlinuz initrd=/initrd.img" > syslinux.cfg

initrd.img: kexec-loader.static
	./mkinitramfs.sh initramfs.cpio
	lzma -c -9 initramfs.cpio > initrd.img

kexec-loader.static: $(OBJS) src/kexec.a
	$(CC) $(CFLAGS) -static -o kexec-loader.static $(OBJS) $(LIBS)

%.o: %.c src/libblkid.a
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

src/kexec.a:
	wget -nc $(KT_URL)
	tar -C src -xzf kexec-tools-$(KT_VER).tar.gz
	cd src/kexec-tools-$(KT_VER)/ && \
	patch -Np1 -i ../../patches/kexec-tools-2.0.0.diff && \
	./configure $(KT_CONFIGURE)
	$(MAKE) -C src/kexec-tools-$(KT_VER)/

src/libblkid.a:
	wget -nc $(E2FS_URL)
	tar -C src -xzf e2fsprogs-$(E2FS_VER).tar.gz
	cd src/e2fsprogs-$(E2FS_VER)/ && \
	./configure $(E2FS_CONFIGURE)
	$(MAKE) -C src/e2fsprogs-$(E2FS_VER)/lib/blkid/
	$(MAKE) -C src/e2fsprogs-$(E2FS_VER)/lib/uuid/
	cp src/e2fsprogs-$(E2FS_VER)/lib/lib{blkid,uuid}.a src
