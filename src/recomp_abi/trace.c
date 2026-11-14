/* Optional guest entry tracing for diagnosing refcount/scene issues. */

#include "recomp_abi.h"

#include <psprecomp/cpu.h>
#include <psprecomp/mem.h>
#include <stdio.h>
#include <stdlib.h>

static int g_trace_init = 0;
static int g_trace_refcount = 0;

static void ensure_trace_init(void) {
    if (!g_trace_init) {
        const char *env = getenv("REDMP1_TRACE_REFCOUNT");
        g_trace_refcount = (env && env[0] == '1') ? 1 : 0;
        g_trace_init = 1;
    }
}

void psp_trace_enter(uint32_t addr) {
    ensure_trace_init();
    if (!g_trace_refcount) {
        return;
    }

    const char *tag = NULL;
    switch (addr) {
    case 0x08850814u: tag = "CInstance::Init"; break;
    case 0x0885085Cu: tag = "CInstance::Destroy"; break;
    case 0x0885089Cu: tag = "CInstance::AddRef"; break;
    case 0x088508D0u: tag = "CInstance::Release"; break;
    case 0x08843498u: tag = "Holder::AddRef"; break;
    case 0x088434DCu: tag = "Holder::Release"; break;
    case 0x08848570u: tag = "List::AddRef"; break;
    case 0x088485E8u: tag = "List::Release"; break;
    case 0x088BC988u: tag = "__assert"; break;
    default: return;
    }

    const uint32_t a0 = psp_cpu.r[4];
    const uint32_t a1 = psp_cpu.r[5];
    const uint32_t ra = psp_cpu.r[31];
    uint32_t ref = 0;
    if (addr >= 0x08850814u && addr <= 0x088508D0u && a0 >= 0x08800000u && a0 < 0x0A000000u) {
        ref = psp_read32(a0 + 4u);
    } else if ((addr == 0x08843498u || addr == 0x088434DCu) && a0 >= 0x08800000u && a0 < 0x0A000000u) {
        const uint32_t inner = psp_read32(a0 + 4u);
        ref = (inner >= 0x08800000u && inner < 0x0A000000u) ? psp_read32(inner + 4u) : 0;
        fprintf(stderr, "[REFCOUNT] %s holder=0x%08X obj=0x%08X cnt=%u ra=0x%08X\n",
                tag, a0, inner, ref, ra);
        return;
    }
    fprintf(stderr, "[REFCOUNT] %s a0=0x%08X cnt=%u a1=0x%08X ra=0x%08X\n",
            tag, a0, ref, a1, ra);
}

void psp_trace_loop(uint32_t addr) { (void)addr; }
void psp_trace_sp(uint32_t addr, uint32_t sp0, uint32_t sp1) { (void)addr; (void)sp0; (void)sp1; }
void psp_trace_sp_leave(uint32_t addr, uint32_t sp0, uint32_t sp1) { (void)addr; (void)sp0; (void)sp1; }
void psp_trace_sp_call(uint32_t target, uint32_t sp0, uint32_t sp1) { (void)target; (void)sp0; (void)sp1; }
void psp_trace_mark(uint32_t addr) { (void)addr; }
