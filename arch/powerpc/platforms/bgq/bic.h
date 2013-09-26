/*
 * Blue Gene/Q Platform
 * authors:
 *    Andrew Tauferner <ataufer@us.ibm.com>
 *    Todd Inglett <tinglett@us.ibm.com>
 *    Jimi Xenidis <jimix@pobox.com>
 *    Eric Van Hensbergen <ericvh@gmail.com>
 *    Marius Hillenbrand <mlhillen@us.ibm.com>
 *
 * Licensed Materials - Property of IBM
 *
 * Blue Gene/Q
 *
 * (c) Copyright IBM Corp. 2011, 2012, 2013 All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM
 * Corporation.
 *
 * This software is available to you under the GNU General Public
 * License (GPL) version 2.
 */


#ifndef __PLATFORM_BGQ_BIC_H
#define __PLATFORM_BGQ_BIC_H

#include <asm/dcr.h>

#define CONFIG_MU 1

#define BGQ_DCR_GEA_INTERRUPT_MAP0 0
#define BGQ_DCR_GEA_INTERRUPT_MAP(x) (BGQ_DCR_GEA_INTERRUPT_MAP0 + (x))

#define BGQ_DCR_DEVBUS_CONTROL				0x0
#define BGQ_DCR_DEVBUS_INTERRUPT_STATE			0x10
#define BGQ_DCR_DEVBUS_INTERRUPT_STATE_CONTROL_LOW	0x11
#define BGQ_DCR_DEVBUS_INTERRUPT_STATE_CONTROL_HI	0x12

#define BGQ_IRQ_SPURIOUS	-1

/* We use the bits in the PUEA interrupt status register as HW IRQ numbers.
 * Thus, determining the position of a set bit int the status register
 * delivers a HW IRQ number */
/* IRQs 0-29 are reserved  by HW, we use them for the DEVBUS interrupts */
#define BGQ_IRQ_INTA	    1
#define BGQ_IRQ_INTB	    2
#define BGQ_IRQ_INTC	    3
#define BGQ_IRQ_INTD	    4
/* UNUSED:
#define BGQ_IRQ_L2C_0	    30
#define BGQ_IRQ_L2C_1	    31
#define BGQ_IRQ_L2C_2	    32
#define BGQ_IRQ_L2C_3	    33

#define BGQ_IRQ_WAC	    36
*/
#define BGQ_IRQ_MU1	    37
#define BGQ_IRQ_MU2	    38
#define BGQ_IRQ_MU3	    39
#define BGQ_IRQ_MU4	    40

#define BGQ_IRQ_GEA0	    41
#define BGQ_IRQ_GEA1	    42
#define BGQ_IRQ_GEA2	    43
#define BGQ_IRQ_GEA3	    44
#define BGQ_IRQ_MSI0	    45 /* GEA lane 4-7 */
#define BGQ_IRQ_MSI1	    46
#define BGQ_IRQ_MSI2	    47
#define BGQ_IRQ_MSI3	    48
#define BGQ_IRQ_DEVBUS	    49 /* GEA lane 8 */
#define BGQ_IRQ_TESTINT	    50 /* GEA lane 9 */
#define BGQ_IRQ_GEA10	    51
#define BGQ_IRQ_GEA11	    52
#define BGQ_IRQ_GEA12	    53
#define BGQ_IRQ_GEA13	    54
#define BGQ_IRQ_GEA14	    55
#define BGQ_IRQ_MU6	    56 /* GEA lane 15 */
/* UNUSED:
#define BGQ_IRQ_L1P_BIC0    57
#define BGQ_IRQ_L1P_BIC1    58
#define BGQ_IRQ_L1P_BIC2    59
#define BGQ_IRQ_L1P_BIC3    60
#define BGQ_IRQ_L1P_BIC4    61
*/
#define BGQ_IRQ_IPI	    62
#define BGQ_IRQ_IPI1	    63
#define BGQ_IRQ_COUNT	    64

struct bgq_bic_puea {
	u64 _reserved[0x400];
	u64 clear_external_reg_0[4];
	u64 clear_critical_reg_0[4];
	u64 clear_wakeup_reg_0[4];
	u64 _hole_0[4];
	u64 clear_external_reg_1[4];
	u64 clear_critical_reg_1[4];
	u64 clear_wakeup_reg_1[4];
	u64 _hole_1[4];
	u64 set_external_reg_0[4];
	u64 set_critical_reg_0[4];
	u64 set_wakeup_reg_0[4];
	u64 _hole_2[4];
	u64 set_external_reg_1[4];
	u64 set_critical_reg_1[4];
	u64 set_wakeup_reg_1[4];
	u64 _hole_3[4];
	u64 interrupt_send;
	u64 _hole_4[7];
	u64 wakeup_send;
	u64 _hole_5[7];
	u64 map_interrupt[4];
	u64 ext_int_summary[4];
	u64 crit_int_summary[4];
	u64 mach_int_summary[4];
	u64 input_status;
};

struct bgq_bic {
        struct bgq_bic_puea __iomem *puea;
        struct irq_domain *domain;
        dcr_host_t dcr_gea;
        dcr_host_t dcr_devbus;
        /* the core we expect external interrupts to go to */
        unsigned master_core;
        /* the virtual IRQ for IPIs */
        unsigned vipi;
};

/* C2C Packet Format - int_type decoding */
#define BGQ_BIC_C2C_INTTYPE_EXTERNAL	0
#define BGQ_BIC_C2C_INTTYPE_CRITICAL	1
#define BGQ_BIC_C2C_INTTYPE_WAKE	2

/* Bits in BIC interrupt status register */
/* UNUSED
#define BGQ_PUEA_INT_SUMMARY_L2_MASK	0x00000003c0000000ULL
#define BGQ_PUEA_INT_SUMMARY_WU_MASK	0x0000000008000000ULL
*/
#define BGQ_PUEA_INT_SUMMARY_MU1_BIT	0x0000000004000000ULL
#define BGQ_PUEA_INT_SUMMARY_MU2_BIT	0x0000000002000000ULL
#define BGQ_PUEA_INT_SUMMARY_MU3_BIT	0x0000000001000000ULL
#define BGQ_PUEA_INT_SUMMARY_MU4_BIT	0x0000000000800000ULL
#define BGQ_PUEA_INT_SUMMARY_MU_MASK	(BGQ_PUEA_INT_SUMMARY_MU1_BIT \
	| BGQ_PUEA_INT_SUMMARY_MU2_BIT | BGQ_PUEA_INT_SUMMARY_MU3_BIT \
	| BGQ_PUEA_INT_SUMMARY_MU4_BIT)
/* UNUSED
#define BGQ_PUEA_INT_SUMMARY_GEA_MASK	0x00000000007fff80ULL
#define BGQ_PUEA_INT_SUMMARY_L1P_MASK	0x000000000000007cULL
*/
#define BQG_PUEA_INT_SUMMARY_C2C_INT0	0x0000000000000002ULL
#define BQG_PUEA_INT_SUMMARY_C2C_INT1	0x0000000000000001ULL
#define BGQ_PUEA_INT_SUMMARY_C2C_MASK (BQG_PUEA_INT_SUMMARY_C2C_INT0 | BQG_PUEA_INT_SUMMARY_C2C_INT1)

/* GEA lanes get mapped as follows:
 * 8: Devbus interrupts
 * 4-7: MSI interrupts 0-3
 * 9: Test int interrupts
 * 15: Message Unit global interrupt 6 */
#define BGQ_PUEA_INT_SUMMARY_DEVBUS_BIT	0x0000000000004000ULL
#define BGQ_PUEA_INT_SUMMARY_MSI_BIT0	0x0000000000040000ULL
#define BGQ_PUEA_INT_SUMMARY_MSI_BIT(x)	(BGQ_PUEA_INT_SUMMARY_MSI_BIT0 >> ((x) & 3))
#define BGQ_PUEA_INT_SUMMARY_TESTINT_BIT 0x0000000000002000ULL
#define BGQ_PUEA_INT_SUMMARY_GEA_MU6	0x0000000000000080ULL

extern void bgq_msi_init(dcr_host_t devbus);
extern int bgq_msi_get_irq(u8 msi_reg);
extern int bgq_msi_get_irq(u8 msi_reg);

#endif /* __PLATFORM_BGQ_BIG_H */
