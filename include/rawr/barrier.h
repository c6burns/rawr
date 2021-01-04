/*
 * $Id: pa_memorybarrier.h 1240 2007-07-17 13:05:07Z bjornroche $
 * Portable Audio I/O Library
 * Memory barrier utilities
 *
 * Author: Bjorn Roche, XO Audio, LLC
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire PortAudio license; however, 
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also 
 * requested that these non-binding requests be included along with the 
 * license above.
 */

/**
 @file pa_memorybarrier.h
 @ingroup common_src
*/

/****************
 * Some memory barrier primitives based on the system.
 * right now only OS X, FreeBSD, and Linux are supported. In addition to providing
 * memory barriers, these functions should ensure that data cached in registers
 * is written out to cache where it can be snooped by other CPUs. (ie, the volatile
 * keyword should not be required)
 *
 * the primitives that must be defined are:
 *
 * rawr_MemoryBarrier_Full()
 * rawr_MemoryBarrier_Read()
 * rawr_MemoryBarrier_Write()
 *
 ****************/

#if defined(__APPLE__)
#   include <libkern/OSAtomic.h>
    /* Here are the memory barrier functions. Mac OS X only provides
       full memory barriers, so the three types of barriers are the same,
       however, these barriers are superior to compiler-based ones. */
#   define rawr_MemoryBarrier_Full()  OSMemoryBarrier()
#   define rawr_MemoryBarrier_Read()  OSMemoryBarrier()
#   define rawr_MemoryBarrier_Write() OSMemoryBarrier()
#elif defined(__GNUC__)
    /* GCC >= 4.1 has built-in intrinsics. We'll use those */
#   if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#      define rawr_MemoryBarrier_Full()  __sync_synchronize()
#      define rawr_MemoryBarrier_Read()  __sync_synchronize()
#      define rawr_MemoryBarrier_Write() __sync_synchronize()
    /* as a fallback, GCC understands volatile asm and "memory" to mean it
     * should not reorder memory read/writes */
    /* Note that it is not clear that any compiler actually defines __PPC__,
     * it can probably removed safely. */
#   elif defined( __ppc__ ) || defined( __powerpc__) || defined( __PPC__ )
#      define rawr_MemoryBarrier_Full()  asm volatile("sync":::"memory")
#      define rawr_MemoryBarrier_Read()  asm volatile("sync":::"memory")
#      define rawr_MemoryBarrier_Write() asm volatile("sync":::"memory")
#   elif defined( __i386__ ) || defined( __i486__ ) || defined( __i586__ ) || \
         defined( __i686__ ) || defined( __x86_64__ )
#      define rawr_MemoryBarrier_Full()  asm volatile("mfence":::"memory")
#      define rawr_MemoryBarrier_Read()  asm volatile("lfence":::"memory")
#      define rawr_MemoryBarrier_Write() asm volatile("sfence":::"memory")
#   else
#      ifdef ALLOW_SMP_DANGERS
#         warning Memory barriers not defined on this system or system unknown
#         warning For SMP safety, you should fix this.
#         define rawr_MemoryBarrier_Full()
#         define rawr_MemoryBarrier_Read()
#         define rawr_MemoryBarrier_Write()
#      else
#         error Memory barriers are not defined on this system. You can still compile by defining ALLOW_SMP_DANGERS, but SMP safety will not be guaranteed.
#      endif
#   endif
#elif (_MSC_VER >= 1400) && !defined(_WIN32_WCE)
#   include <intrin.h>
#   pragma intrinsic(_ReadWriteBarrier)
#   pragma intrinsic(_ReadBarrier)
#   pragma intrinsic(_WriteBarrier)
/* note that MSVC intrinsics _ReadWriteBarrier(), _ReadBarrier(), _WriteBarrier() are just compiler barriers *not* memory barriers */
#   define rawr_MemoryBarrier_Full()  _ReadWriteBarrier()
#   define rawr_MemoryBarrier_Read()  _ReadBarrier()
#   define rawr_MemoryBarrier_Write() _WriteBarrier()
#elif defined(_WIN32_WCE)
#   define rawr_MemoryBarrier_Full()
#   define rawr_MemoryBarrier_Read()
#   define rawr_MemoryBarrier_Write()
#elif defined(_MSC_VER) || defined(__BORLANDC__)
#   define rawr_MemoryBarrier_Full()  _asm { lock add    [esp], 0 }
#   define rawr_MemoryBarrier_Read()  _asm { lock add    [esp], 0 }
#   define rawr_MemoryBarrier_Write() _asm { lock add    [esp], 0 }
#else
#   ifdef ALLOW_SMP_DANGERS
#      warning Memory barriers not defined on this system or system unknown
#      warning For SMP safety, you should fix this.
#      define rawr_MemoryBarrier_Full()
#      define rawr_MemoryBarrier_Read()
#      define rawr_MemoryBarrier_Write()
#   else
#      error Memory barriers are not defined on this system. You can still compile by defining ALLOW_SMP_DANGERS, but SMP safety will not be guaranteed.
#   endif
#endif
