/* Optional guest entry tracing, SP balance checks, and host stack high-water measurement. */

#include "recomp_abi.h"

#include <psprecomp/cpu.h>
#include <psprecomp/dispatch.h>
#include <psprecomp/mem.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#include <intrin.h>
#define REDMP1_TLS __declspec(thread)
static volatile long g_trace_lock = 0;
static void trace_lock(void) {
    while (_InterlockedCompareExchange(&g_trace_lock, 1, 0) != 0) {}
}
static void trace_unlock(void) {
    _InterlockedExchange(&g_trace_lock, 0);
}
#else
#define REDMP1_TLS _Thread_local
static volatile int g_trace_lock = 0;
static void trace_lock(void) {
    while (__atomic_exchange_n(&g_trace_lock, 1, __ATOMIC_ACQUIRE) != 0) {}
}
static void trace_unlock(void) {
    __atomic_exchange_n(&g_trace_lock, 0, __ATOMIC_RELEASE);
}
#endif

static int g_trace_init = 0;
static int g_trace_refcount = 0;

static void ensure_trace_init(void) {
    if (!g_trace_init) {
        const char *env = getenv("REDMP1_TRACE_REFCOUNT");
        g_trace_refcount = (env && env[0] == '1') ? 1 : 0;
        g_trace_init = 1;
    }
}

#define MAX_SP_SITES 1024
#define MAX_HOST_THREADS 64

typedef struct {
    psp_sp_site sites[MAX_SP_SITES];
    int count;
    uint64_t total_violations;
    uint32_t first_bad;
    int32_t first_delta;
} sp_table_t;

static sp_table_t g_sp_check = { .count = 0, .total_violations = 0, .first_bad = 0, .first_delta = 0 };
static sp_table_t g_sp_leave = { .count = 0, .total_violations = 0, .first_bad = 0, .first_delta = 0 };
static sp_table_t g_sp_call  = { .count = 0, .total_violations = 0, .first_bad = 0, .first_delta = 0 };

#ifdef REDMP1_TRACE_GUEST
typedef struct {
    int used;
    uint32_t entry_addr;
    size_t max_stack_bytes;
    uint32_t deep_chain[4];
} host_thread_stack_t;

static host_thread_stack_t g_thread_stacks[MAX_HOST_THREADS];
static int g_thread_stack_count = 0;

static REDMP1_TLS int t_thread_idx = -1;
static REDMP1_TLS uintptr_t t_stack_base = 0;
static REDMP1_TLS size_t t_max_bytes = 0;
#endif /* REDMP1_TRACE_GUEST */
static REDMP1_TLS uint32_t t_ring[4] = {0, 0, 0, 0};
static REDMP1_TLS uint32_t t_ring_pos = 0;

#ifdef REDMP1_TRACE_GUEST
static void record_host_stack(uint32_t addr) {
    volatile uint8_t probe = 0;
    const uintptr_t cur = (uintptr_t)&probe;

    if (t_thread_idx < 0) {
        trace_lock();
        if (g_thread_stack_count < MAX_HOST_THREADS) {
            t_thread_idx = g_thread_stack_count++;
            g_thread_stacks[t_thread_idx].used = 1;
            g_thread_stacks[t_thread_idx].entry_addr = addr;
            g_thread_stacks[t_thread_idx].max_stack_bytes = 0;
        } else {
            t_thread_idx = MAX_HOST_THREADS - 1;
        }
        trace_unlock();
        t_stack_base = cur;
        t_max_bytes = 0;
    }

    t_ring[t_ring_pos & 3u] = addr;
    t_ring_pos++;

    const size_t depth = (t_stack_base >= cur) ? (t_stack_base - cur) : (cur - t_stack_base);
    if (depth > t_max_bytes) {
        t_max_bytes = depth;
        if (t_thread_idx >= 0 && t_thread_idx < MAX_HOST_THREADS) {
            trace_lock();
            if (depth > g_thread_stacks[t_thread_idx].max_stack_bytes) {
                g_thread_stacks[t_thread_idx].max_stack_bytes = depth;
                for (int k = 0; k < 4; ++k) {
                    g_thread_stacks[t_thread_idx].deep_chain[k] = t_ring[(t_ring_pos + (uint32_t)k) & 3u];
                }
            }
            trace_unlock();
        }
    }
}
#endif /* REDMP1_TRACE_GUEST */

static void record_sp_violation(sp_table_t *tbl, const char *kind, uint32_t site, uint32_t sp0, uint32_t sp1) {
    const int32_t delta = (int32_t)(sp1 - sp0);
    int is_new = 0;

    trace_lock();
    tbl->total_violations++;
    if (tbl->first_bad == 0) {
        tbl->first_bad = site;
        tbl->first_delta = delta;
    }
    int found = -1;
    for (int i = 0; i < tbl->count; ++i) {
        if (tbl->sites[i].site == site && tbl->sites[i].delta == delta) {
            found = i;
            break;
        }
    }
    if (found >= 0) {
        tbl->sites[found].count++;
        tbl->sites[found].in = sp0;
        tbl->sites[found].out = sp1;
        tbl->sites[found].last = t_ring[(t_ring_pos - 1u) & 3u];
    } else if (tbl->count < MAX_SP_SITES) {
        psp_sp_site *s = &tbl->sites[tbl->count++];
        s->site = site;
        s->delta = delta;
        s->count = 1;
        s->in = sp0;
        s->out = sp1;
        s->last = t_ring[(t_ring_pos - 1u) & 3u];
        is_new = 1;
    }
    trace_unlock();

    if (is_new) {
        fprintf(stderr, "[%s] site=0x%08X sp_in=0x%08X sp_out=0x%08X delta=%+d\n",
                kind, site, sp0, sp1, delta);
    }
}

static int copy_sp_hist(const sp_table_t *tbl, psp_sp_site *out, int max) {
    if (!out || max <= 0) {
        return 0;
    }
    trace_lock();
    const int n = (tbl->count < max) ? tbl->count : max;
    for (int i = 0; i < n; ++i) {
        out[i] = tbl->sites[i];
    }
    trace_unlock();
    return n;
}

void redmp1_note_guest_entry(uint32_t addr);

void psp_trace_enter(uint32_t addr) {
#ifdef REDMP1_TRACE_GUEST
    redmp1_note_guest_entry(addr);
    record_host_stack(addr);
#endif
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
void psp_trace_mark(uint32_t addr) { (void)addr; }

void psp_trace_sp(uint32_t addr, uint32_t sp0, uint32_t sp1) {
    if (sp1 != sp0) {
        record_sp_violation(&g_sp_check, "SP_CHECK", addr, sp0, sp1);
    }
}

void psp_trace_sp_leave(uint32_t addr, uint32_t sp0, uint32_t sp1) {
    if ((int32_t)(sp1 - sp0) > 0) {
        record_sp_violation(&g_sp_leave, "SP_LEAVE", addr, sp0, sp1);
    }
}

void psp_trace_sp_call(uint32_t target, uint32_t sp0, uint32_t sp1) {
    if (sp1 != sp0) {
        record_sp_violation(&g_sp_call, "SP_CALL", target, sp0, sp1);
    }
}

uint64_t psp_sp_violations(void) {
    return g_sp_check.total_violations + g_sp_leave.total_violations;
}

uint64_t psp_sp_call_violations(void) {
    return g_sp_call.total_violations;
}

uint32_t psp_sp_first_bad(void) {
    return g_sp_check.first_bad ? g_sp_check.first_bad : g_sp_leave.first_bad;
}

int32_t psp_sp_first_delta(void) {
    return g_sp_check.first_bad ? g_sp_check.first_delta : g_sp_leave.first_delta;
}

int psp_sp_hist(psp_sp_site *out, int max) {
    return copy_sp_hist(&g_sp_check, out, max);
}

int psp_sp_call_hist(psp_sp_site *out, int max) {
    return copy_sp_hist(&g_sp_call, out, max);
}

void recomp_abi_trace_summary(void) {
#ifdef REDMP1_TRACE_GUEST
    trace_lock();
    fprintf(stderr,
            "\n=== Guest SP Balance & Host Stack Summary ===\n"
            "  SP_CHECK: total=%llu unique=%d\n"
            "  SP_LEAVE: total=%llu unique=%d\n"
            "  SP_CALL : total=%llu unique=%d\n",
            (unsigned long long)g_sp_check.total_violations, g_sp_check.count,
            (unsigned long long)g_sp_leave.total_violations, g_sp_leave.count,
            (unsigned long long)g_sp_call.total_violations, g_sp_call.count);
    for (int i = 0; i < g_sp_check.count && i < 32; ++i) {
        const psp_sp_site *s = &g_sp_check.sites[i];
        fprintf(stderr, "    [SP_CHECK] site=0x%08X delta=%+d count=%llu (in=0x%08X out=0x%08X)\n",
                s->site, s->delta, (unsigned long long)s->count, s->in, s->out);
    }
    for (int i = 0; i < g_sp_leave.count && i < 32; ++i) {
        const psp_sp_site *s = &g_sp_leave.sites[i];
        fprintf(stderr, "    [SP_LEAVE] site=0x%08X delta=%+d count=%llu (in=0x%08X out=0x%08X)\n",
                s->site, s->delta, (unsigned long long)s->count, s->in, s->out);
    }
    for (int i = 0; i < g_sp_call.count && i < 32; ++i) {
        const psp_sp_site *s = &g_sp_call.sites[i];
        fprintf(stderr, "    [SP_CALL ] target=0x%08X delta=%+d count=%llu (in=0x%08X out=0x%08X)\n",
                s->site, s->delta, (unsigned long long)s->count, s->in, s->out);
    }
    fprintf(stderr, "  Host Stack High-Water (%d guest threads):\n", g_thread_stack_count);
    for (int i = 0; i < g_thread_stack_count; ++i) {
        const host_thread_stack_t *t = &g_thread_stacks[i];
        fprintf(stderr,
                "    thread[%d] entry=0x%08X max_stack=%zu bytes (%.2f KiB) chain=0x%08X->0x%08X->0x%08X->0x%08X\n",
                i, t->entry_addr, t->max_stack_bytes, (double)t->max_stack_bytes / 1024.0,
                t->deep_chain[0], t->deep_chain[1], t->deep_chain[2], t->deep_chain[3]);
    }
    fprintf(stderr, "=============================================\n");
    trace_unlock();
#endif
}
