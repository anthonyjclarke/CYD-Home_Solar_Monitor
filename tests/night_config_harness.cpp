#include "monitor_config.h"
#include "debug.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <cassert>
#include <string>

std::atomic<uint8_t> debugLevel{DBG_LEVEL_OFF};
bool rejected=false;
void error(const char*) { rejected=true; }
#include "night_validation.inc"

int main(int argc,char** argv) {
    assert(argc==2); const std::string mode=argv[1];
    monitor::Config c;
    Preferences::records["solar-monitor"]["access"]="123456789012";
    if (mode=="api") {
        for (const char* body : {"{\"nightMode\":3}","{\"nightMode\":true}","{\"nightMode\":null}",
             "{\"nightMode\":\"0\"}","{\"nightDimAt\":-1}","{\"nightOffAt\":1440}",
             "{\"nightOnAt\":1.5}","{\"nightOnAt\":null}","{\"nightDimAt\":\"19:30\"}",
             "{\"nightMode\":2,\"nightOffAt\":420}","{\"nightMode\":2,\"nightOffAt\":600}"}) {
            JsonDocument d; assert(!deserializeJson(d,body)); rejected=false; c=monitor::Config{};
            validateNight(d,c,19,7); assert(rejected);
        }
        for (int i=0;i<3;++i) {
            JsonDocument d; d["nightMode"]=i; d["nightDimAt"]=1155; d["nightOffAt"]=1420; d["nightOnAt"]=390;
            c=monitor::Config{}; rejected=false; validateNight(d,c,19,7);
            assert(!rejected && c.nightMode==i && c.nightDimAt==1155 && c.nightOffAt==1420 && c.nightOnAt==390);
            assert(c.nightStart==(i==night::OffOnly?23:19) && c.nightEnd==6);
        }
        // An older client round-trip must not round away minute precision.
        c.nightMode=night::DimThenOff; assert(monitor::saveConfig(c));
        JsonDocument old; c=monitor::config(); rejected=false; validateNight(old,c,19,6);
        assert(!rejected && c.nightDimAt==1155 && c.nightOnAt==390);
        validateNight(old,c,20,8); assert(!rejected && c.nightDimAt==1200 && c.nightOnAt==480);
        c.nightMode=night::OffOnly; assert(monitor::saveConfig(c));
        c=monitor::config(); validateNight(old,c,22,8);
        assert(!rejected && c.nightOffAt==1320 && c.nightDimAt==1200);
        return 0;
    }
    if (mode!="default") {
        JsonDocument d; d["schema"]=mode=="legacy1"?1:2;
        for (unsigned i=0;i<(mode=="legacy1"?5:energy::SourceCount);++i) d["entities"].add(c.entities[i]);
        d["nightStart"]=21; d["nightEnd"]=8; d["nightDim"]=true;
        if (mode=="invalid") { d["nightMode"]=2; d["nightDimAt"]=1300; d["nightOffAt"]=800; d["nightOnAt"]=450; }
        String body; serializeJson(d,body); Preferences::records["solar-monitor"]["settings"]=body.c_str();
    }
    monitor::loadConfig(); c=monitor::config();
    assert(c.nightMode==night::DimOnly && c.nightDimAt==(mode=="default"?1140:1260) && c.nightOnAt==(mode=="default"?420:480));
    c.nightMode=night::DimThenOff; c.nightDimAt=0; c.nightOffAt=0; c.nightOnAt=60;
    assert(monitor::saveConfig(c)); monitor::loadConfig(); c=monitor::config();
    assert(c.nightDimAt==0 && c.nightOffAt==0 && c.nightOnAt==60);
    c.nightMode=night::DimThenOff; c.nightDimAt=1155; c.nightOffAt=1420; c.nightOnAt=390;
    assert(monitor::saveConfig(c)); monitor::loadConfig(); c=monitor::config();
    assert(c.nightMode==night::DimThenOff && c.nightDimAt==1155 && c.nightOffAt==1420 && c.nightOnAt==390);
    assert(c.nightStart==19 && c.nightEnd==6);
    JsonDocument saved; assert(!deserializeJson(saved,Preferences::records["solar-monitor"]["settings"]));
    assert(saved["schema"]==2 && saved["nightDimAt"]==1155 && saved["nightOffAt"]==1420 && saved["nightOnAt"]==390);
    Preferences::failWrites=true; c.nightMode=night::OffOnly;
    assert(!monitor::saveConfig(c) && monitor::config().nightMode==night::DimThenOff);
}
