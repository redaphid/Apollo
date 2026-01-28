/**
 * @file src/platform/linux/midi.cpp
 * @brief Linux MIDI stub implementation.
 * @note Future enhancement: implement using ALSA sequencer.
 */

#include <string>
#include <vector>

#include "src/logging.h"
#include "src/platform/common.h"

using namespace std::literals;

namespace platf {

  int midi_init() {
    BOOST_LOG(info) << "MIDI support not yet implemented on Linux"sv;
    return 0;
  }

  void midi_deinit() {
    // No-op
  }

  std::vector<midi_device_info_t> midi_list_devices() {
    return {};
  }

  int midi_open(const std::string &device_name) {
    BOOST_LOG(warning) << "MIDI not supported on this platform"sv;
    return -1;
  }

  void midi_close() {
    // No-op
  }

  int midi_send(const uint8_t *data, size_t length) {
    // Silently drop MIDI messages on unsupported platforms
    return -1;
  }

}  // namespace platf
