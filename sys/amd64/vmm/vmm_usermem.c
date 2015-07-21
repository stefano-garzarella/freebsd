/*
 * Copyright (C) 2015 Stefano Garzarella (stefano.garzarella@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sglist.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#include <machine/md_var.h>

#include "vmm_mem.h"
#include "vmm_usermem.h"

#define MAX_USERMEMS	64

static struct usermem {
	struct vmspace   *vmspace;       /* guest address space */
	vm_paddr_t	gpa;
	size_t		len;
} usermems[MAX_USERMEMS];

static int
vmm_usermem_add(struct vmspace *vmspace, vm_paddr_t gpa, size_t len)
{
	int i;

	for (i = 0; i < MAX_USERMEMS; i++) {
		if (usermems[i].len == 0) {
			usermems[i].vmspace = vmspace;
			usermems[i].gpa = gpa;
			usermems[i].len = len;
			break;
		}
	}

	if (i == MAX_USERMEMS) {
		printf("vmm_usermem_add: empty usermem slot not found\n");
		return (ENOMEM);
	}

	return 0;
}

static int
vmm_usermem_del(struct vmspace *vmspace, vm_paddr_t gpa, size_t len)
{
	int i;

	for (i = 0; i < MAX_USERMEMS; i++) {
		if (usermems[i].vmspace == vmspace && usermems[i].gpa == gpa
				&& usermems[i].len == len) {
			bzero(&usermems[i], sizeof(struct usermem));
			return 1;
		}
	}

	return 0;
}

boolean_t
usermem_mapped(struct vmspace *vmspace, vm_paddr_t gpa)
{
	int i;

	for (i = 0; i < MAX_USERMEMS; i++) {
		if (usermems[i].vmspace != vmspace || usermems[i].len == 0)
			continue;
		if (gpa >= usermems[i].gpa &&
				gpa < usermems[i].gpa + usermems[i].len)
			return (TRUE);
	}
	return (FALSE);
}

vm_object_t
vmm_usermem_alloc(struct vmspace *vmspace, vm_paddr_t gpa, size_t len,
	       void *buf, struct thread *td)
{
	int error;
	vm_object_t obj;
	vm_map_t map;
	vm_map_entry_t entry;
	vm_pindex_t index;
	vm_prot_t prot;
	boolean_t wired;

	map = &td->td_proc->p_vmspace->vm_map;
	/* lookup the vm_object that describe user addr */
	error = vm_map_lookup(&map, (unsigned long)buf, VM_PROT_RW, &entry,
		&obj, &index, &prot, &wired);

	printf("---- guest MAP vm_object_t: %p vm_pindex: %ld ----\n", obj, index);
	/* map th vm_object in the vmspace */
	if (obj != NULL) {
		error = vm_map_find(&vmspace->vm_map, obj, index, &gpa, len, 0,
				    VMFS_NO_SPACE, VM_PROT_RW, VM_PROT_RW, 0);
		if (error != KERN_SUCCESS) {
			vm_object_deallocate(obj);
			obj = NULL;
		}
	}
	vm_map_lookup_done(map, entry);

	/* acquire the reference to the vm_object */
	if (obj != NULL) {
		vm_object_reference(obj);
		vmm_usermem_add(vmspace, gpa, len);
	}

	return (obj);
}

void
vmm_usermem_free(struct vmspace *vmspace, vm_paddr_t gpa, size_t len)
{
	int ret;

	ret  = vmm_usermem_del(vmspace, gpa, len);
	if (ret) {
		vmm_mem_free(vmspace, gpa, len);
	}
}

void
vmm_usermem_cleanup(struct vmspace *vmspace)
{
	int i;

	for (i = 0; i < MAX_USERMEMS; i++) {
		if (usermems[i].vmspace == vmspace) {
			vmm_mem_free(vmspace, usermems[i].gpa, usermems[i].len);
			bzero(&usermems[i], sizeof(struct usermem));
		}
	}
}
