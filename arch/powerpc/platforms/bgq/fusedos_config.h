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

#ifndef _LINUX_FUSEDOS_CONFIG_H_
#define _LINUX_FUSEDOS_CONFIG_H_

//#define USE_WAITRSV      // Use waitrsv instead of IPI to signal CL
//#define RETURN_ON_READ0  // Return immediately when asked to read 0 bytes
//#define SKIP_READ_CHECKS // Skip read sanity checks

#define THREADS_PER_CORE   4          // Must reflect Linux THREADS_PER_CORE

#define SPC_MONITOR_TVADDR	0x060000000 // Temp mapping for monitor, 1.5 GB

#define SPC_MONITOR_VADDR	0x000000000
#define SPC_CONTEXT_VADDR	0x000C00000 // 12 MB
#define FUSEDOS_CONFIG_VADDR	0x000F00000 // 15 MB
#define SPC_MEMORY_PADDR	0x040000000 // 1 GB

#define SPC_MONITOR_SIZE    0x000C00000 // 12 MB
#define SPC_CONTEXT_SIZE    0x000300000 // 3 MB
#define FUSEDOS_CONFIG_SIZE 0x000100000 // 1 MB
#if defined SMALL_SPC_MEMORY
#define SPC_MEMORY_SIZE	    0x002000000 // 32 MB
#else // SMALL_SPC_MEMORY
#define SPC_MEMORY_SIZE	    0x00C000000 // 192 MB
#endif // SMALL_SPC_MEMORY

#endif /* _LINUX_FUSEDOS_CONFIG_H_ */
