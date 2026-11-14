/* Derived from psprecomp commit 5ca18fa (include/psprecomp/cpu.h) under MIT license. */

#include "recomp_abi.h"

#include <psprecomp/cpu.h>
#include <psprecomp/dispatch.h>
#include <repsp/recomp/guest_cpu.h>
#include <string.h>

psp_cpu_state psp_cpu;

const char *const psp_reg_names[PSP_NUM_GPR] = {
    "$zero", "$at",
    "$v0", "$v1",
    "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3",
    "$t4", "$t5", "$t6", "$t7",
    "$s0", "$s1", "$s2", "$s3",
    "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9",
    "$k0", "$k1",
    "$gp", "$sp", "$fp", "$ra"
};

void psp_cpu_reset(void) {
    memset(&psp_cpu, 0, sizeof psp_cpu);
}

static void cpu_save(void* context) {
    memcpy(context, &psp_cpu, sizeof(psp_cpu_state));
}

static void cpu_restore(const void* context) {
    memcpy(&psp_cpu, context, sizeof(psp_cpu_state));
}

static uint32_t cpu_get_gpr(int index) {
    if (index >= 0 && index < PSP_NUM_GPR) {
        return psp_cpu.r[index];
    }
    return 0;
}

static void cpu_set_gpr(int index, uint32_t value) {
    psp_set_reg((uint32_t)index, value);
}

static void cpu_call_guest(uint32_t address) {
    psp_call_from_host(address);
}

static uint32_t cpu_get_fpr(int index) {
    if (index >= 0 && index < 32) {
        uint32_t bits = 0;
        memcpy(&bits, &psp_cpu.f[index], sizeof(bits));
        return bits;
    }
    return 0;
}

static void cpu_set_fpr(int index, uint32_t value) {
    if (index >= 0 && index < 32) {
        memcpy(&psp_cpu.f[index], &value, sizeof(value));
    }
}

static repsp_guest_cpu_ops s_cpu_ops = {
    .context_size = sizeof(psp_cpu_state),
    .save = cpu_save,
    .restore = cpu_restore,
    .get_gpr = cpu_get_gpr,
    .set_gpr = cpu_set_gpr,
    .call_guest = cpu_call_guest,
    .get_fpr = cpu_get_fpr,
    .set_fpr = cpu_set_fpr,
};

void recomp_abi_init(void) {
    repsp_recomp_register_cpu(&s_cpu_ops);
}
