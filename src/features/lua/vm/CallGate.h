#pragma once

#include <cstdint>

namespace Cheat {
namespace Features {
namespace CallGate {

// we swap the implementation pointer in BoundFuncDesc and wait for the engine
// to call the method itself — then our stub executes one command on its thread.
// Hyperion guards .text, so inline hooks are out: we only write
// to .data and keep the stub in a foreign dll / xrw page.

bool Install(const char* method_name = nullptr);
void Remove();
bool Ready();

// rcx/rdx/r8/r9 = a0..a3, result from rax
bool Invoke(std::uint64_t fn,
            std::uint64_t a0, std::uint64_t a1,
            std::uint64_t a2, std::uint64_t a3,
            std::uint64_t* out_ret, unsigned timeout_ms = 3000);

// in-process buffer for out-parameters (>= 256 bytes)
std::uint64_t Scratch();

// how many times the slot fired since install — this is how we measure if it's hot
std::uint64_t Calls();

std::uint64_t SlotAddress();
int LastFail();

} // namespace CallGate
} // namespace Features
} // namespace Cheat
