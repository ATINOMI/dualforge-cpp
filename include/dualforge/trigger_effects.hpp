#pragma once
#include <array>
#include <cstdint>

namespace dualforge {
using TriggerEffect = std::array<std::uint8_t, 11>;
enum class EffectType : std::uint8_t {
    Off = 0x05, Feedback = 0x21, Weapon = 0x25, Vibration = 0x26,
    Bow = 0x22, Galloping = 0x23, Machine = 0x27
};
namespace effects {
// Invalid parameters throw std::invalid_argument; zero strength is not an alias for off.
TriggerEffect off();
TriggerEffect feedback(int position, int strength); // position 0..9, strength 1..8
TriggerEffect weapon(int start, int end, int strength); // 2 <= start < end <= 8, start <= 7
TriggerEffect vibration(int position, int amplitude, int frequency); // 0..9, 1..8, 1..255
// Experimental firmware modes, not guaranteed across firmware revisions.
TriggerEffect bow(int start, int end, int strength, int snap_force); // 0 <= start < end <= 8
// Machine: 1 <= start < end <= 9, start <= 8; amplitudes 0..7, frequency 1..255, period 0..255.
TriggerEffect machine(int start, int end, int amp_a, int amp_b, int frequency, int period);
// Galloping: 0 <= start < end <= 9; 0 <= first < second <= 7, frequency 1..255.
TriggerEffect galloping(int start, int end, int first_foot, int second_foot, int frequency);
TriggerEffect normal();
TriggerEffect very_soft();
TriggerEffect soft();
TriggerEffect medium();
TriggerEffect hard();
TriggerEffect very_hard();
TriggerEffect hardest();
TriggerEffect rigid();
TriggerEffect game_cube();
TriggerEffect semi_automatic_gun(int start = 2, int end = 7, int strength = 8);
TriggerEffect choppy();
TriggerEffect automatic_gun(int position = 0, int amplitude = 8, int frequency = 10);
TriggerEffect vibrate_trigger(int intensity = 10);
TriggerEffect vibrate_trigger_pulse();
TriggerEffect vibrate_trigger_custom(int position, int amplitude, int frequency);
// Only the seven known effect types are accepted; debug/calibration modes are excluded.
TriggerEffect custom(EffectType type, const std::array<std::uint8_t, 10>& params);
bool valid_type(std::uint8_t type) noexcept;
} // namespace effects
} // namespace dualforge
