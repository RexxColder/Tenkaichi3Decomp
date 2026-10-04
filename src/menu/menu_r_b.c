#include "common.h"
#include "menu/menu_r.h"

/*
 * SimEvent, 0x3851B0..0x3851E8: one function between the board screen (SimDay, work pointer 0x3B7384) and the
 * result screen (SimResult, 0x3B741C). Its table gSimEvent (0x3B7388..0x3B741C, 37 function pointers, .data)
 * lies between the two work pointers in the same way; the handlers are the event scripts at 0x38C408..0x392D20
 * (chunks menu_s, menu_t, menu_u). Nothing here emits read-only data and no branch depends on a neighbour, so the
 * file it belongs to is not decided: the last function of the SimDay object (then the table follows that
 * object's work pointer in its .data), or a file of its own in front of SimResult.
 */

/* Runs one step of event script `event` of the turn; non-zero when it is over (also for a number past the table). */
s32 SimEvent_Run(SimDay *day, u32 event) {
    s32 done = 1;

    if (event < 37) {
        done = gSimEvent[event](day);
    }
    return done;
}
