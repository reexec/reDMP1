#ifndef REDMP1_RECOMP_ABI_H
#define REDMP1_RECOMP_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the psprecomp ABI adapter and register repsp_guest_cpu_ops with repsp.
void recomp_abi_init(void);

// Reset ABI state and tables (used by probe tests).
void recomp_abi_reset(void);

// Inspection helpers for testing
uint64_t recomp_abi_dispatch_misses(void);
uint32_t recomp_abi_last_miss(void);

#ifdef __cplusplus
}
#endif

#endif // REDMP1_RECOMP_ABI_H
