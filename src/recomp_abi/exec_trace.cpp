#include <cstdint>
#include <repsp/runtime/exec_history.h>

extern "C" void redmp1_note_guest_entry(uint32_t addr)
{
    repsp::runtime::NoteGuestEntry(addr);
}
