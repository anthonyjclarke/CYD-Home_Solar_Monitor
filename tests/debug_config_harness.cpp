#include "monitor_config.h"
#include "debug.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <cassert>
#include <string>

std::atomic<uint8_t> debugLevel{DBG_LEVEL_OFF};
bool rejected = false;
void error(const char*) { rejected = true; }
// Extracted from saveConfig() so tests exercise the real API field validation.
#include "debug_validation.inc"

int main(int argc, char** argv) {
    assert(argc==2);
    const std::string mode=argv[1];
    Preferences::records["solar-monitor"]["access"]="123456789012";
    monitor::Config c;
    if (mode=="api") {
        for (const char* body : {"{\"debugLevel\":-1}","{\"debugLevel\":5}","{\"debugLevel\":256}",
             "{\"debugLevel\":true}","{\"debugLevel\":null}","{\"debugLevel\":\"0\"}","{\"debugLevel\":1.5}"}) {
            JsonDocument d; assert(!deserializeJson(d,body)); rejected=false;
            validateDebug(d,c); assert(rejected);
        }
        for (int i=0;i<=4;++i) {
            JsonDocument d; d["debugLevel"]=i; rejected=false;
            validateDebug(d,c); assert(!rejected && c.debugLevel==i);
        }
        JsonDocument oldClient; c.debugLevel=0; rejected=false;
        validateDebug(oldClient,c); assert(!rejected && c.debugLevel==0);
        return 0;
    }
    if (mode=="legacy") Preferences::records["cyd-core"]["debug_level"]="2";
    if (mode=="invalid-legacy") Preferences::records["cyd-core"]["debug_level"]="255";
    if (mode=="off" || mode=="legacy-schema" || mode=="invalid-saved") {
        JsonDocument d; d["schema"]=mode=="legacy-schema"?1:2;
        const unsigned count=mode=="legacy-schema"?5:energy::SourceCount;
        for (unsigned i=0;i<count;++i) d["entities"].add(c.entities[i]);
        if (mode=="off") d["debugLevel"]=0;
        if (mode=="invalid-saved") d["debugLevel"]=255;
        String body; serializeJson(d,body);
        Preferences::records["solar-monitor"]["settings"]=body.c_str();
    }
    monitor::loadConfig();
    const uint8_t expected=mode=="off"?0:mode=="legacy"?2:APP_DEFAULT_DEBUG_LEVEL;
    assert(monitor::config().debugLevel==expected && debugLevel.load()==expected);
    if (mode=="off") assert(Serial.calls==0);
    c=monitor::config(); c.debugLevel=0;
    assert(monitor::saveConfig(c) && debugLevel.load()==0);
    JsonDocument saved;
    assert(!deserializeJson(saved,Preferences::records["solar-monitor"]["settings"]));
    assert(saved["schema"]==2 && saved["debugLevel"].is<int>() && saved["debugLevel"]==0);
    monitor::loadConfig(); assert(monitor::config().debugLevel==0 && debugLevel.load()==0);
    Preferences::failWrites=true; c.debugLevel=4;
    assert(!monitor::saveConfig(c) && debugLevel.load()==0 && monitor::config().debugLevel==0);
}
