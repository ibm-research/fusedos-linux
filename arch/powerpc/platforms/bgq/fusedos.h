/*
 * FusedOS modifications to the Linux kernel
 * authors:
 *    Yoonho Park <yoonho@us.ibm.com>
 *    Eric Van Hensbergen <ericvh@gmail.com>
 *    Marius Hillenbrand <mlhillen@us.ibm.com>
 *
 * Licensed Materials - Property of IBM
 *
 * Blue Gene/Q
 *
 * (c) Copyright IBM Corp. 2011, 2013 All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM
 * Corporation.
 *
 * This software is available to you under the GNU General Public
 * License (GPL) version 2.
 */

#ifndef _LINUX_SPC_H_
#define _LINUX_SPC_H_

#include <asm/elf.h>
#include <linux/types.h>

#if !(defined __KERNEL__)
typedef __u8 uint8_t;
typedef __u32 uint32_t;
typedef __u64 uint64_t;
#include <inttypes.h>
#include <sys/types.h>
#endif // __KERNEL__

#ifndef _REGS_H_
// From cnk/src/Regs.h
#define NUM_GPRS (32)
#define NUM_QVRS (32)
#define NUM_SPRG (10) // SPRG[0..8], and VRSAVE
#define NUM_GSPRG (4) // If no active Hypervisor, there are also 4 Guest SPRGs: GPSRG[0..3]
#endif // _REGS_H_

// From cnk/src/Regs.h (renamed)
typedef union {
    double    d[ 4];
    float     f[ 8];
    uint64_t ll[ 4];
    uint32_t  l[ 8];
    uint8_t   b[32];
} __attribute__ ((aligned (32))) qpx_reg_t;
typedef struct {
    uint64_t gpr[NUM_GPRS];
    qpx_reg_t gvr[NUM_QVRS];
    uint64_t fpscr;
    uint64_t sprg[NUM_SPRG];
    uint64_t gsprg[NUM_GSPRG];
    uint64_t ip,  // from (mc|c)srr0
             msr, // from (mc|c)srr1
             cr,
             lr,
             xer,
             ctr,
             esr,
             dear,
             pid,
             dbcr0,
             dbcr1,
             dbcr2,
             dbcr3,
             dac1,
             dac2,
             dac3,
             dac4,
             dbsr,
             iac1,
             iac2,
             iac3,
             iac4,
	     mcsr;
} __attribute__ ((aligned (64))) regs_t;

typedef struct {
    uint64_t slot;
    uint64_t mas1;
    uint64_t mas2;
    uint64_t mas7_3;
    uint64_t mas8;
    uint64_t mmucr3;
} tlb_entry_t;

// From CoreState.h
typedef void (*IPIHANDLER_Fcn_t)( uint64_t , uint64_t );
typedef struct IPI_Message_t {
    IPIHANDLER_Fcn_t fcn;
    uint64_t parm1;
    uint64_t parm2;
} IPI_Message_t;

// From firmware/include/Firmware.h
#define NUM_CORES          (17)
// From firmware/src/Firmware_internals.h
typedef struct FW_InternalState_t {
    struct _CoreState_T {
        uint8_t flags;
        void*   entryPoint;
        void*   arg;
    } coreState[NUM_CORES];

    // Not including node state
} FW_InternalState_t;

#define SPC_IPI_QUIT    1
#define SPC_IPI_UPCINIT 2
typedef struct spc_IPI_Message_t {
    uint64_t fcn;
    uint64_t parm1;
    uint64_t parm2;
} spc_IPI_Message_t;

typedef struct {
    regs_t regs;  // Must be first so we can use CNK's REG_OFS_* defines
    void* bic_int_send;  // &(puea->interrupt_send)
    uint64_t bic_value;
    volatile uint64_t ppr32;
    volatile uint64_t ipi_wakeup;

    uint64_t id;
    volatile uint64_t start;
#define SPC_START      1
#define SPC_RESUME     2
#define SPC_LOAD_TLB   3
#define SPC_UNLOAD_TLB 4
#define SPC_EXIT       5
#define SPC_SAVE_FPU   6
#define SPC_UPC_INIT   7
#define SPC_UPCP_INIT  8
    volatile uint64_t command;

    loff_t mem_bot;  // Memory bottom, set in spc_context_init

    volatile uint64_t tlb_entry_count;
    volatile uint64_t tlb_entry_install;
#define MAX_TLB_ENTRIES 512 // 512 in cnk/src/statictlb.cc
    tlb_entry_t tlb_entry[MAX_TLB_ENTRIES];

#define SPCM_STACK_SIZE 1024
    char spcm_stack[SPCM_STACK_SIZE];

    uint64_t spcm_sp;
    uint64_t spcm_toc;

    uint64_t ex_code;

    struct ppc64_opd_entry spcm_func;

    int fusedosfs_fd;

    volatile spc_IPI_Message_t ipi_message;

    uint64_t text_pstart;
    uint64_t text_pend;
    uint64_t data_pstart;
    uint64_t data_pend;
    uint64_t heap_pstart;
    uint64_t heap_pend;
    uint64_t shared_pstart;
    uint64_t shared_pend;

    uint64_t BG_IULLAVOIDPERIOD; // IU Livelock Buster period
    uint64_t BG_IULLAVOIDDELAY;  // IU Livelock Buster delay

    uint64_t scratch0;
    uint64_t scratch1;
    uint64_t scratch2;
    uint64_t scratch3;
#define SCRATCH_SIZE 2048
    uint64_t scratch[SCRATCH_SIZE];
} spc_context_t;

typedef struct {
    int nr_gpcs;
    int nr_spcs;
    int fusedos_debug;
} fusedos_config_t;

#define SPC_IOCTL_TEST     1
#define SPC_IOCTL_INIT     2
#define SPC_IOCTL_COMMAND  3
#define SPC_IOCTL_IPI_QUIT 4
#define SPC_IOCTL_WAIT_CMD 5

#if defined __CL__
#include <pthread.h>

extern pthread_key_t spc_key;

extern inline __attribute__((always_inline)) int this_spc(void)
{
    return (int)(*(int*)(pthread_getspecific(spc_key)));
}

extern inline __attribute__((always_inline)) spc_context_t* get_spc_context(int spc)
{
    spc_context_t* pc = (spc_context_t*)SPC_CONTEXT_VADDR;
    return &pc[spc];
}

extern inline __attribute__((always_inline)) fusedos_config_t* get_fusedos_config(void)
{
    return (fusedos_config_t*)FUSEDOS_CONFIG_VADDR;
}

#define CPU_TO_SPC(c) (c - get_fusedos_config()->nr_gpcs)
#define SPC_TO_CPU(c) (c + get_fusedos_config()->nr_gpcs)
#else // __CL__
#define CPU_TO_SPC(c) (c - fusedos_config->nr_gpcs)
#define SPC_TO_CPU(c) (c + fusedos_config->nr_gpcs)
#endif // __CL__

#endif /* _LINUX_SPC_H_ */
