#pragma once

#include <atomic>
#include <string>
#include <vector>

#include "core/audio/midi.hpp"
#include "core/audio/processor.hpp"
#include "util/locked.hpp"

#include <RtAudio.h>

namespace otto::service::audio {
  struct RTAudioDriver {
    static RTAudioDriver& get() noexcept;

    void init();
    void shutdown();

    std::atomic_int samplerate = 48000;

    void send_midi_event(core::midi::AnyMidiEvent) noexcept;

    core::audio::AudioBufferPool& buffer_pool();

  private:
    RTAudioDriver() = default;
    ~RTAudioDriver() noexcept = default;

    RtAudio client;

    unsigned buffer_size = 256;

    util::atomic_swap<core::midi::shared_vector<core::midi::AnyMidiEvent>> midi_bufs = {{}, {}};

    int process(float* out_buf,
                 float* in_buf,
                 int nframes,
                 double stream_time,
                 RtAudioStreamStatus stream_status);

    std::unique_ptr<core::audio::AudioBufferPool> _buffer_pool;
  };

  using AudioDriver = RTAudioDriver;
} // namespace otto::service::audio

// kak: other_file=../../src/audio_driver.cpp