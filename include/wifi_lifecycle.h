#pragma once
#include <cstdint>

namespace cyd::network {
enum class Phase : uint8_t { Joining, Portal, RetryWait, Online, Reconnecting };
enum class Action : uint8_t { None, OpenPortal, ClosePortal, Reconnect };
// Portable policy; WiFiManager owns portal timeouts and the adapter executes
// actions. Once Online, outages can only enter Reconnecting until next boot.
class Lifecycle {
    Phase phase_ = Phase::Joining;
    uint32_t entered_ = 0, lastAttempt_ = 0;
public:
    Phase phase() const { return phase_; }
    void begin(uint32_t now) { phase_ = Phase::Joining; entered_ = lastAttempt_ = now; }
    Action update(uint32_t now, bool connected, bool portalActive) {
        if (connected) {
            phase_ = Phase::Online;
            return portalActive ? Action::ClosePortal : Action::None;
        }
        if (phase_ == Phase::Online) phase_ = Phase::Reconnecting;
        if (phase_ == Phase::Portal && !portalActive) {
            phase_ = Phase::RetryWait;
            entered_ = now;
        }
        if ((phase_ == Phase::Joining || phase_ == Phase::RetryWait) &&
            !portalActive && uint32_t(now - entered_) >= 20000) {
            phase_ = Phase::Portal;
            entered_ = lastAttempt_ = now;
            return Action::OpenPortal;
        }
        // Reconnection must not reset the independent portal retry deadline.
        if (!portalActive && uint32_t(now - lastAttempt_) >= 30000) {
            lastAttempt_ = now;
            return Action::Reconnect;
        }
        return Action::None;
    }
};
}  // namespace cyd::network
