/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.
 */
/*
 * Copyright (c) 2012, Joyent, Inc.  All rights reserved.
 */


#include <sys/types.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/processor.h>
#include <sys/cpuvar.h>
#include <sys/kmem.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/systm.h>
#include <sys/kstat.h>

#include <sys/auxv.h>
#include <sys/systeminfo.h>
#include <sys/cpuvar.h>
#include <sys/pghw.h>

//#if defined(__x86)
#include <sys/x86_archext.h>
//#pragma warn it is fine
//#endif

static dev_info_t *coretemp_devi;

struct coretemp_kstat_t {
		kstat_named_t chip_id;
		kstat_named_t core_id;
		kstat_named_t temperature;
		kstat_named_t tj_max;
		kstat_named_t chip_temperature;
} coretemp_kstat_t = {
		{ "chip_id",		KSTAT_DATA_ULONG },
		{ "core_id",		KSTAT_DATA_ULONG },
		{ "temperature",	KSTAT_DATA_ULONG },
		{ "tj_max",			KSTAT_DATA_ULONG },
		{ "package_temp",	KSTAT_DATA_ULONG },
};

kstat_t *entries[1024];

int read_msr_on_cpu(cpu_t *cpu, uint32_t msr_index, uint64_t *result);

static int coretemp_kstat_update(kstat_t *kstat, int rw) {
	if(rw == KSTAT_WRITE) {
		return (EACCES);
	}

	if (!is_x86_feature(x86_featureset, X86FSET_MSR)) {
		return (ENXIO);
	}

	uint64_t msr;
	int cpu_index = (int)kstat->ks_private;

	cpu_t *cpu_ptr = (cpu_t *)cpu[cpu_index];

	int tj_max = 100;

	// tj max
	if(read_msr_on_cpu(cpu_ptr, 0x1a2, &msr) == 0) {
		tj_max = (msr >> 16) & 0x7f;
		coretemp_kstat_t.tj_max.value.ui64 = tj_max;
	}

	if(read_msr_on_cpu(cpu_ptr, 0x1b1, &msr) == 0) {
		int pkg_temp = tj_max - ((msr >> 16) & 0x7f);
		coretemp_kstat_t.chip_temperature.value.ui64 = pkg_temp;
	}

	if(read_msr_on_cpu(cpu_ptr, 0x19c, &msr) == 0) {
		int core_temp = tj_max - ((msr >> 16) & 0x7f);
		coretemp_kstat_t.temperature.value.ui64 = core_temp;
	}

	// misc data
	coretemp_kstat_t.core_id.value.ui64 = cpuid_get_pkgcoreid(cpu_ptr);
	coretemp_kstat_t.chip_id.value.ui64 = pg_plat_hw_instance_id(cpu_ptr, PGHW_CHIP);

	
	return (0);
}

static int coretemp_getinfo(dev_info_t *devi, ddi_info_cmd_t cmd, void *arg, void **result) {
	switch(cmd) {
		case DDI_INFO_DEVT2DEVINFO:
		case DDI_INFO_DEVT2INSTANCE:
			break;
		default:
			return (DDI_FAILURE);
	}

	if(cmd == DDI_INFO_DEVT2INSTANCE) {
		*result = 0;
	} else {
		*result = coretemp_devi;
	}

	return (DDI_SUCCESS);
}

static int coretemp_attach(dev_info_t *devi, ddi_attach_cmd_t cmd) {
	if(cmd != DDI_ATTACH) {
		return (DDI_FAILURE);
	}

	coretemp_devi = devi;

	printf("Attaching coretemp driver for %d CPU(s)", ncpus);

	// for each cpu initialize kstat
	int i;
	for(i = 0; i < ncpus; i++) {
		char kstat_name[128];
		sprintf(kstat_name, "coretemp%d", i);
		kstat_t *ksp = kstat_create("cpu_info", i, kstat_name, "misc", KSTAT_TYPE_NAMED, 
			sizeof(coretemp_kstat_t) / sizeof(kstat_named_t),
			KSTAT_FLAG_VIRTUAL);
		if(!ksp) {
			printf("Failed to create kstat entry");
			return (DDI_FAILURE);
		}

		entries[i] = ksp;
		ksp->ks_data = (void *)&coretemp_kstat_t;
		ksp->ks_update = coretemp_kstat_update;
		ksp->ks_private = (void *)i;
		kstat_install(ksp);
	}
	return (DDI_SUCCESS);
}

static int coretemp_detach(dev_info_t *devi, ddi_detach_cmd_t cmd) {
	if (cmd != DDI_DETACH) {
		return (DDI_FAILURE);
	}
	coretemp_devi = NULL;

	int i;
	for(i = 0; i < ncpus; i++) {
		kstat_delete(entries[i]);
	}

	return (DDI_SUCCESS);
}


static struct cb_ops coretemp_cb_ops = {
	nulldev,
	nulldev,	/* close */
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,
	nodev,		/* write */
	nodev,
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,
	NULL,
	D_64BIT | D_NEW | D_MP
};

static struct dev_ops coretemp_dv_ops = {
	DEVO_REV,
	0,
	coretemp_getinfo,
	nulldev,	/* identify */
	nulldev,	/* probe */
	coretemp_attach,
	coretemp_detach,
	nodev,		/* reset */
	&coretemp_cb_ops,
	(struct bus_ops *)0,
	NULL,
	ddi_quiesce_not_needed,		/* quiesce */
};

static struct modldrv modldrv = {
	&mod_driverops,
	"coretemp driver 1.0",
	&coretemp_dv_ops
};

static struct modlinkage modl = {
	MODREV_1,
	&modldrv
};

int
_init(void)
{
	return (mod_install(&modl));
};

int
_fini(void)
{
	return (mod_remove(&modl));
};

int
_info(struct modinfo *modinfo)
{
	return (mod_info(&modl, modinfo));
};


// internal stuff

struct msr_req_t {
	uint32_t msr_index;
	uint64_t *result;
};

void msr_req_func(uintptr_t req_ptr, uintptr_t error_ptr) {
	
	label_t ljb;
	uint32_t msr_index = ((struct msr_req_t *)req_ptr)->msr_index;
	uint64_t *result = ((struct msr_req_t *)req_ptr)->result;

	int error;

	if(on_fault(&ljb)) {
		dev_err(coretemp_devi, CE_WARN, "Invalid rdmsr(0x%08" PRIx32 ")", (uint32_t)msr_index);
		error = EFAULT;
	} else {
		error = checked_rdmsr(msr_index, result);
	}

	*((int *)error_ptr) = error;

	return;

}

int read_msr_on_cpu(cpu_t *cpu, uint32_t msr_index, uint64_t *result) {
	int error;

	struct msr_req_t request;
	request.msr_index = msr_index;
	request.result = result;

	cpu_call(cpu, (cpu_call_func_t)msr_req_func, (uintptr_t)&request, (uintptr_t)&error);

	return (error);
}
