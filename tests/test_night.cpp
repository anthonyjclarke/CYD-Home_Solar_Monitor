#include "night_mode.h"
#include "touch_gesture.h"
#include <cassert>
#include <iostream>

int main() {
    night::Schedule s{true,night::DimThenOff,19*60+15,23*60+40,6*60+30};
    assert(night::valid(s));
    assert(night::phase(s,true,19*60+14)==night::Normal);
    assert(night::phase(s,true,19*60+15)==night::Dimmed);
    assert(night::phase(s,true,23*60+39)==night::Dimmed);
    assert(night::phase(s,true,23*60+40)==night::Off);
    assert(night::phase(s,true,0)==night::Off);
    assert(night::phase(s,true,6*60+29)==night::Off);
    assert(night::phase(s,true,6*60+30)==night::Normal);
    assert(night::phase(s,false,0)==night::Normal);
    s.enabled=false; assert(night::phase(s,true,0)==night::Normal); s.enabled=true;
    s.mode=night::DimOnly;
    assert(night::phase(s,true,0)==night::Dimmed);
    s.mode=night::OffOnly;
    assert(night::phase(s,true,23*60+39)==night::Normal);
    assert(night::phase(s,true,23*60+40)==night::Off);
    s.offAt=s.onAt; assert(night::phase(s,true,0)==night::Normal);
    s.mode=night::DimOnly; s.dimAt=s.onAt; assert(night::phase(s,true,0)==night::Normal);
    s={true,night::DimThenOff,9*60,12*60,17*60};
    assert(night::phase(s,true,8*60)==night::Normal);
    assert(night::phase(s,true,9*60)==night::Dimmed);
    assert(night::phase(s,true,12*60)==night::Off);
    assert(night::phase(s,true,17*60)==night::Normal);
    s.offAt=s.dimAt; assert(night::valid(s) && night::phase(s,true,9*60)==night::Off);
    s.offAt=s.onAt; assert(!night::valid(s) && night::phase(s,true,12*60)==night::Normal);
    s.offAt=8*60; assert(!night::valid(s));
    s.enabled=false; assert(night::valid(s)); // A disabled schedule never blacks out.
    s.mode=static_cast<night::Mode>(255); assert(!night::valid(s));
    s={true,night::DimThenOff,1438,1439,1};
    assert(night::valid(s) && night::phase(s,true,1438)==night::Dimmed);
    assert(night::phase(s,true,1439)==night::Off && night::phase(s,true,0)==night::Off);
    assert(night::phase(s,true,1)==night::Normal);
    assert(!night::minuteValid(-1) && !night::minuteValid(1440) && night::minuteValid(0) && night::minuteValid(1439));
    assert(night::brightness(night::Off,180,60000,0)==180);
    assert(night::brightness(night::Off,180,60001,0)==0);
    assert(night::brightness(night::Dimmed,180,60001,0)==36);
    assert(night::brightness(night::Dimmed,10,60001,0)==5);
    assert(night::brightness(night::Normal,180,60001,0)==180);
    const uint32_t touched=UINT32_MAX-30000;
    assert(night::brightness(night::Off,180,29999,touched)==180);
    assert(night::brightness(night::Off,180,30000,touched)==0);
    ui::TouchGesture gesture;
    assert(gesture.update(true,60001,night::brightness(night::Off,180,60001,0)<180)==ui::Gesture::None);
    assert(gesture.update(true,63002)==ui::Gesture::None); // Wake hold cannot open settings.
    assert(gesture.update(false,63003)==ui::Gesture::None); // No release tap.
    gesture.update(true,64000);
    assert(gesture.update(false,64100)==ui::Gesture::Tap); // Next separate touch works.
    std::cout<<"Night modes: exact boundaries, midnight/daytime, staged validation, no clock, wake consumption and rollover passed.\n";
}
