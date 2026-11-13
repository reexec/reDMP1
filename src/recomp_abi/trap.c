/* psprecomp ABI trap and HLE bridge to repsp runtime. */

#include "recomp_abi.h"

#include <psprecomp/hle.h>
#include <psprecomp/recomp_rt.h>
#include <repsp/platform/guarded_run.h>
#include <repsp/recomp/guest_cpu.h>
#include <stdio.h>

static uint64_t g_syscalls = 0;
static uint64_t g_unimplemented = 0;
static char g_unimpl_reason[256];

void psp_hle_call(uint32_t nid) {
    repsp_recomp_hle_call(nid);
}

void psp_syscall(uint32_t id) {
    g_syscalls++;
    repsp_recomp_syscall(id);
}

void psp_unimplemented(uint32_t addr, const char *what) {
    g_unimplemented++;
    snprintf(g_unimpl_reason, sizeof(g_unimpl_reason),
             "Unimplemented at 0x%08X: %s", addr, what ? what : "unknown");
    fprintf(stderr, "[recomp_abi] %s\n", g_unimpl_reason);
    repsp_raise_stop(g_unimpl_reason);
}

uint64_t psp_syscall_total(void) { return g_syscalls; }
uint64_t psp_unimplemented_total(void) { return g_unimplemented; }
void psp_trap_reset(void) {
    g_syscalls = 0;
    g_unimplemented = 0;
}
