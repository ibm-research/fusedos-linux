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

#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/interrupt.h>

#include <asm/cputhreads.h>

#include "bgq.h"
#include "bic.h"

#include "fusedos.h"                      // FUSEDOS
extern fusedos_config_t* fusedos_config;  // FUSEDOS
extern void (*spc_ipi_fp)(int, uint64_t); // FUSEDOS
static DEFINE_SPINLOCK(bgq_ipi_lock);     // FUSEDOS

static const char *bgq_pic_compat = "ibm,bgq-bic";

static const char * const bgq_irq_name[] = {
	[BGQ_IRQ_IPI] = "IPI",
	[BGQ_IRQ_INTA] = "INTA",
	[BGQ_IRQ_INTB] = "INTB",
	[BGQ_IRQ_INTC] = "INTC",
	[BGQ_IRQ_INTD] = "INTD",
	[BGQ_IRQ_MU1] = "MU1",
	[BGQ_IRQ_MU2] = "MU2",
	[BGQ_IRQ_MU3] = "MU3",
	[BGQ_IRQ_MU4] = "MU4",
	[BGQ_IRQ_TESTINT] = "INBOX",
	[BGQ_IRQ_MU6] = "MU6",
};

struct bgq_bic bgq_bic;

static inline u64 bgq_ext_int_summary(struct bgq_bic_puea *p, unsigned tid)
{
	return in_be64(&p->ext_int_summary[tid]);
}

#ifdef CONFIG_SMP
void bgq_cause_ipi(int cpu, unsigned long data)
{
	u64 val;

	/* raise IPI for target CPU */
	val = cpu_thread_in_core(cpu) + 1;
	val |= BGQ_BIC_C2C_INTTYPE_EXTERNAL << (63 - 60);
	val |= 0x0000000000200000ULL >> cpu_core_index_of_thread(cpu);

	out_be64(&bgq_bic.puea->interrupt_send, val);
}

static irqreturn_t bgq_ipi_dispatch(int irq, void *dev)
{
	struct bgq_bic *bic = dev;
	u64 isum;
	unsigned cpu = smp_processor_id();
	unsigned tid = cpu_thread_in_core(cpu);
	u64 c2c;
	int cleared = 0;
        unsigned long flags;    // FUSEDOS
        u64 spc_c2c_status = 0; // FUSEDOS

	isum = bgq_ext_int_summary(bic->puea, tid);

	/* Clear the interrupt, apparently it could be either/or */
	if (isum & BQG_PUEA_INT_SUMMARY_C2C_INT0) {
		c2c = in_be64(&bic->puea->clear_external_reg_0[tid]);
                spc_c2c_status = c2c << fusedos_config->nr_gpcs;        // FUSEDOS
		out_be64(&bic->puea->clear_external_reg_0[tid], c2c);
		++cleared;
	}
	if (isum & BQG_PUEA_INT_SUMMARY_C2C_INT1) {
		c2c = in_be64(&bic->puea->clear_external_reg_1[tid]);
                spc_c2c_status |= c2c << (fusedos_config->nr_gpcs - 4); // FUSEDOS
		out_be64(&bic->puea->clear_external_reg_1[tid], c2c);
		++cleared;
	}
	BUG_ON(!cleared);

        spin_lock_irqsave(&bgq_ipi_lock, flags);                                      // FUSEDOS
        if (spc_ipi_fp != NULL && spc_c2c_status) (*spc_ipi_fp)(cpu, spc_c2c_status); // FUSEDOS
        spin_unlock_irqrestore(&bgq_ipi_lock, flags);                                 // FUSEDOS

	return smp_ipi_demux();
}
#endif	/* CONFIG_SMP */

static unsigned int bgq_get_irq(void)
{
	struct bgq_bic *bic = &bgq_bic;
	unsigned cpu;
	unsigned tid;
	u64 isum;
	u64 handled_mask;
	int irqnum;
	u64 db;

	cpu = smp_processor_id();
	tid = cpu_thread_in_core(cpu);

	/* check for critical and machine check first */
	isum = in_be64(&bic->puea->crit_int_summary[tid]);
	if (isum) {
		pr_emerg("Critical interrupt! 0x%llx\n", isum);
		BUG();
		return NO_IRQ;
	}

	isum = in_be64(&bic->puea->mach_int_summary[tid]);
	if (isum) {
		pr_emerg("Machine check! 0x%llx\n", isum);
		BUG();
		return NO_IRQ;
	}

	isum = bgq_ext_int_summary(bic->puea, tid);

	/* quickly check if we do handle the interrupt, or not */
	handled_mask = BGQ_PUEA_INT_SUMMARY_DEVBUS_BIT | BGQ_PUEA_INT_SUMMARY_TESTINT_BIT;
#ifdef CONFIG_SMP
	handled_mask |= BGQ_PUEA_INT_SUMMARY_C2C_MASK;
#endif
#ifdef CONFIG_PCI_MSI
	handled_mask |= BGQ_PUEA_INT_SUMMARY_MSI_BIT(0) | BGQ_PUEA_INT_SUMMARY_MSI_BIT(1);
	handled_mask |= BGQ_PUEA_INT_SUMMARY_MSI_BIT(2) | BGQ_PUEA_INT_SUMMARY_MSI_BIT(3);
#endif
#ifdef CONFIG_MU
	handled_mask |= BGQ_PUEA_INT_SUMMARY_GEA_MU6 | BGQ_PUEA_INT_SUMMARY_MU_MASK;
#endif
	if (isum & ~handled_mask) {
		pr_emerg("Unknown external interrupt 0x%llx\n", isum);
		BUG();
		return NO_IRQ;
	}

	if(!isum) {
	    return NO_IRQ;
	}

	irqnum = __builtin_clzl(isum);

	switch(irqnum) {
#ifdef CONFIG_PCI_MSI
	case BGQ_IRQ_IPI:
	case BGQ_IRQ_IPI1:
		return bic->vipi;
		break;

	case BGQ_IRQ_MSI0: /* fall-through to common case */
	case BGQ_IRQ_MSI1:
	case BGQ_IRQ_MSI2:
	case BGQ_IRQ_MSI3:
		return bgq_msi_get_irq (irqnum - BGQ_IRQ_MSI0);
		break;
#endif
	case BGQ_IRQ_DEVBUS:
		db = dcr_read64(bic->dcr_devbus,
				    BGQ_DCR_DEVBUS_INTERRUPT_STATE);
		switch(db) {
		case 0x800:
			return irq_linear_revmap(bic->domain, BGQ_IRQ_INTA);
		case 0x400:
			return irq_linear_revmap(bic->domain, BGQ_IRQ_INTB);
		case 0x200:
			return irq_linear_revmap(bic->domain, BGQ_IRQ_INTC);
		case 0x100:
			return irq_linear_revmap(bic->domain, BGQ_IRQ_INTD);
		default:
			return NO_IRQ;
		}
		break;

	default: /* testint and message unit interrupts */
		return irq_linear_revmap(bic->domain, irqnum);
	}
}

static int bgq_host_match(struct irq_domain *h, struct device_node *node)
{
	int rc;

	rc = of_device_is_compatible(node, bgq_pic_compat);
	if (rc)
		h->of_node = node;

	return rc;
}

static void bgq_mask_irq(struct irq_data *d)
{
	unsigned irq = d->hwirq;
	struct bgq_bic *bic = &bgq_bic;
	unsigned tid;
	u64 v;
	u64 state;

	switch(irq) {
	case BGQ_IRQ_IPI:
		break;

	/* Mailbox interrupt */
	case BGQ_IRQ_TESTINT:
		bgq_inbox_mask_irq();
		break;

	/* Message Unit interrupts
	 * we mask all of them at the PUEA (local to the CPU core) */
	case BGQ_IRQ_MU1:
	case BGQ_IRQ_MU2:
	case BGQ_IRQ_MU3:
	case BGQ_IRQ_MU4:
		/* UNUSED: we only enable MU interrupts on core 0, thread 0
		 * cpu = smp_processor_id();
		 * tid = cpu_thread_in_core(cpu);
		 */
		tid = 0;
		v = in_be64(&bic->puea->map_interrupt[tid]);
		v &= ~( 0x0001000000000000 >> ( (irq-BGQ_IRQ_MU1) * 2 ) );
		out_be64(&bic->puea->map_interrupt[tid], v);
		break;
	
	case BGQ_IRQ_MU6:
		/* UNUSED: we only enable MU interrupts on core 0, thread 0
		 * cpu = smp_processor_id();
		 * tid = cpu_thread_in_core(cpu);
		 */
		tid = 0;
		v = in_be64(&bic->puea->map_interrupt[tid]);
		v &= ~0x0000000000000400;
		out_be64(&bic->puea->map_interrupt[tid], v);
		break;
	
	/* DEVBUS interrupts */
	case BGQ_IRQ_INTA:
	case BGQ_IRQ_INTB:
	case BGQ_IRQ_INTC:
	case BGQ_IRQ_INTD:
		v = dcr_read64(bic->dcr_devbus,
			       BGQ_DCR_DEVBUS_INTERRUPT_STATE_CONTROL_LOW);
		v &= ~ ( 0xc00000ULL >> ( (irq-BGQ_IRQ_INTA) * 2 ) );
		state = 0x800ULL >> (irq-BGQ_IRQ_INTA);
		dcr_write64(bic->dcr_devbus,
			    BGQ_DCR_DEVBUS_INTERRUPT_STATE_CONTROL_LOW, v);
		/* EOI it too */
		dcr_write64(bic->dcr_devbus, BGQ_DCR_DEVBUS_INTERRUPT_STATE, state);
		break;
	
	default:
		/* Unknown IRQ */
		pr_emerg("%s: unexpected IRQ %d\n", __func__, irq);
		BUG();
	}
}

static void bgq_unmask_irq(struct irq_data *d)
{
	unsigned irq = d->hwirq;
	struct bgq_bic *bic = &bgq_bic;
	unsigned tid;
	u64 v;

	switch(irq) {
	case BGQ_IRQ_IPI:
		break;
	
	case BGQ_IRQ_TESTINT:
		bgq_inbox_unmask_irq();
		break;

	/* Message Unit interrupts
	 * we mask all of them at the PUEA (local to the CPU core) */
	case BGQ_IRQ_MU1:
	case BGQ_IRQ_MU2:
	case BGQ_IRQ_MU3:
	case BGQ_IRQ_MU4:
		/* UNUSED: we only enable MU interrupts on core 0, thread 0
		 * cpu = smp_processor_id();
		 * tid = cpu_thread_in_core(cpu);
		 */
		tid = 0;
		v = in_be64(&bic->puea->map_interrupt[tid]);
		v |=  0x0001000000000000 >> ( (irq-BGQ_IRQ_MU1) * 2 );
		out_be64(&bic->puea->map_interrupt[tid], v);
		break;
	case BGQ_IRQ_MU6:
		/* UNUSED: we only enable MU interrupts on core 0, thread 0
		 * cpu = smp_processor_id();
		 * tid = cpu_thread_in_core(cpu);
		 */
		tid = 0;
		v = in_be64(&bic->puea->map_interrupt[tid]);
		v |= 0x0000000000000400;
		out_be64(&bic->puea->map_interrupt[tid], v);
		break;

	/* DEVBUS interrupts */
	case BGQ_IRQ_INTA:
	case BGQ_IRQ_INTB:
	case BGQ_IRQ_INTC:
	case BGQ_IRQ_INTD:
		v = dcr_read64(bic->dcr_devbus,
			       BGQ_DCR_DEVBUS_INTERRUPT_STATE_CONTROL_LOW);
		v |= 0xc00000ULL >> ( (irq-BGQ_IRQ_INTA) * 2 );
		dcr_write64(bic->dcr_devbus,
			    BGQ_DCR_DEVBUS_INTERRUPT_STATE_CONTROL_LOW, v);
		break;

	default:
		/* Unknown IRQ */
		pr_emerg("%s: unexpected IRQ %d\n", __func__, irq);
		BUG();
	}
}

static void bgq_eoi(struct irq_data *d)
{
	unsigned irq = d->hwirq;
	struct bgq_bic *bic = &bgq_bic;
	u64 state;

	switch(irq) {
	case BGQ_IRQ_IPI:
		break;
	
	case BGQ_IRQ_TESTINT:
		bgq_inbox_eoi();
		break;

	/* Message Unit interrupts */
	case BGQ_IRQ_MU1:
	case BGQ_IRQ_MU2:
	case BGQ_IRQ_MU3:
	case BGQ_IRQ_MU4:
	case BGQ_IRQ_MU6:
		    /* other BGQ Linux implementations do not eoi the MU interrupts
		    * ... do we need that?! */
		    break;

	/* DEVBUS interrupts */
	case BGQ_IRQ_INTA:
	case BGQ_IRQ_INTB:
	case BGQ_IRQ_INTC:
	case BGQ_IRQ_INTD:
		state = 0x800ULL >> (irq-BGQ_IRQ_INTA);
		dcr_write64(bic->dcr_devbus, BGQ_DCR_DEVBUS_INTERRUPT_STATE, state);
		break;

	default:
		/* Unknown IRQ */
		pr_emerg("%s: unexpected IRQ %d\n", __func__, irq);
		BUG();
	}
}

static struct irq_chip bgq_irq_chip = {
	.name = "Blue Gene/Q Interrupt Controller",
	.irq_mask = bgq_mask_irq,
	.irq_unmask = bgq_unmask_irq,
	.irq_ack = bgq_eoi,
};

/* sense map */
static unsigned bgq_bic_sense_map[] = {
	[0] = IRQ_TYPE_LEVEL_LOW,
	[1] = IRQ_TYPE_LEVEL_HIGH,
	[2] = IRQ_TYPE_EDGE_RISING,
};

static int bgq_host_map(struct irq_domain *h, unsigned int virq,
			irq_hw_number_t hw)
{
	const char *n;

	if (hw > ARRAY_SIZE(bgq_irq_name))
		return -EINVAL;

	n = bgq_irq_name[hw];
	if (n == NULL)
		return -EINVAL;

	irq_set_chip_data(virq, h->host_data);
	irq_set_chip_and_handler_name(virq, &bgq_irq_chip, handle_level_irq,
				      n);
	return 0;
}

static int bgq_host_xlate(struct irq_domain *h, struct device_node *ct,
			  const u32 *intspec, unsigned int intsize,
			  irq_hw_number_t *out_hwirq,
			  unsigned int *out_flags)

{
	unsigned sense;

	if (intsize > 2) {
		pr_info("%s: intspec is too large for %s\n", __func__,
			ct->full_name);
	}

	if (intspec[0] >= BGQ_IRQ_COUNT) {
		pr_err("%s: IRQ is too large for %s\n", __func__,
		       ct->full_name);
		return -EINVAL;
	}

	sense = intspec[1];
	if (sense >= ARRAY_SIZE(bgq_bic_sense_map)) {
		pr_warn("%s: sense value is too large for %s\n", __func__,
			ct->full_name);
		sense &= 0x1;
	}
	*out_hwirq = intspec[0];
	*out_flags = bgq_bic_sense_map[sense];

	return 0;
}

static struct irq_domain_ops bgq_domain_ops = {
	.match = bgq_host_match,
	.map = bgq_host_map,
	.xlate = bgq_host_xlate,
};

static void __init bgq_init_host(struct bgq_bic *bic, struct device_node *dn)
{
	/* should we simply be using "direct"? */
	bic->domain = irq_domain_add_linear(dn, BGQ_IRQ_COUNT,
					    &bgq_domain_ops, bic);
	BUG_ON(bic->domain == NULL);

	irq_set_default_host(bic->domain);
}

static void bgq_bic_map(struct bgq_bic *bic, unsigned master)
{
	struct device_node *dn;

	/* do this one first, because we need to return the other */
	dn = of_find_compatible_node(NULL, NULL, "ibm,bgq-devbus");
	if (!dn)
		panic("%s: failed to find the device bus\n", __func__);

	if (bgq_dcr_map(dn, &bic->dcr_devbus))
		panic("%s: dcr_map failed for %s\n", __func__, dn->full_name);

	of_node_put(dn);

	dn = of_find_compatible_node(NULL, NULL, bgq_pic_compat);
	if (!dn)
		panic("%s: failed to find interrupt controller\n", __func__);

	bic->puea = of_iomap(dn, 0);
	if (!bic->puea)
		panic("%s: of_iomap(%s) failed\n", __func__, dn->full_name);

	if (bgq_dcr_map(dn, &bic->dcr_gea))
		panic("%s: dcr_map failed for %s\n", __func__, dn->full_name);

	bgq_init_host(bic, dn);
	of_node_put(dn);

	bic->master_core = master;
}

static void bgq_bic_gea_init(struct bgq_bic *bic)
{
	u64 val;

	/* Configure the GEA. */
	val = dcr_read64(bic->dcr_gea, BGQ_DCR_GEA_INTERRUPT_MAP(3));
	BUG_ON(val & 0xff00U);

	/* Use lane 8 for devbus interrupts. */
	val |= 0x8800U;

#ifdef CONFIG_PCI_MSI
	/* Use lanes 4-7 for MSI interrupts 0-3. */
	BUG_ON(val & 0xffff000000000000ULL);
	val |= 0x4567000000000000ULL;
#endif
	dcr_write64(bic->dcr_gea, BGQ_DCR_GEA_INTERRUPT_MAP(3), val);

	/* lane 9 for test int interrupts */
	val = dcr_read64(bic->dcr_gea, BGQ_DCR_GEA_INTERRUPT_MAP(8));
	BUG_ON(val & 0xfff000000000ULL);
	val |= 0x009000000000ULL;
	dcr_write64(bic->dcr_gea, BGQ_DCR_GEA_INTERRUPT_MAP(8), val);

#ifdef CONFIG_MU
	/* lane 15 for global MU interrupt 6 (used to signal fatal and
	 * non-fatal error  conditions) */
	val = dcr_read64(bic->dcr_gea, BGQ_DCR_GEA_INTERRUPT_MAP(1));
	val |= 0x0000000f00000000ULL;
	dcr_write64(bic->dcr_gea, BGQ_DCR_GEA_INTERRUPT_MAP(1), val);
#endif
}

static void bgq_bic_init(struct bgq_bic_puea __iomem *p, unsigned master)
{
	u64 val;
	int t;

	/* right now we always route interrupt to core 0 */
	BUG_ON(master);

	/* Route GEA lane(s) to core 0, thread 0. */
	val = in_be64(&p->map_interrupt[0]);
	/* lane 8 & 9 external */
	val |= 0x0000000001400000ULL;
#ifdef CONFIG_PCI_MSI
	/* lane 4-7 external */
	val |= 0x0000000154000000ULL;
#endif
#ifdef CONFIG_MU
	val |= 0x0001540000000000ULL; // Message unit interrupts directed at PUEA
	val |= 0x0000000000000400ULL; // global message unit interrupt 6 routed via GEA lane 15
#endif
	out_be64(&p->map_interrupt[0], val);

	/* Clear all PUEA interrupt indication registers. */
	for (t = 0; t < 4; t++) {
		out_be64(&p->clear_external_reg_0[t], ~0ULL);
		out_be64(&p->clear_critical_reg_0[t], ~0ULL);
		out_be64(&p->clear_wakeup_reg_0[t], ~0ULL);
		out_be64(&p->clear_external_reg_1[t], ~0ULL);
		out_be64(&p->clear_critical_reg_1[t], ~0ULL);
		out_be64(&p->clear_wakeup_reg_1[t], ~0ULL);
	}
}

static void bgq_ipi_map(struct bgq_bic *bic)
{
#ifdef CONFIG_SMP
	unsigned int irq;
	int rc;

	irq = irq_create_mapping(bic->domain, BGQ_IRQ_IPI);
	BUG_ON(irq == NO_IRQ);
	irq_set_chip_and_handler(irq, &bgq_irq_chip, handle_percpu_irq);
	rc = request_irq(irq, bgq_ipi_dispatch, IRQF_PERCPU,
			 bgq_irq_name[BGQ_IRQ_IPI], bic);
	BUG_ON(rc);

	irq_set_irq_type(irq, IRQ_TYPE_LEVEL_HIGH);
	/*
	 * stow the irq mapping locally, there is no need to reverse
	 * map it all the time.
	 */
	bic->vipi = irq;
#endif	/* CONFIG_SMP */
}

#ifdef CONFIG_MU
static void bgq_mu_int_map(struct bgq_bic *bic)
{
	unsigned int irq;
	
	irq = irq_create_mapping(bic->domain, BGQ_IRQ_MU1);
	BUG_ON(irq == NO_IRQ);
	irq = irq_create_mapping(bic->domain, BGQ_IRQ_MU2);
	BUG_ON(irq == NO_IRQ);
	irq = irq_create_mapping(bic->domain, BGQ_IRQ_MU3);
	BUG_ON(irq == NO_IRQ);
	irq = irq_create_mapping(bic->domain, BGQ_IRQ_MU4);
	BUG_ON(irq == NO_IRQ);
	irq = irq_create_mapping(bic->domain, BGQ_IRQ_MU6);
	BUG_ON(irq == NO_IRQ);
}
#endif

void __init bgq_init_IRQ(void)
{
	bgq_bic_map(&bgq_bic, 0);
	bgq_bic_gea_init(&bgq_bic);
	bgq_bic_init(bgq_bic.puea, bgq_bic.master_core);

	bgq_ipi_map(&bgq_bic);

#ifdef CONFIG_PCI_MSI
	bgq_msi_init(bgq_bic.dcr_devbus);
#endif
#ifdef CONFIG_MU
	bgq_mu_int_map(&bgq_bic);
#endif

	ppc_md.get_irq = bgq_get_irq;
}
