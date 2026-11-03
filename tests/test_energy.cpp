#include "energy_model.h"
#include "diagnostic_helpers.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace energy;
    float value=999;
    assert(parseValue("-1527","W",Grid,value) && value==1527);
    assert(parseValue("3000","W",Grid,value) && value==-3000);
    assert(parseValue("3904","W",Battery,value) && value==-3904);
    assert(parseValue("-1.9","kW",Battery,value) && value==1900);
    assert(parseValue("0","W",Inverter,value) && value==0);
    assert(parseValue("28.5","%",Soc,value) && value==28.5f);
    assert(parseValue("905.5","Watt",InverterDc,value) && value==905.5f);
    assert(parseValue("0.9055","kW",InverterDc,value) && value==905.5f);
    assert(!parseValue("unknown","W",Inverter,value));
    assert(!parseValue("unavailable","%",Soc,value));
    assert(!parseValue("1527 junk","W",Grid,value));
    assert(!parseValue("nan","W",Home,value));
    assert(!parseValue("inf","W",Home,value));
    assert(!parseValue("1e999","W",Home,value));
    assert(!parseValue("-4","W",Home,value));
    assert(!parseValue("15","kWh",Inverter,value));
    assert(!parseValue("101","%",Soc,value));
    assert(!parseValue("28.5","",Soc,value));
    assert(!parseValue("","W",Home,value));

    const auto t=parseTimestamp("2026-11-03T00:00:00.123456+00:00");
    assert(t==1793664000123ULL);
    assert(t==parseTimestamp("2026-11-03T11:00:00.123+11:00"));
    assert(t==parseTimestamp("2026-11-02T19:00:00.123-05:00"));
    assert(parseTimestamp("2026-11-03T00:00:00Z")==1793664000000ULL);
    assert(parseTimestamp("2024-02-29T00:00:00Z")>0);
    assert(!parseTimestamp("2026-02-29T00:00:00Z"));
    assert(!parseTimestamp("2026-11-03T24:00:00Z"));
    assert(!parseTimestamp("2026-11-03T00:00:00+99:00"));
    assert(!parseTimestamp("2026-11-03T00:00:00Zjunk"));
    assert(!parseTimestamp("2026-11-03T00:00:00"));
    assert(!parseTimestamp(nullptr));

    Reading r{1527,true,1000,t};
    assert(fresh(r,2000,t+1000,180));
    assert(!fresh(r,200000,t+1000,180));
    assert(!fresh(r,2000,t+181000,180));
    assert(!fresh(r,2000,t-61000,180));
    assert(fresh(r,2000,0,180)); // Clock unavailable: transport only.
    r.reportedMs=0;
    assert(fresh(r,2000,t,180)); // No last_reported: age labelled unknown.
    r.valid=false;assert(!fresh(r,2000,t,180));
    r={0,true,UINT32_MAX-500,0};
    assert(fresh(r,500,0,180)); // millis rollover.
    assert(!fresh(r,200000,0,180));

    auto s=demoSnapshot(1000,0);
    assert(s.demo && s.readings[Inverter].value+s.readings[Grid].value==s.readings[Home].value);
    assert(s.readings[Battery].value==-3904 && s.readings[Soc].value==28.5f);
    assert(demoSnapshot(1000,1).readings[Grid].value<0);
    assert(demoSnapshot(1000,2).readings[Battery].value>0);
    assert(s.readings[Solar].valid && s.readings[Solar].value==4404);
    assert(demoSnapshot(1000,2).readings[Solar].value==0); // Battery-only discharge.
    assert(demoSnapshot(1000,3).readings[Solar].valid && demoSnapshot(1000,3).readings[Solar].value==0);

    // Controlled simultaneous fixtures, not a claim about current site values.
    // Charge: raw battery +1200 W, inverter DC 905.5 W => PV 2105.5 W.
    s.readings[InverterDc]={905.5f,true,2000,t};
    s.readings[Battery]={-1200,true,2000,t};
    s.readings[Inverter].value=9999; // AC must never enter the PV formula.
    deriveSolar(s,2100,10);
    assert(s.readings[Solar].value==2105.5f && metricFresh(s,Solar,2100,t+1000,180,10));
    // Discharge: raw battery -1200 W, inverter DC 2000 W => PV 800 W.
    s.readings[InverterDc].value=2000;s.readings[Battery].value=1200;
    deriveSolar(s,2100,10);assert(s.readings[Solar].value==800);
    // Grid charging: inverter DC -1200 W, raw battery +1200 W => no PV.
    s.readings[InverterDc].value=-1200;s.readings[Battery].value=-1200;
    deriveSolar(s,2100,10);assert(s.readings[Solar].valid && s.readings[Solar].value==0);
    s.readings[Battery].value=-1100;deriveSolar(s,2100,10);
    assert(s.readings[Solar].value==0); // Small negative balance is clamped.

    s.readings[InverterDc].value=1000;s.readings[Battery].value=-1200;
    deriveSolar(s,2100,10);assert(s.readings[Solar].value==2200);
    s.readings[Battery].valid=false;deriveSolar(s,2100,10);
    assert(!s.readings[Solar].valid && s.readings[Solar].value==2200);
    s.readings[Battery].valid=true;s.readings[InverterDc].valid=false;
    deriveSolar(s,2100,10);assert(!s.readings[Solar].valid);
    s.readings[InverterDc].valid=true;s.readings[Battery].reportedMs=t+11000;
    deriveSolar(s,2100,10);assert(!s.readings[Solar].valid); // Skewed upstream sample.
    s.readings[Battery].reportedMs=t;s.readings[Battery].receivedMs=20000;
    deriveSolar(s,20100,10);assert(!s.readings[Solar].valid); // Skewed receive times.
    s.readings[Battery].receivedMs=2000;deriveSolar(s,2100,10);
    assert(!metricFresh(s,Solar,2100,t+181000,180,10)); // Upstream stale.
    s.readings[Battery].reportedMs=0;deriveSolar(s,2100,10);
    assert(s.readings[Solar].reportedMs==0);
    assert(!metricFresh(s,Solar,2100,t+181000,180,10)); // Known stale DC cannot hide behind unknown battery time.
    s.readings[InverterDc].reportedMs=0;
    s.readings[InverterDc].receivedMs=UINT32_MAX-500;s.readings[Battery].receivedMs=250;
    deriveSolar(s,500,10);assert(s.readings[Solar].valid && metricFresh(s,Solar,500,0,180,10));
    assert(s.readings[Solar].receivedMs==UINT32_MAX-500);
    assert(skewLimitSeconds(2)==10 && skewLimitSeconds(5)==10 && skewLimitSeconds(30)==60);
    auto live=demoSnapshot(1000,0);
    assert(metricUsable(live,Home,false,1100,0,180,5)); // Demo needs no network.
    live.demo=false;
    assert(!metricUsable(live,Home,false,1100,0,180,5) && metricUsable(live,Home,true,1100,0,180,5));
    // Diagnostics describe failures without echoing arbitrary HA strings.
    bool numeric=false;
    assert(std::strcmp(diagnostics::stateKind("905.5",value,numeric),"numeric")==0 && numeric && value==905.5f);
    assert(std::strcmp(diagnostics::stateKind("private-token-content",value,numeric),"invalid")==0 && !numeric);
    assert(std::strcmp(diagnostics::safeUnit("private-token-content"),"unexpected")==0);
    assert(std::strcmp(diagnostics::safeUnit("Watt"),"W")==0);
    assert(std::strcmp(diagnostics::safeUnit("kWh"),"kWh")==0);
    assert(std::strcmp(diagnostics::readingReason({0,true,1000,t},2000,t+181000,180),"report-stale")==0);
    assert(std::strcmp(diagnostics::readingReason({0,true,1000,t},2000,t-61000,180),"report-in-future")==0);
    auto diag=demoSnapshot(1000,0);
    assert(std::strcmp(diagnostics::solarReason(diag,1100,0,180,10),"source-age-unknown")==0);
    diag.readings[InverterDc].valid=false;
    assert(std::strcmp(diagnostics::solarReason(diag,1100,0,180,10),"dc-invalid")==0);
    diag=demoSnapshot(1000,0);diag.readings[Battery].reportedMs=t;diag.readings[InverterDc].reportedMs=t+11000;
    assert(std::strcmp(diagnostics::solarReason(diag,1100,t+11000,180,10),"input-timestamp-skew")==0);
    std::cout<<"Energy decoding, ISO timestamps, freshness, rollover and solar charge/discharge fixtures passed.\n";
}

