#ifndef __COOKIE_JAR_H__
#define __COOKIE_JAR_H__

#include <vmm/types.h>

#define __PACK__ __attribute__ ((packed))

struct hd_info {
	u_int16	cylinders	__PACK__ ;
	u_int8	heads		__PACK__ ;
	u_int8	reserved1[2]	__PACK__ ;
	u_int16 precomp		__PACK__ ;
	u_int8	max_ecc		__PACK__ ;
	u_int8	drive_opts	__PACK__ ;
	u_int8	reserved2[3]	__PACK__ ;
	u_int16	lz		__PACK__ ;
	u_int8	sectors		__PACK__ ;
	u_int8	reserved3[1]	__PACK__ ;
};

struct cpu_fpu {
	u_int8	cpu_type __PACK__ ;	/* 0 = 8086 5 = 80586 */
	u_int8	fpu_type __PACK__ ;	/* 0 = no fpu 1 = fpu  2/3 = 287/387 + 386 */
	u_int8	id_flag __PACK__ ;	/* supports CPUID  and thus the following: */
	char	vendor[12] __PACK__ ;	/* vendor string */
	u_int8	model __PACK__ ;	/* model info */
	u_int8	stepping __PACK__;	/* stepping info */
	u_int8	intel_proc __PACK__ ;	/* 0 = not intel 1 = is intel */
	u_int32	flags __PACK__ ;	/* feature flags */
};

struct cookie_jar {
	u_int16	total_mem;	/* Total memory in K */
	struct cpu_fpu proc;	/* Processor Information */
	u_int16	comports[4];	/* base address of comports 0 if not present */
	u_int16	lprports[4];	/* printer ports as above */
	u_int8	total_hdisks;
	struct	hd_info hdisk[2];
	u_int8	floppy_types[2]; /* cmos types */
	u_int8	monitor_type;	/* cmos types */
	u_int16	base_mem;	/* mem upto 640 mark in K */
	u_int16 extended_mem;	/* mem above 1meg in K */
        u_int8  got_cmos;	/* if there is a (valid) cmos */
	u_int8	got_dma;	/* if there is dma hardware available */
};

extern struct cookie_jar cookie;
	
#endif
