/*-
 * Copyright (c) 2006 Elad Efrat <elad@NetBSD.org>
 * Copyright (c) 2013-2015, by Oliver Pinter <oliver.pinter@hardenedbsd.org>
 * Copyright (c) 2014-2015, by Shawn Webb <shawn.webb@hardenedbsd.org>
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
 *
 * HardenedBSD-version: 2fba75c32739bd7f7c80163ec88e3655c3130753
 * HardenedBSD-version: v16
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_pax.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/elf_common.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/jail.h>
#include <sys/kthread.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/pax.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/syslimits.h>
#include <sys/sx.h>
#include <sys/vnode.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <machine/elf.h>

static int pax_validate_flags(uint32_t flags);
static int pax_check_conflicting_modes(uint32_t mode);

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

	/* p can be NULL with kernel threads, so use prison0. */
	if (p == NULL || p->p_ucred == NULL)
		return (&prison0);

	PROC_LOCK_ASSERT(p, MA_OWNED);

	return (p->p_ucred->cr_prison);
}

/*
* As it stands right now, only the sysctl(8)s controlling ASLR use
* pax_get_prison_td. Thus this function requires td being equal to
* curthread.
*/
struct prison *
pax_get_prison_td(struct thread *td)
{

	KASSERT(td == curthread, ("pax_get_prison_td: td != curthread"));
	if (td == NULL || td->td_ucred == NULL)
		return (&prison0);
	return (td->td_ucred->cr_prison);
}

void
pax_get_flags(struct proc *p, uint32_t *flags)
{

	*flags = p->p_pax;
}

void
pax_get_flags_td(struct thread *td, uint32_t *flags)
{

	*flags = td->td_pax;
}

static int
pax_validate_flags(uint32_t flags)
{

	if ((flags & ~PAX_NOTE_ALL) != 0)
		return (1);
	return (0);
}

static int
pax_check_conflicting_modes(uint32_t mode)
{

	if (((mode & PAX_NOTE_ALL_ENABLED) & ((mode & PAX_NOTE_ALL_DISABLED) >> 1)) != 0)
		return (1);
	return (0);
}

int
pax_elf(struct image_params *imgp, uint32_t mode)
{
	uint32_t flags, flags_aslr;

	flags = mode;
	flags_aslr = 0;
	if (pax_validate_flags(flags) != 0)
		return (ENOEXEC);
	if (pax_check_conflicting_modes(mode) != 0)
		return (ENOEXEC);
#ifdef PAX_ASLR
	flags_aslr = pax_aslr_setup_flags(imgp, mode);
#endif
	flags = flags_aslr;
	CTR3(KTR_PAX, "%s : flags = %x mode = %x",
	    __func__, flags, mode);
	imgp->proc->p_pax = flags;
	return (0);
}

void
pax_init_prison(struct prison *pr)
{

	CTR2(KTR_PAX, "%s: Setting prison %s PaX variables\n",
	    __func__, pr->pr_name);

	pax_aslr_init_prison(pr);
#ifdef COMPAT_FREEBSD32
	pax_aslr_init_prison32(pr);
#endif
}
