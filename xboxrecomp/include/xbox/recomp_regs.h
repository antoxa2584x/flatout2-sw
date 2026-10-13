/* The guest register file: one thread-local struct, shared by the runtime
 * and the lifted code.
 *
 * Each guest thread has its own registers (they were separate thread-local
 * variables, g_eax ... g_mm7). On the Switch every thread-local access is a
 * call (devkitA64 reads the thread pointer through __aarch64_read_tp,
 * tpidrro_el0 + a load), and the lifted code re-read it after every guest
 * call it made: 4% of FlatOut 2's game thread in that function alone, plus
 * two adds per register for the large TLS offsets.
 *
 * Now they are fields of _rr[0]. Lifted functions open with RECOMP_RR_DECL,
 * a local pointer named _rr that shadows the array, so the thread pointer is
 * read once per function and every register is a plain load or store off a
 * register-held base. The pointer is this thread's own, so it stays right
 * when the guest lock changes hands mid-function. Code without the local
 * (the runtime, recomp_manual.c, header helpers) uses _rr, the array, as
 * before. The g_* names are macros over the fields. */
#ifndef XBOX_RECOMP_REGS_H
#define XBOX_RECOMP_REGS_H

#include <stdint.h>

#ifndef RECOMP_TLS
#  if defined(_MSC_VER)
#    define RECOMP_TLS __declspec(thread)
#  elif defined(__GNUC__) || defined(__clang__)
#    define RECOMP_TLS __thread
#  else
#    define RECOMP_TLS _Thread_local
#  endif
#endif

#ifndef RECOMP_XMM_DEFINED
#define RECOMP_XMM_DEFINED
typedef union RecompXmm {
    float    f[4];
    double   d[2];
    uint32_t u[4];
    int32_t  i[4];
    uint64_t q[2];
} RecompXmm;
#endif

#ifndef RECOMP_MMX_DEFINED
#define RECOMP_MMX_DEFINED
typedef union RecompMmx {
    int8_t   b[8];
    uint8_t  ub[8];
    int16_t  w[4];
    uint16_t uw[4];
    int32_t  d[2];
    uint32_t ud[2];
    uint64_t q;
} RecompMmx;
#endif

typedef struct RecompRegs {
    /* Hot first: small offsets from the base. */
    /* Field names carry r_: the lifted code defines eax, xmm0, ... as
     * macros for the g_ names, which would rewrite plain field names. */
    uint32_t r_eax, r_ecx, r_edx, r_ebx, r_esi, r_edi, r_esp, r_ebp;
    uint32_t r_seh_ebp, r_fs_base;
    int      r_fp_top, r_fp_cmp, r_df;
    uint16_t r_fp_control_word, r_fp_cc;
    double   r_fp_stack[8];
    RecompXmm r_xmm[8];
    RecompMmx r_mm[8];
} RecompRegs;

extern RECOMP_TLS RecompRegs _rr[1];

/* Opaque to the compiler: a plain `return _rr` is a TLS address, which GCC
 * re-derives (another __aarch64_read_tp call) after every call instead of
 * keeping it in a register. Through the empty asm it is just a value. */
static inline RecompRegs *recomp_rr_self(void)
{
    RecompRegs *p = _rr;
#if defined(__GNUC__) || defined(__clang__)
    __asm__("" : "+r"(p));
#endif
    return p;
}
#define RECOMP_RR_DECL RecompRegs *const _rr = recomp_rr_self(); (void)_rr;

#define g_eax  (_rr->r_eax)
#define g_ecx  (_rr->r_ecx)
#define g_edx  (_rr->r_edx)
#define g_ebx  (_rr->r_ebx)
#define g_esi  (_rr->r_esi)
#define g_edi  (_rr->r_edi)
#define g_esp  (_rr->r_esp)
#define g_ebp  (_rr->r_ebp)
#define g_seh_ebp (_rr->r_seh_ebp)
#define g_fs_base (_rr->r_fs_base)
#define g_fp_top (_rr->r_fp_top)
#define g_fp_cmp (_rr->r_fp_cmp)
#define g_df   (_rr->r_df)
#define g_fp_control_word (_rr->r_fp_control_word)
#define g_fp_cc (_rr->r_fp_cc)
#define g_fp_stack (_rr->r_fp_stack)
#define g_xmm0 (_rr->r_xmm[0])
#define g_xmm1 (_rr->r_xmm[1])
#define g_xmm2 (_rr->r_xmm[2])
#define g_xmm3 (_rr->r_xmm[3])
#define g_xmm4 (_rr->r_xmm[4])
#define g_xmm5 (_rr->r_xmm[5])
#define g_xmm6 (_rr->r_xmm[6])
#define g_xmm7 (_rr->r_xmm[7])
#define g_mm0  (_rr->r_mm[0])
#define g_mm1  (_rr->r_mm[1])
#define g_mm2  (_rr->r_mm[2])
#define g_mm3  (_rr->r_mm[3])
#define g_mm4  (_rr->r_mm[4])
#define g_mm5  (_rr->r_mm[5])
#define g_mm6  (_rr->r_mm[6])
#define g_mm7  (_rr->r_mm[7])

#endif
