/* map.h -- Defines the logical memory map.
   John Harper. */

#ifndef _VMM_MAP_H
#define _VMM_MAP_H

/* Physical vs. Logical Addresses
   ==============================

   A physical address is the *true* address of a piece of memory, as
   opposed to its logical address which is where the piece of memory
   appears to be (as defined by the segmentation and paging tables which
   are currently active).

   It is not possible for the kernel to access the contents of a physical
   address directly (since no direct mapping between logical and physical
   addresses exists); instead an image of the whole extent of the physical
   memory in the system will exist at the address PHYS_MAP_ADDR (defined
   below). Physical addresses are defined as the type `u_long' in the
   kernel -- this is an attempt to prohibit direct accesses to physical
   addresses.

   Two macros are defined below to allow conversion of logical <->
   physical addresses. TO_LOGICAL(x,t) converts the physical address X (a
   u_long) to a logical address of type T at which the contents of physical
   address X may be accessed (by the kernel). TO_PHYSICAL(x) converts the
   logical address (of a piece of physical memory as created by TO_LOGICAL())
   to a physical address (of type u_long).

   In fact the linear address space of each virtual machine or task will
   look like the following diagram:

      4G  +------------------+ -------------> +------------------+  4G
          | Kernel space     |                |                  |
   K_B_A  +------------------+  --            |                  |
          |                  |    \           |                  |
          |                  |     \          + - - - - - - - - -+
          |                  |      \         | Mapping of       |
          |                  |       \        |  physical mem.   |
          |                  |        \       +------------------+  P_M_A
          |                  |         \      |                  |
          |                  |          \     | Dynamic kernel   |
          |- - - - - - - - - |           \    |  code & data     |
          |                  |            \   + - - - - - - - - -+
          | VM's virtual     |             \  | Static Kernel    |
          |  memory.         |              \ |  code & data     |
       0  +------------------+                +------------------+  K_B_A

   [ K_B_A == KERNEL_BASE_ADDR (== $F800000)
     P_M_A == PHYS_MAP_ADDR (== $F880000)    ]

   Also note that the kernel will have its own segment(s), mapping the
   diagram on the right to logical address zero.

   (Dotted lines denote boundaries which aren't set in stone.) */

/* The number of page tables that the kernel needs to map all this stuff. */
#define NUM_KERNEL_PAGE_TABLES (128 / 4)

/* Memory above the following logical address is reserved for kernel code
   and data (including all dynamically allocated stuff).
   This gives the kernel 8M of virtual space. */
#define KERNEL_BASE_ADDR 0xF8000000

/* All of the available physical memory is mapped into a task's logical
   address space at the following address. Since the kernel takes up 8M
   of the available 128M the most physical memory this layout can handle
   is 120M... */
#define PHYS_MAP_ADDR    0xF8800000

/* Convert the physical address (i.e. 0->?M) X to the logical address
   of X. The resulting value will have the type T */
#define TO_LOGICAL(x,t) ((t)((x) + (8*1024*1024)))

/* Convert the logical address of a piece of physical memory X to it's
   actual physical address. The returned value will be a u_long. */
#define TO_PHYSICAL(x) (((u_long)(x)) - (8*1024*1024))

/* Converts an address in the kernel space X to the corresponding linear
   address. */
#define TO_LINEAR(x) ((u_long)(x) + KERNEL_BASE_ADDR)

/* Converts the linear address of a byte in the kernel space X to its
   logical address in the kernel space. */
#define TO_KERNEL(x) ((x) - KERNEL_BASE_ADDR)

#endif /* _VMM_MAP_H */
