#pragma once
#include "energy_model.h"
#include "monitor_config.h"

namespace source {
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

