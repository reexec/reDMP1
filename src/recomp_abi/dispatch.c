/* Derived from psprecomp commit 5ca18fa (src/dispatch.c) under MIT license. */

#include "recomp_abi.h"

#include <psprecomp/dispatch.h>
#include <psprecomp/cpu.h>
#include <repsp/platform/guarded_run.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t addr;
    psp_fn_t fn;
    int      used;
} slot;

static slot     *g_table = NULL;
static uint32_t  g_cap = 0;
static uint32_t  g_count = 0;
static uint64_t  g_misses = 0;
static uint32_t  g_last_miss = 0;
static uint64_t  g_budget = 0;
static uint64_t  g_calls = 0;
static void    (*g_over)(void) = NULL;

static uint32_t hash_addr(uint32_t a) {
    return (a >> 2) * 2654435761u;
}

static void grow(void) {
    uint32_t ncap = g_cap ? g_cap * 2 : 4096;
    slot *nt = (slot *)calloc(ncap, sizeof *nt);
    if (!nt) return;

    for (uint32_t i = 0; i < g_cap; i++) {
        if (!g_table[i].used) continue;
        uint32_t m = ncap - 1;
        uint32_t j = hash_addr(g_table[i].addr) & m;
        while (nt[j].used) j = (j + 1) & m;
        nt[j] = g_table[i];
    }
    free(g_table);
    g_table = nt;
    g_cap = ncap;
}

void psp_register(uint32_t addr, psp_fn_t fn) {
    if (!g_table || (g_count + 1) * 4 >= g_cap * 3) grow();
    if (!g_table) return;

    uint32_t m = g_cap - 1;
    uint32_t j = hash_addr(addr) & m;
    while (g_table[j].used) {
        if (g_table[j].addr == addr) { g_table[j].fn = fn; return; }
        j = (j + 1) & m;
    }
    g_table[j].addr = addr;
    g_table[j].fn = fn;
    g_table[j].used = 1;
    g_count++;
}

void psp_register_label(uint32_t addr, psp_fn_t fn) {
    if (!g_table || (g_count + 1) * 4 >= g_cap * 3) grow();
    if (!g_table) return;

    uint32_t m = g_cap - 1;
    uint32_t j = hash_addr(addr) & m;
    while (g_table[j].used) {
        if (g_table[j].addr == addr) return;
        j = (j + 1) & m;
    }
    g_table[j].addr = addr;
    g_table[j].fn = fn;
    g_table[j].used = 1;
    g_count++;
}

psp_fn_t psp_lookup(uint32_t addr) {
    if (!g_table) return NULL;
    uint32_t m = g_cap - 1;
    uint32_t j = hash_addr(addr) & m;
    while (g_table[j].used) {
        if (g_table[j].addr == addr) return g_table[j].fn;
        j = (j + 1) & m;
    }
    return NULL;
}

static char g_miss_reason[128];

static void default_miss(uint32_t addr) {
    g_misses++;
    g_last_miss = addr;
    snprintf(g_miss_reason, sizeof(g_miss_reason),
             "Dispatch miss: indirect call to 0x%08X (not registered)", addr);
    fprintf(stderr, "[recomp_abi] %s\n", g_miss_reason);
    repsp_raise_stop(g_miss_reason);
}

void psp_dispatch(uint32_t addr) {
    if (g_budget && ++g_calls >= g_budget) {
        g_budget = 0;
        if (g_over) g_over();
    }
    psp_fn_t fn = psp_lookup(addr);
    if (fn) {
        fn();
        return;
    }
    default_miss(addr);
}

void psp_call_from_host(uint32_t addr) {
    psp_frame f = { 0, psp_cpu.frame };
    psp_cpu.frame = &f;
    psp_dispatch(addr);
    psp_cpu.frame = f.prev;
    psp_cpu.unwind = 0;
}

int psp_ra_is_jump(uint32_t ra) {
    const psp_frame *f = psp_cpu.frame;
    if (!f || !f->ret || ra == f->ret) return 0;
    for (const psp_frame *g = f->prev; g && g->ret; g = g->prev)
        if (g->ret == ra) { psp_cpu.unwind = ra; return 0; }
    return 1;
}

void psp_dispatch_set_budget(uint64_t calls, void (*on_exceeded)(void)) {
    g_budget = calls;
    g_over = on_exceeded;
    g_calls = 0;
}

uint64_t psp_dispatch_calls(void) { return g_calls; }

uint64_t recomp_abi_dispatch_misses(void) { return g_misses; }
uint32_t recomp_abi_last_miss(void) { return g_last_miss; }

void recomp_abi_reset(void) {
    free(g_table);
    g_table = NULL;
    g_cap = 0;
    g_count = 0;
    g_misses = 0;
    g_last_miss = 0;
    g_budget = 0;
    g_calls = 0;
    g_over = NULL;
    psp_cpu_reset();
}
