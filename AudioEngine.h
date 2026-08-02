#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <array>
#include <chrono>

class AudioEngine;

// ============================================================================
// MicCallback — separate callback registered on the input device manager
// ============================================================================
class MicCallback : public juce::AudioIODeviceCallback
{
public:
    explicit MicCallback(AudioEngine& engine) : owner(engine) {}

    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}
    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext&) override;

private:
    AudioEngine& owner;
};

// ============================================================================
// AudioEngine
// ============================================================================
class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    AudioEngine();
    ~AudioEngine();

    void audioDeviceStopped() override;
    void setBPM(float newBpm);
    void start();
    void stop();
    void setSubdivision(int choice);

    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext&) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        sampleRate = device->getCurrentSampleRate();

        double beatSec    = 60.0 / bpm;
        double subTickSec = beatSec / notesPerBeat;

        samplesPerTick        = subTickSec * sampleRate;
        samplesUntilNextClick = samplesPerTick;

        // 100 ms debounce in samples (input sample rate ≈ output, close enough)
        debounceSamples = static_cast<int>(sampleRate * 0.10);
    }

    // Called by MicCallback on the input thread
    void processMicBuffer(const float* data, int numSamples, double bufferStartTimeSec);

    // Public so MicCallback can call them without friendship boilerplate
    static double  nowSec();
    static int64_t nowUs();

    double sampleRate             = 44100.0;
    int    lastTimingErrorSamples = 0;  // kept for compatibility
    // Blocks the calling thread (main) and returns average latency in ms.
    // Call AFTER start(), BEFORE the metronome loop — identical flow to RtAudio.

private:
    // ── Device managers ───────────────────────────────────────────────────────
    juce::AudioDeviceManager outputManager;
    juce::AudioDeviceManager inputManager;
    MicCallback              micCallback { *this };
    bool                     isRunning = false;

    // ── Metronome state (output thread only) ──────────────────────────────────
    float  bpm          = 120.0f;
    int    notesPerBeat = 1;

    double samplesUntilNextClick = 0.0;
    double samplesPerTick        = 0.0;
    int    tickCounter           = 0;
    float  clickPhase            = 0.0f;
    float  clickFrequency        = 800.0f;
    float  duration              = 0.03f;
    int    subTickIndex          = 0;
    int    beatsPerBar           = 4;
    float  amplitude             = 1.0f;
    float  clickAmplitude        = 0.5f;
    int    clickSamplesLeft      = 0;

    // ── Guitar input detection (input thread only) ────────────────────────────
    float prevInputSample      = 0.0f;
    float onsetThreshold       = 0.15f;   // base threshold
    int   samplesSinceLastTick = 0;
    int   debounceSamples      = 4410;
    int   samplesSinceLastOnset = 999999;

    // Envelope follower for adaptive threshold (input thread only)
    // Tracks signal energy so the threshold rises when the string rings on
    float envelopeFollower  = 0.0f;
    float envelopeAttack    = 0.999f;  // slow attack  — rises fast with signal
    float envelopeRelease   = 0.9995f; // slow release — falls slowly after note

    // ── Lock-free tick timestamp ring buffer ──────────────────────────────────
    // Written by the output thread, read by the input thread.
    // Uses a power-of-2 size so index wrapping is a bitmask.
    // Each slot is written atomically as a double (seconds, steady_clock).
    //
    // Protocol:
    //   Writer (output): increments writeIndex AFTER storing the timestamp.
    //   Reader (input):  reads writeIndex first, then reads slots up to that index.
    //   No mutex needed — reader only ever reads slots the writer has finished.
    static constexpr int kMaxTicks   = 64;   // must be power of 2
    static constexpr int kTickMask   = kMaxTicks - 1;

    // Each tick timestamp stored as int64 microseconds to allow atomic reads
    // on all platforms (std::atomic<double> is not always lock-free)
    std::array<std::atomic<int64_t>, kMaxTicks> tickTimestampsUs {};
    std::atomic<int>                            tickWriteIndex { 0 };




};