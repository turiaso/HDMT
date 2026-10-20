#include "model/state_machine/state_machine.h"

inline StateMachine *StateMachine_init(State stateInitial, void *ctx) {
    StateMachine *sm = (StateMachine *)MEM_alloc(sizeof(StateMachine));
    sm->state = stateInitial;
    sm->ctx = ctx;
    return sm;
}

inline void StateMachine_update(StateMachine *sm) {
    if (sm && sm->state) {
        sm->state = sm->state(sm->ctx);
    }
}

inline void StateMachine_destroy(StateMachine **sm) {
    if (sm && *sm) {
        MEM_free(*sm);
        *sm = NULL;
    }
}

inline State STATE_NULL(void *ctx) {
    return STATE_NULL;
}

