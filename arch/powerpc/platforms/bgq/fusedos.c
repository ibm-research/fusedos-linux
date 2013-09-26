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

#include <asm/cputhreads.h>
#include <linux/bootmem.h>
#include <linux/export.h>
#include <linux/memblock.h>

#include "bgq.h"
#include "bic.h"
#include "fusedos_config.h"
#include "fusedos.h"

extern void spc_exception_vector(void);

int NR_GPCS = 4;
void* spc_monitor;
EXPORT_SYMBOL(spc_monitor);
spc_context_t* spc_context;
EXPORT_SYMBOL(spc_context);
fusedos_config_t* fusedos_config;
EXPORT_SYMBOL(fusedos_config);
void* spc_memory;

#define PHYMAP_CONST64(x) x ## LL
#define PHYMAP_MINADDR_L1P PHYMAP_CONST64(0x3fde8000000)
#define PHYMAP_PRIVILEGEDOFFSET PHYMAP_CONST64(0x00200000000)
#define BIC_DEVICE_BASE_ADDRESS (unsigned long long)(PHYMAP_MINADDR_L1P + PHYMAP_PRIVILEGEDOFFSET)
static struct bgq_bic_puea* _puea = (struct bgq_bic_puea*)BIC_DEVICE_BASE_ADDRESS;

void* _fw = (void*)1;
EXPORT_SYMBOL(_fw);

static int __init nr_gpcs(char *str)
{
    int val;
    if (get_option(&str, &val)) {
        NR_GPCS = val;
    } else {
        NR_GPCS = 4;
    }
    return 0;
}
early_param("nr_gpcs", nr_gpcs);

void fusedos_config_init(void)
{
    fusedos_config->nr_gpcs = NR_GPCS;
    fusedos_config->nr_spcs = num_present_cpus() - NR_GPCS;
    BUG_ON(fusedos_config->nr_spcs < 0);
    fusedos_config->fusedos_debug = 0;
    pr_info("NR_GPCS %d, num_present_cpus %d, nr_spcs %d\n",
	    NR_GPCS, num_present_cpus(), fusedos_config->nr_spcs);
}

void spc_context_init(void)
{
    int i;
    int linux_cpu = 0; // Linux cpu that will handle spc interrupts

    for (i = 0; i < fusedos_config->nr_spcs; i++) {
        memset((void*)(&(spc_context[i].regs)), 0, sizeof(regs_t));

        // Taken from bgq_cause_ipi() in bic.c
        spc_context[i].bic_int_send = (void*)(&_puea->interrupt_send);
        spc_context[i].bic_value = cpu_thread_in_core(linux_cpu) + 1;
        spc_context[i].bic_value |= BGQ_BIC_C2C_INTTYPE_EXTERNAL << (63 - 60);
        spc_context[i].bic_value |= 0x0000000000200000ULL >> cpu_core_index_of_thread(linux_cpu);

        spc_context[i].ipi_wakeup = 0;

        spc_context[i].id = i;
        spc_context[i].start = 0;
        spc_context[i].command = 0;

        spc_context[i].mem_bot = __pa(spc_memory) + (uint64_t)(i) * (uint64_t)(SPC_MEMORY_SIZE);
        //printk("FUSEDOS spc_context_init: spc_context[%d].mem_bot %016llx\n", i, spc_context[i].mem_bot);

        memset((void*)(spc_context[i].tlb_entry), 0, sizeof(tlb_entry_t) * MAX_TLB_ENTRIES);
        spc_context[i].tlb_entry_count = 0;

        memset(spc_context[i].spcm_stack, 0, SPCM_STACK_SIZE);

        spc_context[i].spcm_sp = 0;
        spc_context[i].spcm_toc = 0;

        spc_context[i].ex_code = 0;

        spc_context[i].spcm_func.funcaddr = 0;
        spc_context[i].spcm_func.r2 = 0;

        spc_context[i].ipi_message.fcn = 0;
        spc_context[i].ipi_message.parm1 = 0;
        spc_context[i].ipi_message.parm2 = 0;

        spc_context[i].text_pstart = 0;
        spc_context[i].text_pend = 0;
        spc_context[i].data_pstart = 0;
        spc_context[i].data_pend = 0;
        spc_context[i].heap_pstart = 0;
        spc_context[i].heap_pend = 0;

        spc_context[i].scratch0 = 0;
        spc_context[i].scratch1 = 0;
        spc_context[i].scratch2 = 0;
        spc_context[i].scratch3 = 0;
        memset(spc_context[i].scratch, 0, SCRATCH_SIZE);
    }
}

int __init spc_memory_init(void)
{
#if 0
    void* bedram;
#endif

    // Quick sanity checks
    // Is SPC context area large enough for all SPC contexts?
    BUILD_BUG_ON((sizeof(spc_context_t) * NR_CPUS) > SPC_CONTEXT_SIZE);
    // Does struct fusedos_config_t fit into memory area for FusedOS config?
    BUILD_BUG_ON(sizeof(fusedos_config_t) > FUSEDOS_CONFIG_SIZE);

    // Put the SPC monitor, context, and config just below 1 GB
    spc_monitor = (void*)memblock_alloc_base(SPC_MONITOR_SIZE + SPC_CONTEXT_SIZE + FUSEDOS_CONFIG_SIZE,
                                             (phys_addr_t)(1ul << 24),  // align to 16 MB
                                             (phys_addr_t)(1ul << 30)); // below 1 GB
    if (!spc_monitor) {
        printk(KERN_ERR "FUSEDOS spc_memory_init: Cannot allocate spc_monitor.\n");
        return -2;
    }
    spc_context = (spc_context_t*)(__va(spc_monitor + SPC_MONITOR_SIZE));
    fusedos_config = (fusedos_config_t*)(__va(spc_monitor + SPC_MONITOR_SIZE + SPC_CONTEXT_SIZE));

    fusedos_config_init();

    if( fusedos_config->nr_spcs > 0 ) {
        spc_memory = __alloc_bootmem(
    	    ((unsigned long)SPC_MEMORY_SIZE) * (fusedos_config->nr_spcs),
    	    PAGE_SIZE, SPC_MEMORY_PADDR);
    
        if (__pa(spc_memory) < SPC_MEMORY_PADDR) {
            printk(KERN_ERR "FUSEDOS spc_memory_init: Cannot allocate spc_memory at 0x%x, 0x%lx\n",
                   SPC_MEMORY_PADDR, __pa(spc_memory));
            return -3;
        }
    }
    printk("FUSEDOS spc_memory_init: spc_monitor 0x%p, spc_context 0x%p, fusedos_config 0x%p\n",
           spc_monitor, spc_context, fusedos_config);
    printk("FUSEDOS spc_memory_init: spc_memory 0x%p, __pa(spc_memory) 0x%lx\n", spc_memory, __pa(spc_memory));
    printk("FUSEDOS spc_memory_init: _fw %p\n", _fw);

    // From firmware/src/fw_mmu.c, tlbwe_slot parameters calculated
    // with tests/fusedos/tlbwe_slot_defines
    //
    // NOTE: we force this into way 3 of the TLB set in order to avoid an A2 defect
    //       that does not properly honor IPROT (Linux relies on IPROT to keep the
    //       firmware TLB resident).
    // tlbwe_slot(
    //     3,
    //     MAS1_V(1) | MAS1_TID(0) | MAS1_TS(0) | MAS1_TSIZE_1GB | MAS1_IPROT(1),
    //     MAS2_EPN((PHYMAP_MINADDR_MMIO | PHYMAP_PRIVILEGEDOFFSET) >> 12) | MAS2_W(0) | MAS2_I(1) | MAS2_M(1) |
    //              MAS2_G(1) | MAS2_E(0),
    //     MAS7_3_RPN((PHYMAP_MINADDR_MMIO | PHYMAP_PRIVILEGEDOFFSET) >> 12) | MAS3_SR(1) | MAS3_SW(1) | MAS3_SX(1) |
    //                MAS3_UR(0) | MAS3_UW(0) | MAS3_UX(0) | MAS3_U1(1),
    //     MAS8_TGS(0) | MAS8_VF(0) | MAS8_TLPID(0),
    //     MMUCR3_X(0) | MMUCR3_R(1) |       MMUCR3_C(1) | MMUCR3_ECL(0) | MMUCR3_CLASS(1) |MMUCR3_ThdID(0xF)
    //     );
    //
#define SPRN_MMUCR3               (1023)           // Memory Management Unit Control Register 3
    asm volatile ("mtspr %0,%1": : "i" (SPRN_MAS0),      "r" (0x30000));
    asm volatile ("mtspr %0,%1": : "i" (SPRN_MAS1),      "r" (0xc0000a00));
    asm volatile ("mtspr %0,%1": : "i" (SPRN_MAS2),      "r" (0x3ffc000000e));
    asm volatile ("mtspr %0,%1": : "i" (SPRN_MAS7_MAS3), "r" (0x3ffc0000115));
    asm volatile ("mtspr %0,%1": : "i" (SPRN_MAS8),      "r" (0x0));
    asm volatile ("mtspr %0,%1": : "i" (SPRN_MMUCR3),    "r" (0x310f));
    asm volatile ("isync;" : : : "memory" );
    asm volatile ("tlbwe;" : : : "memory" );

    spc_context_init();

    return 0;
}

void (*spc_ipi_fp)(int, uint64_t) = NULL;
EXPORT_SYMBOL(spc_ipi_fp);

//void (*upc_ipi_fp)() = NULL;
//EXPORT_SYMBOL(upc_ipi_fp);

wait_queue_head_t cl_wait_array[NR_CPUS];
wait_queue_head_t* cl_wait = cl_wait_array;
EXPORT_SYMBOL(cl_wait);
