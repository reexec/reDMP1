# psprecomp ABI Adapter (src/recomp_abi/)

이 디렉터리는 `allegrexrecomp`가 기계적으로 생성한 C 코드가 기대하는 `psprecomp` 런타임 ABI를 `repsp`의 lifter 중립 재컴파일 코드 계약(`include/repsp/recomp/guest_cpu.h`)에 연결하는 어댑터 타깃(`redmp1_recomp_abi`)입니다.

## 역할과 파일 구성

`repsp`는 특정 lifter(psprecomp)에 의존하지 않으며, psprecomp의 레지스터 구조나 함수 명칭을 알지 못합니다. 이 계층이 psprecomp의 전역 심볼 계약을 완전히 구현하여 repsp 런타임과 연결합니다.

| 소스 파일 | 담당 ABI 및 심볼 | repsp 연결 |
|---|---|---|
| `cpu.c` | `psp_cpu_t psp_cpu`<br/>`psp_cpu_init`, `psp_cpu_reset`<br/>`repsp_recomp_register_cpu` | 레지스터 파일(`r0..r31`, `pc`, `hi`, `lo`, `f0..f31`, `v000..v777`)을 정의하고, repsp의 `repsp_guest_cpu_ops`(`save`, `restore`, `get_gpr`, `set_gpr`, `call_guest`)를 구현하여 런타임에 등록. |
| `mem.c` | `psp_read8/16/32/64`<br/>`psp_write8/16/32/64`<br/>`psp_mem_ptr` | C-호출 가능한 `repsp_guest_read*`, `repsp_guest_write*`, `repsp_guest_translate`를 직접 호출. based 모델의 가상 메모리 변환과 미러링을 온전히 상속. |
| `dispatch.c` | `psp_dispatch_register`<br/>`psp_dispatch_call`<br/>`psp_dispatch_calls` | 0x08804000 기준 게스트 주소 -> 재컴파일된 C 함수 포인터 디스패치 테이블 관리 (동적 간접 점프 처리). |
| `trap.c` | `psp_hle_call`<br/>`psp_syscall`<br/>`psp_trap`<br/>`recomp_abi_set_stop_on_hle` | NID 호출을 `repsp_recomp_hle_call(nid)`로 전달. 테스트 및 검증을 위한 최초 HLE 호출 가로채기(`stop_on_hle` / `longjmp`) 지원. |
| `vfpu.c` | VFPU 행렬 전치 도우미 매크로 | psprecomp가 방출하는 VFPU 전치 연산자 지원. |

---

# psprecomp ABI Adapter (English)

This directory hosts the adapter library (`redmp1_recomp_abi`) connecting the `psprecomp` ABI that mechanically lifted C code expects to repsp's lifter-neutral recompiled-code contract (`include/repsp/recomp/guest_cpu.h`).

## Responsibilities and File Structure

`repsp` has zero dependencies on `psprecomp` and knows nothing about its register layout or symbol conventions. This adapter implements all psprecomp ABI symbols and forwards operations into repsp.

| Source File | ABI Symbols Implemented | repsp Binding |
|---|---|---|
| `cpu.c` | `psp_cpu_t psp_cpu`<br/>`psp_cpu_init`, `psp_cpu_reset` | Defines the guest CPU context and registers `repsp_guest_cpu_ops` callbacks (`save`, `restore`, `get_gpr`, `set_gpr`, `call_guest`) with repsp. |
| `mem.c` | `psp_read8/16/32/64`<br/>`psp_write8/16/32/64`<br/>`psp_mem_ptr` | Dispatches memory accesses directly to `repsp_guest_read*`, `repsp_guest_write*`, and `repsp_guest_translate`. |
| `dispatch.c` | `psp_dispatch_register`<br/>`psp_dispatch_call`<br/>`psp_dispatch_calls` | Manages address-to-function dispatch table for indirect jumps. |
| `trap.c` | `psp_hle_call`<br/>`psp_syscall`<br/>`psp_trap`<br/>`recomp_abi_set_stop_on_hle` | Forwards import calls to `repsp_recomp_hle_call`. Supports `stop_on_hle` intercept for test harnesses. |
| `vfpu.c` | VFPU matrix transpose helpers | Implements VFPU matrix manipulation operations expected by generated code. |
