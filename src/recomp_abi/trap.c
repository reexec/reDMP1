/* psprecomp ABI trap and HLE bridge to repsp runtime. */

#include "recomp_abi.h"

#include <psprecomp/hle.h>
#include <psprecomp/recomp_rt.h>
#include <repsp/recomp/guest_cpu.h>
#include <setjmp.h>
#include <stdio.h>

static uint64_t g_syscalls = 0;
static uint64_t g_unimplemented = 0;
static int g_stop_on_hle = 0;
static uint32_t g_first_hle_nid = 0;
static jmp_buf g_hle_jmp;
static int g_hle_jmp_set = 0;

void recomp_abi_set_stop_on_hle(int stop) {
    g_stop_on_hle = stop;
}

uint32_t recomp_abi_get_first_hle_nid(void) {
    return g_first_hle_nid;
}

void* recomp_abi_get_hle_jmp_buf(void) {
    g_hle_jmp_set = 1;
    return &g_hle_jmp;
}

void psp_hle_call(uint32_t nid) {
    if (g_first_hle_nid == 0) {
        g_first_hle_nid = nid;
    }
    repsp_recomp_hle_call(nid);
    if (g_stop_on_hle && g_hle_jmp_set) {
        longjmp(g_hle_jmp, 1);
    }
}

void psp_syscall(uint32_t id) {
    g_syscalls++;
    repsp_recomp_syscall(id);
}

void psp_unimplemented(uint32_t addr, const char *what) {
    g_unimplemented++;
    fprintf(stderr, "[recomp_abi] UNIMPLEMENTED at 0x%08X: %s\n", addr, what ? what : "unknown");
}

uint64_t psp_syscall_total(void) { return g_syscalls; }
uint64_t psp_unimplemented_total(void) { return g_unimplemented; }
void psp_trap_reset(void) {
    g_syscalls = 0;
    g_unimplemented = 0;
}
