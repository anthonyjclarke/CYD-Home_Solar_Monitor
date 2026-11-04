#include "debug_levels.h"
#include <cassert>
#include <cstring>
#include <iostream>

int main() {
    for (int i=0;i<=4;++i) assert(validDebugLevel(i));
    assert(!validDebugLevel(-1) && !validDebugLevel(5) && !validDebugLevel(256));
    assert(std::strcmp(debugLevelName(0),"off")==0);
    assert(std::strcmp(debugLevelName(4),"verbose")==0);
    CycleLogGate gate;
    assert(!gate.ready(0,0,DBG_LEVEL_INFO));
    assert(gate.ready(1000,1,DBG_LEVEL_INFO));
    for (unsigned cycle=2;cycle<=12;++cycle) assert(!gate.ready(cycle*5000,cycle,DBG_LEVEL_INFO));
    assert(gate.ready(61000,13,DBG_LEVEL_INFO));
    assert(!gate.ready(61001,13,DBG_LEVEL_INFO));
    assert(gate.ready(62000,14,DBG_LEVEL_INFO,true)); // Mode/optional request.
    assert(!gate.ready(62001,14,DBG_LEVEL_INFO,true)); // No repeated snapshot.
    assert(gate.ready(63000,14,DBG_LEVEL_VERBOSE)); // Level applies immediately.
    assert(gate.ready(64000,15,DBG_LEVEL_VERBOSE));
    assert(!gate.ready(65000,15,DBG_LEVEL_VERBOSE));
    assert(!gate.ready(66000,16,DBG_LEVEL_OFF));
    assert(gate.ready(67000,16,DBG_LEVEL_INFO)); // Off then re-enable.
    assert(gate.ready(68000,17,DBG_LEVEL_WARN));
    assert(!gate.ready(69000,18,DBG_LEVEL_WARN));
    CycleLogGate rollover;
    assert(rollover.ready(UINT32_MAX-30000,1,DBG_LEVEL_INFO));
    assert(!rollover.ready(29998,2,DBG_LEVEL_INFO));
    assert(rollover.ready(29999,3,DBG_LEVEL_INFO));
    std::cout << "Serial debug levels, minute cadence, verbose, mode changes, duplicate suppression, off/re-enable and rollover passed.\n";
}
