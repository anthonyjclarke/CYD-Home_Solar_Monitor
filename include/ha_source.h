#pragma once
#include "energy_model.h"
#include "monitor_config.h"

namespace source {
enum class ReaderState { NotStarted, Running, Failed };
ReaderState readerState();  // Startup status; main-task initialization owns it.
enum class ReadError { NotRead, None, Wifi, MissingCa, Clock, Url, Http, TooLarge, Json, Value };
struct RequestTrace {
    ReadError error = ReadError::NotRead;
    int httpStatus = 0;
    uint32_t elapsedMs = 0;
    const char* state = "not-read";
    const char* unit = "missing";
    float rawValue = 0;
    bool numeric = false;
};
enum class SourceMode { Live, Demo, TokenMissing };
struct CycleTrace {
    uint32_t id = 0;
    uint32_t elapsedMs = 0;
    uint32_t nextPollMs = 0;
    SourceMode mode = SourceMode::Live;
    RequestTrace requests[energy::SourceCount];
    bool totalsRead = false;  // Totals are read once a minute, not every cycle.
    RequestTrace totals[energy::TotalCount];
    bool weatherRead = false;  // Weather is fetched every 30 minutes.
    RequestTrace weather;
};
struct DiagnosticSnapshot {
    energy::Snapshot data;
    CycleTrace trace;
};
void begin();
void configure(const monitor::Config& config);
energy::Snapshot snapshot();
DiagnosticSnapshot diagnosticSnapshot();
void pause(bool value);
}  // namespace source

