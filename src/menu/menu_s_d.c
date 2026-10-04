#include "common.h"
#include "menu/menu_s.h"

/*
 * SimEvent handlers, 0x38C360..0x38CB38: gSimEvent[0..2] (table 0x3B7388, run by SimEvent_Run of
 * src/menu/menu_r_b.c), the three trainings of the sim day screen, each behind the roll of its outcome. The
 * object goes on in the next chunk (menu_t: SimEv03 at 0x38CB38 and the rest of the 37 handlers). These six
 * functions emit no read-only data.
 *
 * A handler is called once a frame with the day screen's work area until it returns 1; day->step is its
 * position. The three are one piece of source instantiated per training: step 0 rolls the outcome and puts
 * picture N on the monitor, step 1 shows line 0x59 with one of two sounds, step 2 shows the outcome's line
 * (0x5A + outcome) and queues the changes of attack, defence and health, step 3 ends the event.
 */

/* Rolls the outcome (0..5) of training 0: one draw of 0..100 against the weights of the repeat count. */
s32 SimEv00_Roll(SimDayS *day) {
    s32 r = Rand_Range(101);
    s32 sum;
    s32 i;

    if (day->flags & SIMDAY_FLAG1000) {
        return 5;
    }
    if (day->flags & SIMDAY_FLAG800) {
        return 0;
    }
    sum = 0;
    for (i = 0; i < SIMTRAIN_OUTCOMES; i++) {
        sum += day->train[0].weight[day->repeat][i];
        if (r < sum) {
            return i;
        }
    }
    return 5;
}

/* gSimEvent[0]: training 0. */
s32 SimEv00(SimDayS *day) {
    s32 atk[2];
    s32 def[2];
    s32 hp;

    switch (day->step) {
    case 0:
        gSimTrainOutcome0 = SimEv00_Roll(day);
        day->faceA = 1;
        SimDay_Cmd(5);
        day->step++;
        day->wait = 0;
        break;
    case 1:
        day->msgLine = 0x59;
        if (Rand_Range(2)) {
            Snd_PlaySe(2, 0x15);
        } else {
            Snd_PlaySe(2, 0x16);
        }
        day->flags |= SIMDAY_WAIT_KEY;
        day->step++;
        day->wait = 0;
        break;
    case 2:
        day->msgLine = gSimTrainOutcome0 + 0x5A;
        atk[0] = day->train[0].atkMin[gSimTrainOutcome0];
        atk[1] = day->train[0].atkMax[gSimTrainOutcome0];
        def[0] = day->train[0].defMin[gSimTrainOutcome0];
        def[1] = day->train[0].defMax[gSimTrainOutcome0];
        hp = day->train[0].hp[gSimTrainOutcome0];
        if (atk[0] == atk[1]) {
            SimDay_AddChange(0, atk[0]);
        } else {
            SimDay_AddChange(0, Rand_Range(atk[1] - atk[0] + 1) + atk[0]);
        }
        if (def[0] == def[1]) {
            SimDay_AddChange(1, def[0]);
        } else {
            SimDay_AddChange(1, Rand_Range(def[1] - def[0] + 1) + def[0]);
        }
        SimDay_AddChange(2, hp);
        SimDay_Cmd(0x1A);
        day->flags |= SIMDAY_WAIT_KEY;
        day->step++;
        day->wait = 0;
        break;
    case 3:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Rolls the outcome (0..5) of training 1: one draw of 0..100 against the weights of the repeat count. */
s32 SimEv01_Roll(SimDayS *day) {
    s32 r = Rand_Range(101);
    s32 sum;
    s32 i;

    if (day->flags & SIMDAY_FLAG1000) {
        return 5;
    }
    if (day->flags & SIMDAY_FLAG800) {
        return 0;
    }
    sum = 0;
    for (i = 0; i < SIMTRAIN_OUTCOMES; i++) {
        sum += day->train[1].weight[day->repeat][i];
        if (r < sum) {
            return i;
        }
    }
    return 5;
}

/* gSimEvent[1]: training 1. */
s32 SimEv01(SimDayS *day) {
    s32 atk[2];
    s32 def[2];
    s32 hp;

    switch (day->step) {
    case 0:
        gSimTrainOutcome1 = SimEv01_Roll(day);
        day->faceA = 2;
        SimDay_Cmd(5);
        day->step++;
        day->wait = 0;
        break;
    case 1:
        day->msgLine = 0x59;
        if (Rand_Range(2)) {
            Snd_PlaySe(2, 0x15);
        } else {
            Snd_PlaySe(2, 0x16);
        }
        day->flags |= SIMDAY_WAIT_KEY;
        day->step++;
        day->wait = 0;
        break;
    case 2:
        day->msgLine = gSimTrainOutcome1 + 0x5A;
        atk[0] = day->train[1].atkMin[gSimTrainOutcome1];
        atk[1] = day->train[1].atkMax[gSimTrainOutcome1];
        def[0] = day->train[1].defMin[gSimTrainOutcome1];
        def[1] = day->train[1].defMax[gSimTrainOutcome1];
        hp = day->train[1].hp[gSimTrainOutcome1];
        if (atk[0] == atk[1]) {
            SimDay_AddChange(0, atk[0]);
        } else {
            SimDay_AddChange(0, Rand_Range(atk[1] - atk[0] + 1) + atk[0]);
        }
        if (def[0] == def[1]) {
            SimDay_AddChange(1, def[0]);
        } else {
            SimDay_AddChange(1, Rand_Range(def[1] - def[0] + 1) + def[0]);
        }
        SimDay_AddChange(2, hp);
        SimDay_Cmd(0x1A);
        day->flags |= SIMDAY_WAIT_KEY;
        day->step++;
        day->wait = 0;
        break;
    case 3:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Rolls the outcome (0..5) of training 2: one draw of 0..100 against the weights of the repeat count. */
s32 SimEv02_Roll(SimDayS *day) {
    s32 r = Rand_Range(101);
    s32 sum;
    s32 i;

    if (day->flags & SIMDAY_FLAG1000) {
        return 5;
    }
    if (day->flags & SIMDAY_FLAG800) {
        return 0;
    }
    sum = 0;
    for (i = 0; i < SIMTRAIN_OUTCOMES; i++) {
        sum += day->train[2].weight[day->repeat][i];
        if (r < sum) {
            return i;
        }
    }
    return 5;
}

/* gSimEvent[2]: training 2. */
s32 SimEv02(SimDayS *day) {
    s32 atk[2];
    s32 def[2];
    s32 hp;

    switch (day->step) {
    case 0:
        gSimTrainOutcome2 = SimEv02_Roll(day);
        day->faceA = 3;
        SimDay_Cmd(5);
        day->step++;
        day->wait = 0;
        break;
    case 1:
        day->msgLine = 0x59;
        if (Rand_Range(2)) {
            Snd_PlaySe(2, 0x15);
        } else {
            Snd_PlaySe(2, 0x16);
        }
        day->flags |= SIMDAY_WAIT_KEY;
        day->step++;
        day->wait = 0;
        break;
    case 2:
        day->msgLine = gSimTrainOutcome2 + 0x5A;
        atk[0] = day->train[2].atkMin[gSimTrainOutcome2];
        atk[1] = day->train[2].atkMax[gSimTrainOutcome2];
        def[0] = day->train[2].defMin[gSimTrainOutcome2];
        def[1] = day->train[2].defMax[gSimTrainOutcome2];
        hp = day->train[2].hp[gSimTrainOutcome2];
        if (atk[0] == atk[1]) {
            SimDay_AddChange(0, atk[0]);
        } else {
            SimDay_AddChange(0, Rand_Range(atk[1] - atk[0] + 1) + atk[0]);
        }
        if (def[0] == def[1]) {
            SimDay_AddChange(1, def[0]);
        } else {
            SimDay_AddChange(1, Rand_Range(def[1] - def[0] + 1) + def[0]);
        }
        SimDay_AddChange(2, hp);
        SimDay_Cmd(0x1A);
        day->flags |= SIMDAY_WAIT_KEY;
        day->step++;
        day->wait = 0;
        break;
    case 3:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}
