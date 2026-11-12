/* psprecomp ABI trap and HLE bridge to repsp runtime. */

#include <psprecomp/hle.h>
#include <psprecomp/recomp_rt.h>
#include <repsp/recomp/guest_cpu.h>
#include <stdio.h>

static uint64_t g_syscalls = 0;
static uint64_t g_unimplemented = 0;

void psp_hle_call(uint32_t nid) {
    repsp_recomp_hle_call(nid);
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
