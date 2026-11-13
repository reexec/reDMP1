#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "recomp_abi.h"

#include <psprecomp/cpu.h>
#include <psprecomp/dispatch.h>
#include <psprecomp/hle.h>
#include <psprecomp/mem.h>
#include <psprecomp/recomp_rt.h>
#include <psprecomp/vfpu.h>
#include <repsp/platform/guarded_run.h>
#include <repsp/recomp/guest_cpu.h>
#include <repsp/runtime/guest_memory.h>
#include <repsp/runtime/nid_registry.h>

#define CHECK(cond)                                                                                \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            std::fprintf(stderr, "Check failed at %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (0)

static int g_dummy_calls = 0;
static void DummyTarget(void)
{
    g_dummy_calls++;
}

int main(void)
{
    std::printf("[recomp_abi_probe] Starting verification probe...\n");

    // 1. Initialise repsp GuestMemory (512 MiB window)
    repsp::runtime::GuestMemory& memory = repsp::runtime::GetGuestMemory();
    bool memOk = memory.Initialize();
    CHECK(memOk);

    // 2. Initialise ABI adapter
    recomp_abi_init();
    const repsp_guest_cpu_ops* ops = repsp_recomp_get_cpu_ops();
    CHECK(ops != nullptr);
    CHECK(ops->context_size == sizeof(psp_cpu_state));
    CHECK(ops->save != nullptr);
    CHECK(ops->restore != nullptr);
    CHECK(ops->get_gpr != nullptr);
    CHECK(ops->set_gpr != nullptr);
    CHECK(ops->call_guest != nullptr);

    // 3. Memory read/write operations
    psp_write32(0x08800000, 0x12345678);
    CHECK(psp_read32(0x08800000) == 0x12345678);
    CHECK(psp_read8(0x08800000) == 0x78);
    CHECK(psp_read16(0x08800000) == 0x5678);

    psp_write_f32(0x08800004, 3.141592f);
    float fval = psp_read_f32(0x08800004);
    CHECK(std::fabs(fval - 3.141592f) < 0.00001f);

    // 4. GPR operations & context save/restore
    ops->set_gpr(PSP_REG_A0, 0xDEADBEEF);
    CHECK(ops->get_gpr(PSP_REG_A0) == 0xDEADBEEF);

    ops->set_gpr(PSP_REG_ZERO, 0x12345678);
    CHECK(ops->get_gpr(PSP_REG_ZERO) == 0); // $zero must stay 0

    std::vector<uint8_t> savedContext(ops->context_size);
    ops->save(savedContext.data());

    ops->set_gpr(PSP_REG_A0, 0xCAFEBABE);
    CHECK(ops->get_gpr(PSP_REG_A0) == 0xCAFEBABE);

    ops->restore(savedContext.data());
    CHECK(ops->get_gpr(PSP_REG_A0) == 0xDEADBEEF);

    // 5. Function registration & dispatch
    psp_register(0x08804000, DummyTarget);
    CHECK(psp_lookup(0x08804000) == DummyTarget);

    ops->call_guest(0x08804000);
    CHECK(g_dummy_calls == 1);

    // 6. Dispatch miss & psp_unimplemented stop handling via repsp_raise_stop
    uint64_t missesBefore = recomp_abi_dispatch_misses();
    auto missRes = repsp::platform::GuardedRun([]() {
        psp_dispatch(0x08809990); // unmapped target
    });
    CHECK(!missRes.succeeded);
    CHECK(missRes.stopped);
    CHECK(missRes.stop_reason.find("08809990") != std::string::npos);
    CHECK(recomp_abi_dispatch_misses() == missesBefore + 1);
    CHECK(recomp_abi_last_miss() == 0x08809990);

    uint64_t unimplBefore = psp_unimplemented_total();
    auto unimplRes = repsp::platform::GuardedRun([]() {
        psp_unimplemented(0x08804444, "test_insn");
    });
    CHECK(!unimplRes.succeeded);
    CHECK(unimplRes.stopped);
    CHECK(unimplRes.stop_reason.find("08804444") != std::string::npos);
    CHECK(unimplRes.stop_reason.find("test_insn") != std::string::npos);
    CHECK(psp_unimplemented_total() == unimplBefore + 1);

    // 7. HLE calls into repsp NID registry
    bool nidInvoked = false;
    repsp::runtime::GetNidRegistry().Register("TestModule", 0x12345678, "TestFunction", [&nidInvoked]() {
        nidInvoked = true;
    });
    psp_hle_call(0x12345678);
    CHECK(nidInvoked);

    // Unregistered NID returns not-implemented in $v0 (reg 2)
    psp_hle_call(0x99999999);
    CHECK(ops->get_gpr(PSP_REG_V0) == repsp::runtime::kSceKernelErrorNotImplemented);

    // 8. VFPU operations (vdiv, lv_q, sv_s)
    psp_vfpu_reset();

    // Store four floats for quad load: [10.0, 20.0, 30.0, 40.0]
    psp_write_f32(0x08802000, 10.0f);
    psp_write_f32(0x08802004, 20.0f);
    psp_write_f32(0x08802008, 30.0f);
    psp_write_f32(0x0880200C, 40.0f);

    // Store divisors: [2.0, 4.0, 5.0, 8.0]
    psp_write_f32(0x08802010, 2.0f);
    psp_write_f32(0x08802014, 4.0f);
    psp_write_f32(0x08802018, 5.0f);
    psp_write_f32(0x0880201C, 8.0f);

    // Load into VFPU: col 0 = 0x00, col 1 = 0x01
    psp_lv_q(0x00, 0x08802000);
    psp_lv_q(0x01, 0x08802010);

    // Vector divide: vd = col 2 (0x02) = col 0 (0x00) / col 1 (0x01)
    psp_vdiv(0x02, 0x00, 0x01, 4);

    // Check results: 10/2=5, 20/4=5, 30/5=6, 40/8=5
    // Save lane 0 of col 2 to memory with sv_s
    psp_sv_s(0x02, 0x08802020);
    CHECK(std::fabs(psp_read_f32(0x08802020) - 5.0f) < 0.0001f);

    // Also check single load and store: sv_s with lane 2
    // single register for row 2, col 2 in matrix 0:
    // (vreg >> 5) & 3 = 2 -> vreg = 0x02 | (2 << 5) = 0x42
    psp_sv_s(0x42, 0x08802028);
    CHECK(std::fabs(psp_read_f32(0x08802028) - 6.0f) < 0.0001f);

    std::printf("[recomp_abi_probe] All checks passed successfully!\n");
    return 0;
}
