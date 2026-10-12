#include "stall_watch.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

int main(void) {
    BluewakeStallWatch w;

    // A running game: the counter moves every frame (every other retrace at 30 FPS), never a stall.
    bluewake_stall_watch_init(&w, 300);
    for (uint32_t r = 0; r < 100000; r++)
        assert(!bluewake_stall_watch_step(&w, 1000u + r / 2u));

    // A load that holds the counter for 4 seconds (240 retraces) is not a stall.
    bluewake_stall_watch_init(&w, 300);
    assert(!bluewake_stall_watch_step(&w, 50));
    for (int r = 0; r < 240; r++)
        assert(!bluewake_stall_watch_step(&w, 50));
    assert(!bluewake_stall_watch_step(&w, 51));

    // A stall: reported once, on the 300th retrace without a frame, then not again.
    bluewake_stall_watch_init(&w, 300);
    assert(!bluewake_stall_watch_step(&w, 70));
    for (int r = 1; r < 300; r++)
        assert(!bluewake_stall_watch_step(&w, 70));
    assert(bluewake_stall_watch_step(&w, 70));
    for (int r = 0; r < 10000; r++)
        assert(!bluewake_stall_watch_step(&w, 70));

    // The game recovers, then stalls again: reported again.
    assert(!bluewake_stall_watch_step(&w, 71));
    for (int r = 1; r < 300; r++)
        assert(!bluewake_stall_watch_step(&w, 71));
    assert(bluewake_stall_watch_step(&w, 71));

    // Before the game loop starts (counter 0 during the boot) nothing counts.
    bluewake_stall_watch_init(&w, 300);
    for (int r = 0; r < 5000; r++)
        assert(!bluewake_stall_watch_step(&w, 0));

    // Off.
    bluewake_stall_watch_init(&w, 0);
    for (int r = 0; r < 5000; r++)
        assert(!bluewake_stall_watch_step(&w, 9));
    return 0;
}
