#include "boot_status.h"
#include <cassert>
#include <iostream>

int main() {
    boot::Inputs in;
    in.token=true;
    auto report=boot::build(in);
    assert(!report.ready && !report.warnings);
    assert(report.rows[boot::Wifi].state==boot::Waiting && report.rows[boot::History].state==boot::Waiting);
    boot::Tracker tracker; tracker.begin(0);
    auto changes=tracker.update(report,0);
    assert(changes.rows==(1U<<boot::RowCount)-1 && !changes.finished);
    assert(tracker.update(report,250).rows==0);
    in.connected=true; in.ip="192.0.2.5"; in.ssid="Synthetic\nNetwork"; in.rssi=-50;
    report=boot::build(in);
    assert(!std::strchr(report.rows[boot::Wifi].value,'\n'));
    changes=tracker.update(report,500);
    assert(changes.rows&(1U<<boot::Wifi));
    assert(changes.rows&(1U<<boot::Ip));
    assert(changes.rows&(1U<<boot::Signal));
    in.rssi=-51;
    assert(tracker.update(boot::build(in),750).rows==0); // Ignore RSSI jitter.
    in.clock=true; in.localTime="17:08 AEDT";
    changes=tracker.update(boot::build(in),1000);
    assert(changes.rows==(1U<<boot::Clock));
    in.localTime="17:09 AEDT";
    assert(tracker.update(boot::build(in),1250).rows==0); // Ignore clock ticking.
    in.restore=history::RestoreState::Restored; in.historyHealthy=true; in.historySamples=123;
    report=boot::build(in);
    assert(std::strcmp(report.rows[boot::History].value,"Restored 123 samples")==0);
    assert(tracker.update(report,1500).rows==(1U<<boot::History));
    in.sources=3; in.attempted=true;
    report=boot::build(in);
    assert(report.warnings && report.rows[boot::Ha].state==boot::Problem && !report.ready);
    in.sources=energy::SourceCount; in.solar=true;
    report=boot::build(in);
    assert(report.ready && report.rows[boot::Ha].state==boot::Info);
    assert(std::strstr(report.rows[boot::Ha].value,"age unknown"));
    changes=tracker.update(report,2000);
    assert(changes.finished && !changes.timedOut);
    in.connected=false;
    assert(tracker.update(boot::build(in),3000).rows==0); // Reporting has ended.

    in=boot::Inputs{}; in.portal=true;
    report=boot::build(in);
    assert(report.warnings && report.rows[boot::Wifi].state==boot::Problem);
    assert(report.rows[boot::Ha].state==boot::Problem); // Missing token.
    boot::Tracker timeout; timeout.begin(UINT32_MAX-1000);
    assert(!timeout.update(report,58998).finished);
    changes=timeout.update(report,58999);
    assert(changes.finished && changes.timedOut);
    assert(!timeout.update(report,59000).finished); // One final report only.

    in.connected=in.clock=in.demo=in.demoReady=true;
    in.restore=history::RestoreState::Unavailable;
    report=boot::build(in);
    assert(report.ready && report.warnings && report.rows[boot::History].state==boot::Problem);
    assert(std::strstr(report.rows[boot::Ha].value,"synthetic"));
    in.historyHealthy=true; in.restore=history::RestoreState::MappingChanged;
    assert(boot::build(in).rows[boot::History].state==boot::Info);
    in.restore=history::RestoreState::Corrupt; in.historyHealthy=false;
    assert(boot::build(in).rows[boot::History].state==boot::Problem);
    in.restore=history::RestoreState::Restored; in.historyHealthy=true; in.clock=false;
    assert(std::strstr(boot::build(in).rows[boot::History].value,"clock pending"));
    in.clock=in.sourceTimeKnown=true; in.demo=false; in.token=true; in.sources=6; in.solar=true;
    assert(boot::build(in).rows[boot::Ha].state==boot::Ok);
    in.readerFailed=true;
    report=boot::build(in);
    assert(!report.ready && report.warnings && std::strstr(report.rows[boot::Ha].value,"Reader failed"));

    const uint32_t epoch=history::bucket(1793836800);
    history::Store store; history::Record r;
    r.slot=r.capturedAt=epoch; r.valid=1U<<history::Solar;
    assert(store.add(r));
    r.slot=r.capturedAt=epoch+history::interval; assert(store.add(r));
    assert(boot::restoredSampleCount(store,epoch,0)==1); // Exclude new RAM sample.
    assert(boot::restoredSampleCount(store,epoch+history::interval,epoch)==1); // Future record.
    assert(boot::restoredSampleCount(store,epoch,epoch+history::duration)==0); // Expired.
    assert(boot::restoredSampleCount(store,0,0)==0);
    std::cout<<"Shared boot status: readiness, faults, restore counts, change-only output, jitter, deadline and rollover passed.\n";
}
