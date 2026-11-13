# Third-Party Notices

reDMP1 incorporates or links the following third-party open-source projects:

## 1. psprecomp

* Location: `third_party/psprecomp` (submodule) and `src/recomp_abi/` (carried code)
* Upstream: https://github.com/sp00nznet/psprecomp
* Pin: `5ca18fa`
* License: MIT License (Copyright (c) 2024 sp00nznet)
* Usage: Static recompilation toolkit (allegrexrecomp host CLI tool and ABI headers); portions of `src/vfpu.c` and `src/dispatch.c` carried into `src/recomp_abi/`.

## 2. repsp

* Location: `third_party/repsp`
* Upstream: https://github.com/reexec/repsp
* Pin: `730c404` (v0.0.1)
* License: MIT License (Copyright (c) 2026 reexec)
* Usage: Shared portable PSP runtime, guest memory translation, NID registry, HLE kernel, platform backends.

### 2.1. Simple DirectMedia Layer (SDL3) (repsp sub-dependency)

* Location: `third_party/repsp/third_party/SDL`
* Upstream: https://github.com/libsdl-org/SDL
* Pin: `release-3.4.x` (`ee239da42bd043039fddbad0622645f54bf5a640`)
* License: zlib License (Copyright (C) 1997-2026 Sam Lantinga)
* Usage: Platform windowing, input events, and audio output.
