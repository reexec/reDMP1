# psprecomp ABI Adapter (src/recomp_abi/)

이 디렉터리는 `allegrexrecomp`가 기계적으로 생성한 C 코드가 기대하는 `psprecomp` 런타임 ABI를 `repsp`의 lifter 중립 재컴파일 코드 계약(`include/repsp/recomp/guest_cpu.h`)에 연결하는 어댑터 타깃(`redmp1_recomp_abi`)이 위치할 자리입니다.

## 역할과 설계 원칙

1. **repsp의 lifter 중립성 보존**:
   * `repsp`는 특정 lifter(psprecomp)에 의존하지 않으며, psprecomp의 레지스터 구조나 함수 명칭을 알지 못합니다.
   * `psprecomp` 헤더(`cpu.h`, `mem.h`, `dispatch.h`, `vfpu.h`, `hle.h`)가 선언한 전역 심볼(`psp_cpu`, `psp_read32`, `psp_write32`, `psp_mem_ptr`, `psp_dispatch`, `psp_hle_call`, VFPU 도우미 등)을 이 어댑터 계층에서 구현합니다.
2. **런타임 연결**:
   * 메모리 접근은 `repsp::runtime::GuestMemory::Translate`로 연결합니다.
   * HLE 호출은 `repsp::runtime::NidRegistry::Invoke`로 전달합니다.
   * 시작 시 게스트 CPU 컨텍스트 조작 콜백 표(`repsp_guest_cpu_ops`)를 `repsp`에 등록합니다.
3. **구현 계획**:
   * issue #1 (프로젝트 부트스트랩)에서는 설계와 자리만 마련합니다.
   * issue #2 (lift & link)에서 실제 어댑터 구현 및 링크가 진행됩니다.

---

# psprecomp ABI Adapter (English)

This directory hosts the adapter (`redmp1_recomp_abi`) connecting the `psprecomp` ABI that mechanically generated code expects to repsp's lifter-neutral recompiled-code contract (`include/repsp/recomp/guest_cpu.h`).

## Purpose and Design Principles

1. **Preserving repsp's Lifter Neutrality**:
   * `repsp` does not depend on `psprecomp` and knows neither psprecomp's register layout nor its symbol names.
   * Symbols declared by psprecomp headers (`psp_cpu`, `psp_read32`, `psp_write32`, `psp_mem_ptr`, `psp_dispatch`, `psp_hle_call`, VFPU helpers, etc.) are implemented here.
2. **Runtime Binding**:
   * Memory accesses map to `repsp::runtime::GuestMemory::Translate`.
   * HLE calls dispatch via `repsp::runtime::NidRegistry::Invoke`.
   * At startup, a CPU context operations table (`repsp_guest_cpu_ops`) is registered with repsp.
3. **Implementation Plan**:
   * Issue #1 (project bootstrap) defines the architecture and placeholder.
   * Issue #2 (lift & link) will implement and link the adapter.
