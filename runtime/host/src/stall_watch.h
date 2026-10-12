#ifndef BLUEWAKE_STALL_WATCH_H
#define BLUEWAKE_STALL_WATCH_H

#include <stdbool.h>
#include <stdint.h>

// The stall watch: when the game's own frame counter (g_Counter, 0x803E8140 in
// GZLE01 revision 0) stops while the host keeps delivering retraces, every game
// thread is blocked (the CPU sits in PPCHalt) and the game will not recover by
// itself. main.c then writes one [stall] block to the session log: each game
// thread's state, what it waits on and where it stopped, the disc driver's
// command and queues, and the host's pending disc completions. Once per stall;
// the counter moving again re-arms it. Time the host holds the game (a menu, the
// app in the background) delivers no retraces, so it never counts.
//
//   BLUEWAKE_STALL_SECONDS=N     retraces' worth of seconds without a game frame
//                                before the dump (default 5; 0 turns it off)
//   BLUEWAKE_STALL_DUMP_AT=R     also write the block once at retrace R, stalled
//                                or not (to check the output)

#define BLUEWAKE_STALL_GAME_COUNTER 0x803E8140u

typedef struct BluewakeStallWatch {
    uint32_t last_counter;
    uint64_t still_retraces;  // retraces since the counter last moved
    uint64_t limit;           // 0: off
    bool seen;
    bool reported;            // this stall was written already
} BluewakeStallWatch;

static inline void bluewake_stall_watch_init(BluewakeStallWatch* w, uint64_t limit_retraces) {
    w->last_counter = 0;
    w->still_retraces = 0;
    w->limit = limit_retraces;
    w->seen = false;
    w->reported = false;
}

// Once per retrace with the game's frame counter. True once per stall, when it
// has not moved for the limit; false otherwise. Before the counter first moves
// (the boot, before the game loop runs) nothing counts.
static inline bool bluewake_stall_watch_step(BluewakeStallWatch* w, uint32_t counter) {
    if (w->limit == 0)
        return false;
    if (!w->seen) {
        w->seen = true;
        w->last_counter = counter;
        return false;
    }
    if (counter != w->last_counter) {
        w->last_counter = counter;
        w->still_retraces = 0;
        w->reported = false;
        return false;
    }
    if (counter == 0u)  // the game loop has not started yet
        return false;
    if (++w->still_retraces < w->limit || w->reported)
        return false;
    w->reported = true;
    return true;
}

#endif
