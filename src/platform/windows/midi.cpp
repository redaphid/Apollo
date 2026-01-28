/**
 * @file src/platform/windows/midi.cpp
 * @brief Windows MIDI implementation using Windows Multimedia API (winmm).
 */

// clang-format off
#include <Windows.h>
#include <mmsystem.h>
// clang-format on

#include <string>
#include <vector>

#include "src/config.h"
#include "src/logging.h"
#include "src/platform/common.h"

using namespace std::literals;

namespace platf {

  static HMIDIOUT midi_handle = nullptr;

  int midi_init() {
    BOOST_LOG(info) << "Initializing MIDI subsystem"sv;

    auto devices = midi_list_devices();
    BOOST_LOG(info) << "Found "sv << devices.size() << " MIDI output device(s)"sv;
    for (const auto &dev : devices) {
      BOOST_LOG(info) << "  MIDI device "sv << dev.id << ": "sv << dev.name;
    }

    // Open the configured MIDI device
    if (config::input.midi && !config::input.midi_device.empty()) {
      if (midi_open(config::input.midi_device) == 0) {
        BOOST_LOG(info) << "MIDI output device opened successfully"sv;
      } else {
        BOOST_LOG(warning) << "Failed to open MIDI output device"sv;
      }
    }

    return 0;
  }

  void midi_deinit() {
    BOOST_LOG(info) << "Shutting down MIDI subsystem"sv;
    midi_close();
  }

  std::vector<midi_device_info_t> midi_list_devices() {
    std::vector<midi_device_info_t> devices;
    UINT num_devs = midiOutGetNumDevs();

    for (UINT i = 0; i < num_devs; i++) {
      MIDIOUTCAPSW caps;
      if (midiOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
        // Convert wide string to UTF-8
        int len = WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, nullptr, 0, nullptr, nullptr);
        std::string name(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, name.data(), len, nullptr, nullptr);

        devices.push_back({(int) i, std::move(name)});
      }
    }

    return devices;
  }

  int midi_open(const std::string &device_name) {
    if (midi_handle) {
      midi_close();
    }

    UINT device_id = 0;  // Default to first device

    if (device_name != "auto" && !device_name.empty()) {
      // Find device by name
      auto devices = midi_list_devices();
      bool found = false;

      for (const auto &dev : devices) {
        if (dev.name == device_name) {
          device_id = dev.id;
          found = true;
          BOOST_LOG(info) << "Found MIDI device '"sv << device_name << "' at index "sv << device_id;
          break;
        }
      }

      if (!found) {
        BOOST_LOG(warning) << "MIDI device '"sv << device_name << "' not found, using default"sv;
      }
    }

    // Check if there are any devices
    if (midiOutGetNumDevs() == 0) {
      BOOST_LOG(warning) << "No MIDI output devices available"sv;
      return -1;
    }

    MMRESULT result = midiOutOpen(&midi_handle, device_id, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
      BOOST_LOG(error) << "Failed to open MIDI device: error "sv << result;
      midi_handle = nullptr;
      return -1;
    }

    BOOST_LOG(info) << "Opened MIDI output device "sv << device_id;
    return 0;
  }

  void midi_close() {
    if (midi_handle) {
      midiOutReset(midi_handle);
      midiOutClose(midi_handle);
      midi_handle = nullptr;
      BOOST_LOG(debug) << "Closed MIDI output device"sv;
    }
  }

  int midi_send(const uint8_t *data, size_t length) {
    if (!midi_handle) {
      BOOST_LOG(debug) << "MIDI send called but no device open"sv;
      return -1;
    }

    if (length == 0 || data == nullptr) {
      return -1;
    }

    // Short messages (up to 3 bytes) - most common MIDI messages
    if (length <= 3) {
      DWORD msg = data[0];
      if (length > 1) msg |= ((DWORD) data[1] << 8);
      if (length > 2) msg |= ((DWORD) data[2] << 16);

      MMRESULT result = midiOutShortMsg(midi_handle, msg);
      if (result != MMSYSERR_NOERROR) {
        BOOST_LOG(warning) << "MIDI short message send failed: error "sv << result;
        return -1;
      }

      return 0;
    }

    // SysEx messages (longer than 3 bytes)
    MIDIHDR header = {};
    header.lpData = (LPSTR) data;
    header.dwBufferLength = (DWORD) length;
    header.dwBytesRecorded = (DWORD) length;

    MMRESULT result = midiOutPrepareHeader(midi_handle, &header, sizeof(header));
    if (result != MMSYSERR_NOERROR) {
      BOOST_LOG(warning) << "MIDI prepare header failed: error "sv << result;
      return -1;
    }

    result = midiOutLongMsg(midi_handle, &header, sizeof(header));

    // Always unprepare the header, even if send failed
    midiOutUnprepareHeader(midi_handle, &header, sizeof(header));

    if (result != MMSYSERR_NOERROR) {
      BOOST_LOG(warning) << "MIDI long message send failed: error "sv << result;
      return -1;
    }

    return 0;
  }

}  // namespace platf
