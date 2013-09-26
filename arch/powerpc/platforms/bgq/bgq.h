/*
 * Blue Gene/Q Platform
 * authors:
 *    Andrew Tauferner <ataufer@us.ibm.com>
 *    Todd Inglett <tinglett@us.ibm.com>
 *    Jimi Xenidis <jimix@pobox.com>
 *    Eric Van Hensbergen <ericvh@gmail.com>
 *
 * Licensed Materials - Property of IBM
 *
 * Blue Gene/Q
 *
 * (c) Copyright IBM Corp. 2011, 2012 All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM
 * Corporation.
 *
 * This software is available to you under the GNU General Public
 * License (GPL) version 2.
 */

#ifndef __PLATFORM_BGQ_H
#define __PLATFORM_BGQ_H

#include <linux/pci.h>
#include <linux/device.h>

#include <asm/dcr.h>

extern void bgq_secondary_smp_init(void *data);

extern void bgq_setup_smp(void);
extern void __devinit bgq_setup_cpu(int cpu);
extern void bgq_init_IRQ(void);
extern int bgq_init_mu(phys_addr_t end_of_dram);
extern int bgq_dcr_map(struct device_node *dn, dcr_host_t *hostp);
extern void bgq_cause_ipi(int cpu, unsigned long data);

extern int bgq_inbox_poll(u32 vtermno, char *buf, int count);
extern int bgq_put_chars(u32 vtermno, const char *data, int count);
extern void udbg_init_bgq_early(void);
extern void bgq_inbox_mask_irq(void);
extern void bgq_inbox_unmask_irq(void);
extern void bgq_inbox_eoi(void);
extern void bgq_mailbox_init(void);
extern void bgq_halt(void);
extern void bgq_restart(char *s);
extern void bgq_panic(char *s);
extern int bgq_ras_puts(u32 id, const char *s);
extern int bgq_ras_write(u32 id, const void *data, u16 len);
extern int bgq_block_state(u16 status, u32 block_id);

extern u32 bgq_io_reset_block_id;

extern void bgq_pci_init(void);
extern void bgq_pcibios_fixup(void);
extern void bgq_dma_dev_setup(struct pci_dev *dev);

#endif	/* __PLATFORM_BGQ_H */
