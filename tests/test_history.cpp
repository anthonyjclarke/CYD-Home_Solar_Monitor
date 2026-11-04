#include "history_model.h"
#include "touch_gesture.h"
#include <cassert>
#include <iostream>
#include <vector>

using namespace history;
static std::vector<uint8_t> bank(const Store& s, Header h) {
    std::vector<uint8_t> bytes;
    assert(encode([&](uint8_t b) { bytes.push_back(b); return true; },s,h));
    assert(bytes.size()==encodedSize);
    return bytes;
}
static bool restore(const std::vector<uint8_t>& bytes, Header& h, Store* s=nullptr) {
    size_t i=0;
    return decode([&]() -> int { return i<bytes.size()?bytes[i++]:-1; },h,s);
}
int main() {
    const uint32_t epoch=bucket(1793836800), ms=10000;
    auto snap=energy::demoSnapshot(ms,0); snap.demo=false;
    for (auto& r : snap.readings) r.reportedMs=uint64_t(epoch)*1000;
    snap.heartbeatMs=uint64_t(epoch)*1000;
    auto r=capture(snap,true,ms,epoch+1,180,5);
    assert(r.slot==epoch && r.capturedAt==epoch+1 && r.valid==allFields);
    assert(r.values[Solar]==4404 && r.values[Battery]==-3904 && r.values[Grid]==1527);
    assert(r.values[Soc]==28.5f && r.has(Today) && r.has(TomorrowCode));
    assert(!capture(snap,false,ms,epoch,180,5).valid);
    assert(!capture(snap,true,ms,0,180,5).valid);
    snap.demo=true; assert(!capture(snap,true,ms,epoch,180,5).valid); snap.demo=false;
    snap.readings[energy::Battery].valid=false;
    auto partial=capture(snap,true,ms,epoch,180,5);
    assert(!partial.has(Battery) && !partial.has(Solar) && partial.has(Grid));
    snap.readings[energy::Battery].valid=true;
    snap.readings[energy::InverterDc].receivedMs=ms-11000U;
    assert(!capture(snap,true,ms,epoch,180,5).has(Solar));
    snap.readings[energy::InverterDc].receivedMs=ms;
    for (auto& reading : snap.readings) reading.reportedMs=uint64_t(epoch-181)*1000;
    snap.heartbeatMs=uint64_t(epoch-181)*1000;
    auto stale=capture(snap,true,ms,epoch,180,5);
    assert(!(stale.valid & ((1<<energy::Count)-1)) && stale.has(Today));
    for (auto& reading : snap.readings) reading.reportedMs=uint64_t(epoch+61)*1000;
    snap.heartbeatMs=uint64_t(epoch+61)*1000;
    assert(!capture(snap,true,ms,epoch,180,5).has(Grid));
    for (auto& reading : snap.readings) reading.reportedMs=0;
    snap.heartbeatMs=0;
    auto unknown=capture(snap,true,ms,epoch,180,5);
    assert(unknown.has(Grid) && unknown.sourceTimeKnown==0);
    for (auto& reading : snap.readings) reading.receivedMs=UINT32_MAX-100;
    snap.totals[0].receivedMs=UINT32_MAX-100;
    assert(capture(snap,true,100,epoch,180,5).has(Grid));
    assert(!capture(snap,true,180001,epoch,180,5).has(Grid));
    snap.weather.valid=false;
    assert(!capture(snap,true,100,epoch,180,5).has(Temperature));

    Store s;
    assert(s.add(r) && !s.add(r)); // One real snapshot per UTC five-minute slot.
    assert(!s.at(epoch+interval)); // Never manufacture readings for missed slots.
    auto next=r; next.slot+=2*interval; next.capturedAt+=2*interval; next.values[Grid]=-1000;
    assert(s.add(next));
    auto st=stats(s,Grid,epoch+2*interval+2);
    assert(st.count==2 && st.minimum==-1000 && st.maximum==1527 && st.mean==263.5f && st.latest==-1000);
    assert(stats(s,Grid,0).count==0);
    // Checkpoint round-trip includes sign, fractional units, actual capture
    // times, missing slots and the unknown-upstream-time mask.
    Header h{0x12345678,7}, got;
    const auto original=bank(s,h); Store loaded;
    assert(restore(original,got,&loaded) && got.identity==h.identity && got.generation==7);
    assert(loaded.at(epoch)->capturedAt==epoch+1 && loaded.at(epoch)->values[Soc]==28.5f);
    assert(!loaded.at(epoch+interval));
    const auto old=original;
    auto newerBank=bank(s,{h.identity,8});
    Header h0,h1;
    assert(restore(old,h0) && restore(newerBank,h1) && newer(h1.generation,h0.generation));
    assert(newer(0,UINT32_MAX) && !newer(UINT32_MAX,0));
    // Power loss at every byte of a checkpoint keeps the previous bank valid.
    for (size_t length=0;length<newerBank.size();++length) {
        std::vector<uint8_t> torn(newerBank.begin(),newerBank.begin()+length);
        assert(!restore(torn,got));
    }
    assert(restore(old,got));
    // Header, identity, generation, record and trailer corruption rejected.
    for (size_t pos : {size_t(0),size_t(4),size_t(8),size_t(12),size_t(16),size_t(20),size_t(500),original.size()-1}) {
        auto corrupt=original; corrupt[pos]^=1; assert(!restore(corrupt,got));
    }
    auto trailing=original; trailing.push_back(0); assert(!restore(trailing,got));
    unsigned written=0;
    assert(!encode([&](uint8_t) { return ++written<100; },s,h));
    // Clock moves backwards: discard all future capture times, including one
    // from the same slot, and accept a new timestamp without freezing a day.
    assert(s.prune(epoch) && !s.at(epoch) && !s.at(epoch+2*interval));
    auto back=r; back.capturedAt=epoch;
    assert(s.add(back));
    assert(s.prune(epoch+duration) && !s.at(epoch));
    // More than a full day across midnight rolls the buffer, never resets it.
    for (unsigned i=0;i<capacity+20;++i) {
        auto point=r; point.slot=epoch+i*interval; point.capturedAt=point.slot+1;
        point.valid=1U<<Grid; point.sourceTimeKnown=0; point.values[Grid]=float(i);
        assert(s.add(point));
    }
    auto end=epoch+(capacity+19)*interval+2;
    assert(stats(s,Grid,end).count==capacity && !s.at(epoch+19*interval));
    assert(stats(s,Grid,end).minimum==20);
    assert(s.prune(end+duration) && stats(s,Grid,end+duration).count==0);

    ui::TouchGesture gesture;
    using ui::Gesture;
    assert(gesture.update(true,100)==Gesture::None);
    assert(gesture.update(false,149)==Gesture::None); // Noise.
    gesture.update(true,200);
    assert(gesture.update(false,300)==Gesture::Tap);
    gesture.update(true,1000);
    assert(gesture.update(true,3000)==Gesture::None); // Strictly more than 2s.
    assert(gesture.update(true,3001)==Gesture::Hold);
    assert(gesture.update(true,6000)==Gesture::None);
    assert(gesture.update(false,6100)==Gesture::None); // No tap after hold.
    gesture.update(true,7000,true);
    assert(gesture.update(true,10000)==Gesture::None && gesture.update(false,10100)==Gesture::None);
    gesture.update(true,UINT32_MAX-1000);
    assert(gesture.update(true,1001)==Gesture::Hold);
    std::cout << "History capture, gaps, retention, clock correction, checkpoint corruption/power loss and touch gestures passed.\n";
}
