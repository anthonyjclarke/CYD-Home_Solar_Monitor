#pragma once
#include "history_model.h"
#include <cstdio>
#include <cstring>

// Shared, portable startup status for the TFT and change-only serial report.
namespace boot {
enum State : uint8_t { Info, Ok, Waiting, Problem };
enum Step : uint8_t { Wifi, Ip, Web, Signal, Ha, History, Clock, RowCount };
constexpr uint32_t diagnosticWindowMs = 60000;
struct Row { char value[48] = ""; State state = Waiting; };
struct Report {
    Row rows[RowCount];
    bool ready = false, warnings = false;
};
struct Inputs {
    bool connected = false, portal = false, clock = false;
    bool demo = false, demoReady = false, token = false, attempted = false;
    bool readerFailed = false;
    bool solar = false, sourceTimeKnown = false, historyHealthy = false;
    unsigned sources = 0, historySamples = 0;
    int rssi = 0;
    history::RestoreState restore = history::RestoreState::Starting;
    const char* ssid = "";
    const char* ip = "-";
    const char* web = "http://solar.local/";
    const char* localTime = "";
};
inline const char* label(unsigned step) {
    const char* labels[] = {"Wi-Fi", "IP address", "Web page", "Signal", "Home Asst", "History", "Clock"};
    return step < RowCount ? labels[step] : "";
}
inline const char* signalQuality(int rssi) {
    return rssi >= -55 ? "excellent" : rssi >= -67 ? "good" : rssi >= -75 ? "fair" : "weak";
}
inline Report build(const Inputs& in) {
    Report out;
    auto set = [&](Step step, State state, const char* text) {
        out.rows[step].state = state;
        std::snprintf(out.rows[step].value,sizeof(out.rows[step].value),"%s",text?text:"");
    };
    char text[48];
    if (in.connected) {
        std::snprintf(text,sizeof(text),"Connected - %.26s",in.ssid?in.ssid:"");
        set(Wifi,Ok,text); set(Ip,Info,in.ip);
        std::snprintf(text,sizeof(text),"%d dBm (%s)",in.rssi,signalQuality(in.rssi));
        set(Signal,Ok,text);
    } else {
        set(Wifi,in.portal?Problem:Waiting,in.portal?"Setup hotspot active":"Connecting...");
        set(Ip,Info,"-"); set(Signal,Waiting,"-");
    }
    set(Web,Info,in.web);
    if (in.readerFailed) set(Ha,Problem,"Reader failed - restart");
    else if (in.demo) set(Ha,in.demoReady?Info:Waiting,in.demoReady?"Demo (synthetic)":"Starting demo...");
    else if (!in.token) set(Ha,Problem,"No token - configure HA");
    else if (in.sources == energy::SourceCount && in.solar)
        set(Ha,in.sourceTimeKnown?Ok:Info,in.sourceTimeKnown?"6/6 sources + solar live":"Live / source age unknown");
    else if (!in.connected) set(Ha,Waiting,"Waiting for Wi-Fi");
    else if (!in.attempted) set(Ha,Waiting,"Waiting for first readings");
    else {
        std::snprintf(text,sizeof(text),"%u/6 live / %s",in.sources,in.solar?"partial data":"solar pending");
        set(Ha,Problem,text);
    }
    if (in.restore == history::RestoreState::Starting) set(History,Waiting,"Initializing...");
    else if (in.restore == history::RestoreState::Unavailable) set(History,Problem,"RAM only / mount failed");
    else if (!in.historyHealthy) set(History,Problem,"RAM only / checkpoint fault");
    else if (in.restore == history::RestoreState::MappingChanged) set(History,Info,"New source / collecting");
    else if (in.restore == history::RestoreState::Restored) {
        std::snprintf(text,sizeof(text),"Restored %u%s",in.historySamples,in.clock?" samples":" / clock pending");
        set(History,Ok,text);
    } else set(History,Ok,"Ready / collecting");
    if (in.clock) {
        std::snprintf(text,sizeof(text),"%.30s - synced",in.localTime?in.localTime:"");
        set(Clock,Ok,text);
    } else set(Clock,Waiting,"Waiting for time server");
    const bool sourceReady = !in.readerFailed && (in.demo ? in.demoReady : in.token && in.sources == energy::SourceCount && in.solar);
    out.ready = in.connected && in.clock && sourceReady && in.restore != history::RestoreState::Starting;
    for (auto& row : out.rows) {
        out.warnings |= row.state == Problem;
        // Keep terminal output to one line per row and text compatible with
        // the TFT's built-in ASCII fonts, even for unusual Wi-Fi names.
        for (char& ch : row.value) {
            if (!ch) break;
            if (static_cast<unsigned char>(ch) < 32 || static_cast<unsigned char>(ch) > 126) ch = '?';
        }
    }
    return out;
}
inline unsigned restoredSampleCount(const history::Store& store, uint32_t persistedThrough, uint32_t epoch) {
    unsigned count=0;
    const bool clock=epoch>history::clockMinimum;
    for (const auto& r:store.records)
        if (r.valid && r.capturedAt<=persistedThrough && (!clock ||
            (r.capturedAt<=epoch && r.slot>=history::bucket(epoch)-(history::capacity-1)*history::interval))) ++count;
    return count;
}
struct Changes { unsigned rows = 0; bool finished = false, timedOut = false; };
class Tracker {
    Report previous;
    uint32_t started = 0;
    bool seen = false, ended = false;
public:
    void begin(uint32_t ms) { started = ms; seen = ended = false; }
    Changes update(const Report& report, uint32_t ms) {
        Changes out;
        if (ended) return out;
        for (unsigned i=0;i<RowCount;++i) {
            // Clock ticks and RSSI jitter are not startup milestones.
            const bool valueChanged = i!=Clock && i!=Signal && std::strcmp(previous.rows[i].value,report.rows[i].value);
            if (!seen || previous.rows[i].state!=report.rows[i].state || valueChanged) out.rows |= 1U<<i;
        }
        previous = report; seen = true;
        if (report.ready || uint32_t(ms-started) >= diagnosticWindowMs) {
            ended = out.finished = true; out.timedOut = !report.ready;
        }
        return out;
    }
};
}  // namespace boot
