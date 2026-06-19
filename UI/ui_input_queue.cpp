#include "ui_input_queue.h"

#include <atomic>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

constexpr uint32_t kCapacity = 256;          // power of two for cheap masking
constexpr uint32_t kMask     = kCapacity - 1;

UIInputEvent g_buf[kCapacity];
std::atomic<uint32_t> g_head{ 0 };  // consumer read position
std::atomic<uint32_t> g_tail{ 0 };  // producer write position
uint32_t g_dropped = 0;

}  // namespace

namespace UIInput {

void Enqueue(const UIInputEvent& ev) {
    const uint32_t tail = g_tail.load(std::memory_order_relaxed);
    const uint32_t head = g_head.load(std::memory_order_acquire);
    if (tail - head >= kCapacity) {
        // Full: drop the incoming event. (Docs §4.2 originally said "drop oldest",
        // but the producer can't safely advance head in an SPSC queue, so we drop
        // the new event and count it. With 256 slots per frame this never triggers.)
        if ((++g_dropped & 0x3F) == 1) {
            char msg[64];
            wsprintfA(msg, "[UIInput] queue overflow, dropped=%u\n", g_dropped);
            OutputDebugStringA(msg);
        }
        return;
    }
    g_buf[tail & kMask] = ev;
    g_tail.store(tail + 1, std::memory_order_release);
}

bool Dequeue(UIInputEvent& out) {
    const uint32_t head = g_head.load(std::memory_order_relaxed);
    if (head == g_tail.load(std::memory_order_acquire)) return false;
    out = g_buf[head & kMask];
    g_head.store(head + 1, std::memory_order_release);
    return true;
}

void Clear() {
    g_head.store(g_tail.load(std::memory_order_acquire), std::memory_order_release);
}

}  // namespace UIInput
