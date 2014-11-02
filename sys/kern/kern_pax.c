/*-
 * Copyright (c) 2006 Elad Efrat <elad@NetBSD.org>
 * Copyright (c) 2013-2014, by Oliver Pinter <oliver.pntr at gmail.com>
 * Copyright (c) 2014, by Shawn Webb <lattera at gmail.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_pax.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysent.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/elf_common.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/queue.h>
#include <sys/libkern.h>
#include <sys/jail.h>

#include <sys/mman.h>
#include <sys/libkern.h>
#include <sys/exec.h>
#include <sys/kthread.h>

#include <sys/syslimits.h>
#include <sys/param.h>

#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <machine/elf.h>

#include <sys/pax.h>

#include <security/mac_bsdextended/mac_bsdextended.h>

SYSCTL_NODE(_security, OID_AUTO, pax, CTLFLAG_RD, 0,
    "PaX (exploit mitigation) features.");

const char *pax_status_str[] = {
	[PAX_FEATURE_DISABLED] = "disabled",
	[PAX_FEATURE_OPTIN] = "opt-in",
	[PAX_FEATURE_OPTOUT] = "opt-out",
	[PAX_FEATURE_FORCE_ENABLED] = "force enabled",
	[PAX_FEATURE_UNKNOWN_STATUS] = "UNKNOWN -> changed to \"force enabled\""
};

const char *pax_status_simple_str[] = {
	[PAX_FEATURE_SIMPLE_DISABLED] = "disabled",
	[PAX_FEATURE_SIMPLE_ENABLED] = "enabled"
};

struct prison *
pax_get_prison(struct proc *p)
{
	/* p can be NULL with kernel threads, so use prison0 */
	if (p == NULL || p->p_ucred == NULL)
		return &prison0;

	return (p->p_ucred->cr_prison);
}

int
pax_get_flags(struct proc *p, uint32_t *flags)
{
	*flags = p->p_pax;

	return (0);
}

int
pax_elf(struct image_params *imgp, uint32_t mode)
{
	u_int flags, flags_aslr;

	flags = 0;
	flags_aslr = 0;

	if ((mode & MBI_ALLPAX) != MBI_ALLPAX) {
		if (mode & MBI_ASLR_ENABLED)
			flags |= PAX_NOTE_ASLR;
		if (mode & MBI_ASLR_DISABLED)
			flags |= PAX_NOTE_NOASLR;
	}

	if ((flags & ~PAX_NOTE_ALL) != 0) {
		printf("%s: unknown paxflags: %x\n", __func__, flags);

		return (1);
	}

	if (((flags & PAX_NOTE_ALL_ENABLED) & ((flags & PAX_NOTE_ALL_DISABLED) >> 1)) != 0) {
		/*
		 * indicate flags inconsistencies in dmesg and in user terminal
		 */
		printf("%s: inconsistent paxflags: %x\n", __func__, flags);

		return (1);
	}

#ifdef PAX_ASLR
	flags_aslr = pax_aslr_setup_flags(imgp, mode);
#endif

	flags = flags_aslr;


	CTR3(KTR_PAX, "%s : flags = %x mode = %x",
	    __func__, flags, mode);

	imgp->pax_flags = flags;
	if (imgp->proc != NULL) {
		PROC_LOCK(imgp->proc);
		imgp->proc->p_pax = flags;
		PROC_UNLOCK(imgp->proc);
	}

	return (0);
}


/*
 * print out PaX settings on boot time, and validate some of them
 */
static void
pax_sysinit(void)
{

	printf("PAX: initialize and check PaX and HardeneBSD features.\n");
}
SYSINIT(pax, SI_SUB_PAX, SI_ORDER_FIRST, pax_sysinit, NULL);

void
pax_init_prison(struct prison *pr)
{
	CTR2(KTR_PAX, "%s: Setting prison %s PaX variables\n",
	    __func__, pr->pr_name);

	if (pr->pr_parent == NULL) {
		/* prison0 has no parent, use globals */
#ifdef PAX_ASLR
		pr->pr_pax_aslr_status = pax_aslr_status;
		pr->pr_pax_aslr_debug = pax_aslr_debug;
		pr->pr_pax_aslr_mmap_len = pax_aslr_mmap_len;
		pr->pr_pax_aslr_stack_len = pax_aslr_stack_len;
		pr->pr_pax_aslr_exec_len = pax_aslr_exec_len;

#ifdef COMPAT_FREEBSD32
		pr->pr_pax_aslr_compat_status = pax_aslr_compat_status;
		pr->pr_pax_aslr_compat_mmap_len = pax_aslr_compat_mmap_len;
		pr->pr_pax_aslr_compat_stack_len = pax_aslr_compat_stack_len;
		pr->pr_pax_aslr_compat_exec_len = pax_aslr_compat_exec_len;
#endif /* COMPAT_FREEBSD32 */
#endif /* PAX_ASLR */
	} else {
#ifdef PAX_ASLR
		struct prison *p = pr->pr_parent;

		pr->pr_pax_aslr_status = p->pr_pax_aslr_status;
		pr->pr_pax_aslr_debug = p->pr_pax_aslr_debug;
		pr->pr_pax_aslr_mmap_len = p->pr_pax_aslr_mmap_len;
		pr->pr_pax_aslr_stack_len = p->pr_pax_aslr_stack_len;
		pr->pr_pax_aslr_exec_len = p->pr_pax_aslr_exec_len;

#ifdef COMPAT_FREEBSD32
		pr->pr_pax_aslr_compat_status =
		    p->pr_pax_aslr_compat_status;
		pr->pr_pax_aslr_compat_mmap_len =
		    p->pr_pax_aslr_compat_mmap_len;
		pr->pr_pax_aslr_compat_stack_len =
		    p->pr_pax_aslr_compat_stack_len;
		pr->pr_pax_aslr_compat_exec_len =
		    p->pr_pax_aslr_compat_exec_len;
#endif /* COMPAT_FREEBSD32 */
#endif /* PAX_ASLR */
	}
}
