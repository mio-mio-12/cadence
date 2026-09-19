#pragma once
#include <cstdint>

namespace gameplay::action_cancel {
enum class Kind { None, Inspect, Reload, Other };

inline bool mayMint(double now,double cooldownEnd,std::uint64_t shotSerial,
                    std::uint64_t consumedSerial,Kind entered) noexcept {
    return now<cooldownEnd&&shotSerial>0&&consumedSerial!=shotSerial&&
           (entered==Kind::Inspect||entered==Kind::Reload);
}

inline bool mayFire(bool freshPress,std::uint64_t shotSerial,
                    std::uint64_t tokenSerial,std::uint64_t consumedSerial,
                    Kind active) noexcept {
    return freshPress&&shotSerial>0&&tokenSerial==shotSerial&&
           consumedSerial!=shotSerial&&
           (active==Kind::Inspect||active==Kind::Reload);
}
} // namespace gameplay::action_cancel
