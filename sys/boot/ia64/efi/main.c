/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 1998,2000 Doug Rabson <dfr@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <stand.h>
#include <string.h>
#include <setjmp.h>

#include <efi.h>
#include <efilib.h>

#include "bootstrap.h"
#include "efiboot.h"

extern char bootprog_name[];
extern char bootprog_rev[];
extern char bootprog_date[];
extern char bootprog_maker[];

struct efi_devdesc	currdev;	/* our current device */
struct arch_switch	archsw;		/* MI/MD interface boundary */

EFI_STATUS
efi_main (EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
	int i;
	EFI_PHYSICAL_ADDRESS mem;

	efi_init(image_handle, system_table);

	/* 
	 * Initialise the heap as early as possible.  Once this is done,
	 * alloc() is usable. The stack is buried inside us, so this is
	 * safe.
	 */
	BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
			  512*1024/4096, &mem);
	setheap((void *)mem, (void *)(mem + 512*1024));

	/* 
	 * XXX Chicken-and-egg problem; we want to have console output
	 * early, but some console attributes may depend on reading from
	 * eg. the boot device, which we can't do yet.  We can use
	 * printf() etc. once this is done.
	 */
	cons_probe();

	/*
	 * Initialise the block cache
	 */
	bcache_init(32, 512);		/* 16k XXX tune this */

	/*
	 * March through the device switch probing for things.
	 */
	for (i = 0; devsw[i] != NULL; i++)
		if (devsw[i]->dv_init != NULL)
			(devsw[i]->dv_init)();

	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);
#if 0
	printf("Memory: %ld k\n", memsize() / 1024);
#endif
    
	/* XXX presumes that biosdisk is first in devsw */
	currdev.d_dev = devsw[0];
	currdev.d_type = currdev.d_dev->dv_type;
	currdev.d_kind.efidisk.unit = 0;
	/* XXX should be able to detect this, default to autoprobe */
	currdev.d_kind.efidisk.slice = -1;
	/* default to 'a' */
	currdev.d_kind.efidisk.partition = 0;

#if 0
	/* Create arc-specific variables */
	bootfile = GetEnvironmentVariable(ARCENV_BOOTFILE);
	if (bootfile)
		setenv("bootfile", bootfile, 1);
#endif

	env_setenv("currdev", EV_VOLATILE, efi_fmtdev(&currdev),
	    efi_setcurrdev, env_nounset);
	env_setenv("loaddev", EV_VOLATILE, efi_fmtdev(&currdev), env_noset,
	    env_nounset);

	setenv("LINES", "24", 1);	/* optional */
    
	archsw.arch_autoload = efi_autoload;
	archsw.arch_getdev = efi_getdev;
	archsw.arch_copyin = efi_copyin;
	archsw.arch_copyout = efi_copyout;
	archsw.arch_readin = efi_readin;

	interact();			/* doesn't return */

	return (EFI_SUCCESS);		/* keep compiler happy */
}

COMMAND_SET(quit, "quit", "exit the loader", command_quit);

static int
command_quit(int argc, char *argv[])
{
	exit(0);
	return (CMD_OK);
}

COMMAND_SET(memmap, "memmap", "print memory map", command_memmap);

static int
command_memmap(int argc, char *argv[])
{
	UINTN sz;
	EFI_MEMORY_DESCRIPTOR *map, *p;
	UINTN key, dsz;
	UINT32 dver;
	EFI_STATUS status;
	int i, ndesc;
	static char *types[] = {
	    "Reserved",
	    "LoaderCode",
	    "LoaderData",
	    "BootServicesCode",
	    "BootServicesData",
	    "RuntimeServicesCode",
	    "RuntimeServicesData",
	    "ConventionalMemory",
	    "UnusableMemory",
	    "ACPIReclaimMemory",
	    "ACPIMemoryNVS",
	    "MemoryMappedIO",
	    "MemoryMappedIOPortSpace",
	    "PalCode"
	};

	sz = 0;
	status = BS->GetMemoryMap(&sz, 0, &key, &dsz, &dver);
	if (status != EFI_BUFFER_TOO_SMALL) {
		printf("Can't determine memory map size\n");
		return;
	}
	map = malloc(sz);
	status = BS->GetMemoryMap(&sz, map, &key, &dsz, &dver);
	if (EFI_ERROR(status)) {
		printf("Can't read memory map\n");
		return;
	}

	ndesc = sz / dsz;
	printf("%23s %12s %12s %8s %4s\n",
	       "Type", "Physical", "Virtual", "#Pages", "Attr");
	       
	for (i = 0, p = map; i < ndesc;
	     i++, p = NextMemoryDescriptor(p, dsz)) {
	    printf("%23s %012lx %012lx %08lx ",
		   types[p->Type],
		   p->PhysicalStart,
		   p->VirtualStart,
		   p->NumberOfPages);
	    if (p->Attribute & EFI_MEMORY_UC)
		printf("UC ");
	    if (p->Attribute & EFI_MEMORY_WC)
		printf("WC ");
	    if (p->Attribute & EFI_MEMORY_WT)
		printf("WT ");
	    if (p->Attribute & EFI_MEMORY_WB)
		printf("WB ");
	    if (p->Attribute & EFI_MEMORY_UCE)
		printf("UCE ");
	    if (p->Attribute & EFI_MEMORY_WP)
		printf("WP ");
	    if (p->Attribute & EFI_MEMORY_RP)
		printf("RP ");
	    if (p->Attribute & EFI_MEMORY_XP)
		printf("XP ");
	    if (p->Attribute & EFI_MEMORY_RUNTIME)
		printf("RUNTIME");
	    printf("\n");
	}

	return (CMD_OK);
}
