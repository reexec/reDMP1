/* psprecomp ABI memory bridge to repsp guest memory. */

#include <psprecomp/mem.h>
#include <repsp/recomp/guest_cpu.h>
#include <string.h>

uint64_t psp_mem_bad_access = 0;

static inline uint8_t* guest_ptr(uint32_t addr) {
    uint8_t* base = repsp_guest_base();
    uint32_t mask = repsp_guest_mask();
    if (!base) return NULL;
    return base + (addr & mask);
}

void *psp_mem_ptr(uint32_t addr, uint32_t size) {
    (void)size;
    return guest_ptr(addr);
}

uint8_t psp_read8(uint32_t addr) {
    uint8_t* p = guest_ptr(addr);
    if (!p) { psp_mem_bad_access++; return 0; }
    return *p;
}

uint16_t psp_read16(uint32_t addr) {
    uint8_t* p = guest_ptr(addr);
    if (!p) { psp_mem_bad_access++; return 0; }
    uint16_t val;
    memcpy(&val, p, sizeof val);
    return val;
}

uint32_t psp_read32(uint32_t addr) {
    uint8_t* p = guest_ptr(addr);
    if (!p) { psp_mem_bad_access++; return 0; }
    uint32_t val;
    memcpy(&val, p, sizeof val);
    return val;
}

float psp_read_f32(uint32_t addr) {
    uint32_t u = psp_read32(addr);
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

void psp_write8(uint32_t addr, uint8_t val) {
    uint8_t* p = guest_ptr(addr);
    if (!p) { psp_mem_bad_access++; return; }
    *p = val;
}

void psp_write16(uint32_t addr, uint16_t val) {
    uint8_t* p = guest_ptr(addr);
    if (!p) { psp_mem_bad_access++; return; }
    memcpy(p, &val, sizeof val);
}

void psp_write32(uint32_t addr, uint32_t val) {
    uint8_t* p = guest_ptr(addr);
    if (!p) { psp_mem_bad_access++; return; }
    memcpy(p, &val, sizeof val);
}

void psp_write_f32(uint32_t addr, float val) {
    uint32_t u;
    memcpy(&u, &val, sizeof u);
    psp_write32(addr, u);
}

int psp_mem_init(void) { return 0; }
void psp_mem_free(void) {}
int psp_mem_map_module(uint32_t base, uint32_t size) { (void)base; (void)size; return 0; }
int psp_mem_write_block(uint32_t addr, const void *src, uint32_t len) {
    uint8_t* p = guest_ptr(addr);
    if (!p) return -1;
    memcpy(p, src, len);
    return 0;
}
int psp_mem_read_block(void *dst, uint32_t addr, uint32_t len) {
    uint8_t* p = guest_ptr(addr);
    if (!p) return -1;
    memcpy(dst, p, len);
    return 0;
}
