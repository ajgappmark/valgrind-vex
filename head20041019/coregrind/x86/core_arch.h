
/*--------------------------------------------------------------------*/
/*--- x86-specific stuff for the core.             x86/core_arch.h ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Valgrind, an extensible x86 protected-mode
   emulator for monitoring program execution on x86-Unixes.

   Copyright (C) 2000-2004 Nicholas Nethercote
      njn25@cam.ac.uk

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#ifndef __X86_CORE_ARCH_H
#define __X86_CORE_ARCH_H

#include "core_arch_asm.h"    // arch-specific asm  stuff
#include "tool_arch.h"        // arch-specific tool stuff

#include "../../../pub/libvex_guest_x86.h"


/* ---------------------------------------------------------------------
   Interesting registers
   ------------------------------------------------------------------ */

/* Generate a pointer into baseBlock via which we can prod the
   Vex guest state. */
#define BASEBLOCK_VEX  \
   ((VexGuestX86State*)(&VG_(baseBlock)[VGOFF_(m_vex)]))

// Accessors for the arch_thread_t
#define ARCH_INSTR_PTR(regs)           ((regs).vex.guest_EIP)
#define ARCH_STACK_PTR(regs)           ((regs).vex.guest_ESP)
#define ARCH_FRAME_PTR(regs)           ((regs).vex.guest_EBP)

#define ARCH_CLREQ_ARGS(regs)          ((regs).vex.guest_EAX)
#define ARCH_PTHREQ_RET(regs)          ((regs).vex.guest_EDX)
#define ARCH_CLREQ_RET(regs)           ((regs).vex.guest_EDX)

// Accessors for the baseBlock
#define R_STACK_PTR                    R_ESP
#define R_FRAME_PTR                    R_EBP

#define R_CLREQ_RET                    R_EDX
#define R_PTHREQ_RET                   R_EDX

// Stack frame layout and linkage
#define FIRST_STACK_FRAME(ebp)         (ebp)
#define STACK_FRAME_RET(ebp)           (((UInt*)ebp)[1])
#define STACK_FRAME_NEXT(ebp)          (((UInt*)ebp)[0])

// Baseblock access to interesting registers
#define BASEBLOCK_INSTR_PTR            BASEBLOCK_VEX->guest_EIP
#define BASEBLOCK_STACK_PTR            BASEBLOCK_VEX->guest_ESP
#define BASEBLOCK_FRAME_PTR            BASEBLOCK_VEX->guest_EBP

// Get stack pointer and frame pointer
#define ARCH_GET_REAL_STACK_PTR(esp) do {   \
   asm("movl %%esp, %0" : "=r" (esp));       \
} while (0)

#define ARCH_GET_REAL_FRAME_PTR(ebp) do {   \
   asm("movl %%ebp, %0" : "=r" (ebp));       \
} while (0)


/* -----------------------------------------------------
   Read-write parts of baseBlock.
   -------------------------------------------------- */

/* State of the simulated CPU. */
extern Int VGOFF_(m_vex);

/* Reg-alloc spill area (VG_MAX_SPILLSLOTS words long). */
extern Int VGOFF_(spillslots);

/* Records the valid bits for the 8 integer regs & flags reg. */
extern Int VGOFF_(sh_eax);
extern Int VGOFF_(sh_ecx);
extern Int VGOFF_(sh_edx);
extern Int VGOFF_(sh_ebx);
extern Int VGOFF_(sh_esp);
extern Int VGOFF_(sh_ebp);
extern Int VGOFF_(sh_esi);
extern Int VGOFF_(sh_edi);
extern Int VGOFF_(sh_eflags);

/* -----------------------------------------------------
   Read-only parts of baseBlock.
   -------------------------------------------------- */

/* This thread's LDT pointer. */
extern Int VGOFF_(ldt);

/* This thread's TLS pointer. */
extern Int VGOFF_(tls_ptr);

/* Nb: Most helper offsets are in include/tool.h, for use by tools */
extern Int VGOFF_(helper_undefined_instruction);


/* ---------------------------------------------------------------------
   Elf stuff
   ------------------------------------------------------------------ */

#define VG_ELF_ENDIANNESS     ELFDATA2LSB
#define VG_ELF_MACHINE        EM_386       
#define VG_ELF_CLASS          ELFCLASS32


/* ---------------------------------------------------------------------
   Exports of vg_helpers.S
   ------------------------------------------------------------------ */

extern void VG_(helper_IN);
extern void VG_(helper_OUT);

extern void VG_(helper_RDTSC);
extern void VG_(helper_CPUID);

extern void VG_(helper_DAS);
extern void VG_(helper_DAA);
extern void VG_(helper_AAS);
extern void VG_(helper_AAA);
extern void VG_(helper_AAD);
extern void VG_(helper_AAM);

extern void VG_(helper_cmpxchg8b);

/* ---------------------------------------------------------------------
   LDT type             
   ------------------------------------------------------------------ */

// XXX: eventually this will be x86-private, not seen by the core(?)

/* This is the hardware-format for a segment descriptor, ie what the
   x86 actually deals with.  It is 8 bytes long.  It's ugly.  */

typedef struct _LDT_ENTRY {
    union {
       struct {
          UShort      LimitLow;
          UShort      BaseLow;
          unsigned    BaseMid         : 8;
          unsigned    Type            : 5;
          unsigned    Dpl             : 2;
          unsigned    Pres            : 1;
          unsigned    LimitHi         : 4;
          unsigned    Sys             : 1;
          unsigned    Reserved_0      : 1;
          unsigned    Default_Big     : 1;
          unsigned    Granularity     : 1;
          unsigned    BaseHi          : 8;
       } Bits;
       struct {
          UInt word1;
          UInt word2;
       } Words;
    } 
    LdtEnt;
} VgLdtEntry;

/* ---------------------------------------------------------------------
   Constants pertaining to the simulated CPU state, VG_(baseBlock),
   which need to go here to avoid ugly circularities.
   ------------------------------------------------------------------ */

/* How big is the saved SSE/SSE2 state?  Note that this subsumes the
   FPU state.  On machines without SSE, we just save/restore the FPU
   state into the first part of this area. */
/* A general comment about SSE save/restore: It appears that the 7th
   word (which is the MXCSR) has to be &ed with 0x0000FFBF in order
   that restoring from it later does not cause a GP fault (which is
   delivered as a segfault).  I guess this will have to be done
   any time we do fxsave :-(  7th word means word offset 6 or byte
   offset 24 from the start address of the save area.
 */
//#define VG_SIZE_OF_SSESTATE 512
/* ... and in words ... */
//#define VG_SIZE_OF_SSESTATE_W ((VG_SIZE_OF_SSESTATE+3)/4)


// Architecture-specific part of a ThreadState
// XXX: eventually this should be made abstract, ie. the fields not visible
//      to the core...  then VgLdtEntry can be made non-visible to the core
//      also.
typedef struct {
   /* Pointer to this thread's Local (Segment) Descriptor Table.
      Starts out as NULL, indicating there is no table, and we hope to
      keep it that way.  If the thread does __NR_modify_ldt to create
      entries, we allocate a 8192-entry table at that point.  This is
      a straight copy of the Linux kernel's scheme.  Don't forget to
      deallocate this at thread exit. */
   VgLdtEntry* ldt;

   /* TLS table. This consists of a small number (currently 3) of
      entries from the Global Descriptor Table. */
   VgLdtEntry tls[VKI_GDT_TLS_ENTRIES];

   /* Saved machine context. */
   VexGuestX86State vex;

   UInt sh_eax;
   UInt sh_ebx;
   UInt sh_ecx;
   UInt sh_edx;
   UInt sh_esi;
   UInt sh_edi;
   UInt sh_ebp;
   UInt sh_esp;
   UInt sh_eflags;
} 
arch_thread_t;


/* ---------------------------------------------------------------------
   Miscellaneous constants
   ------------------------------------------------------------------ */

// Total number of spill words available for register allocation.
#define VG_MAX_SPILLSLOTS     100

// Valgrind's signal stack size, in words.
#define VG_SIGSTACK_SIZE_W    10000

// Base address of client address space.
#define CLIENT_BASE	0x00000000ul

#endif   // __X86_CORE_ARCH_H

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
