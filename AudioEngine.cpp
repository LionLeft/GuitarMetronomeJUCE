#include "AudioEngine.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <algorithm>

// ============================================================================
// Timing helpers
// ============================================================================
double AudioEngine::nowSec()
{
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

int64_t AudioEngine::nowUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

// ============================================================================
// MicCallback — timestamps the buffer on arrival, then forwards to engine
// ============================================================================
void MicCallback::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int   numInputChannels,
    float* const* /*outputChannelData*/,
    int   /*numOutputChannels*/,
    int   numSamples,
    const juce::AudioIODeviceCallbackContext&)
{
    // Timestamp the moment this buffer arrived from the driver.
    // This is the most accurate wall-clock anchor we have for the input stream.
    // All per-sample times inside processMicBuffer are derived from this.
    double bufferArrivalSec = AudioEngine::nowSec();

    if (numInputChannels > 0 && inputChannelData[0] != nullptr)
        owner.processMicBuffer(inputChannelData[0], numSamples, bufferArrivalSec);
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
AudioEngine::AudioEngine()
{
    // Zero-initialise the atomic tick ring
    for (auto& slot : tickTimestampsUs)
        slot.store(0, std::memory_order_relaxed);

    std::cout << "AudioEngine created\n";
}

AudioEngine::~AudioEngine()
{
    stop();
    std::cout << "AudioEngine destroyed\n";
}

// ============================================================================
// start
// ============================================================================
void AudioEngine::start()
{
    if (isRunning) return;

    // Output only (clicks) — 0 input channels avoids WASAPI deadlock
    {
        auto err = outputManager.initialise(0, 2, nullptr, true);
        if (err.isNotEmpty())
            std::cout << "Output init error: " << err << "\n";
        else
            std::cout << "Output device opened OK\n";
        outputManager.addAudioCallback(this);
    }

    // Input only (mic) — separate manager, separate WASAPI session
    {
        auto err = inputManager.initialise(1, 0, nullptr, true);
        if (err.isNotEmpty())
            std::cout << "Input init error: " << err << "\n";
        else
            std::cout << "Input device opened OK\n";
        inputManager.addAudioCallback(&micCallback);
    }

    isRunning = true;
    std::cout << "Audio started\n";
}

// ============================================================================
// stop
// ============================================================================
void AudioEngine::stop()
{
    if (!isRunning) return;

    outputManager.removeAudioCallback(this);
    outputManager.closeAudioDevice();

    inputManager.removeAudioCallback(&micCallback);
    inputManager.closeAudioDevice();

    isRunning = false;
    std::cout << "Audio stopped\n";
}

void AudioEngine::audioDeviceStopped() {}


// ============================================================================
// BPM / Subdivision
// ============================================================================
void AudioEngine::setBPM(float newBpm)
{
    bpm = newBpm;

    double beatDuration    = 60.0 / bpm;
    double subTickDuration = beatDuration / notesPerBeat;

    samplesPerTick        = subTickDuration * sampleRate;
    samplesUntilNextClick = samplesPerTick;
}

void AudioEngine::setSubdivision(int choice)
{
    double beatSec = 60.0 / bpm;
    notesPerBeat   = 1;

    if      (choice == 2) { notesPerBeat = 1; beatSec *= 2; }
    else if (choice == 3) { notesPerBeat = 3; }
    else if (choice == 4) { notesPerBeat = 2; }
    else                  { notesPerBeat = 1; }

    samplesPerTick        = (beatSec / notesPerBeat) * sampleRate;
    samplesUntilNextClick = samplesPerTick;
    subTickIndex          = 0;
}

// ============================================================================
// Output callback — click generation + tick timestamp recording
// ============================================================================
void AudioEngine::audioDeviceIOCallbackWithContext(
    const float* const* /*inputChannelData*/,
    int   /*numInputChannels*/,
    float* const* outputChannelData,
    int   numOutputChannels,
    int   numSamples,
    const juce::AudioIODeviceCallbackContext&)
{
    const int clickDuration = static_cast<int>(sampleRate * 0.03);

    // Timestamp the start of this output buffer once (one syscall per buffer)
    // Each sample's true time = bufferStartSec + (i / sampleRate)
    double bufferStartSec = nowSec();

    for (int i = 0; i < numSamples; ++i)
    {
        float sample = 0.0f;

        if (samplesUntilNextClick <= 0)
        {
            bool accent = (subTickIndex == 0);

            clickFrequency   = accent ? 1200.0f : 800.0f;
            clickAmplitude   = accent ? 1.0f : 0.5f;
            clickPhase       = 0.0f;
            clickSamplesLeft = clickDuration;

            samplesUntilNextClick += samplesPerTick;
            subTickIndex = (subTickIndex + 1) % notesPerBeat;

            // ── Record sample-accurate tick timestamp ─────────────────────
            // bufferStartSec is when sample 0 of this buffer was handed to
            // the driver, so sample i fired i/sampleRate seconds later.
            double tickTimeSec = bufferStartSec + static_cast<double>(i) / sampleRate;
            int64_t tickTimeUs = static_cast<int64_t>(tickTimeSec * 1e6);

            // Write into the ring buffer slot, then advance the write index.
            // The input thread reads writeIndex first and only reads slots
            // strictly below it, so there is no torn-read risk.
            int slot = tickWriteIndex.load(std::memory_order_relaxed) & kTickMask;
            tickTimestampsUs[slot].store(tickTimeUs, std::memory_order_release);
            tickWriteIndex.fetch_add(1, std::memory_order_release);

            samplesSinceLastTick = 0;
        }

        samplesUntilNextClick--;
        samplesSinceLastTick++;

        // Click synthesis — unchanged
        if (clickSamplesLeft > 0)
        {
            float progress = 1.0f - (float)clickSamplesLeft / (float)clickDuration;
            float envelope = std::exp(-progress * 8.0f) * clickAmplitude;
            sample         = std::sin(clickPhase) * envelope;

            clickPhase += juce::MathConstants<float>::twoPi * clickFrequency / sampleRate;
            if (clickPhase >= juce::MathConstants<float>::twoPi)
                clickPhase -= juce::MathConstants<float>::twoPi;

            --clickSamplesLeft;
        }

        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                outputChannelData[ch][i] = sample * 0.3f;
    }
}

// ============================================================================
// processMicBuffer — runs on the input device thread
//
// WHY timestamps instead of sample counters:
//   The output and input devices run on separate hardware clocks (two
//   independent crystal oscillators inside the audio interface or motherboard).
//   Even if both are set to 44100 Hz, they drift by 50–200 ppm relative to
//   each other.  Over 60 seconds that is 3–12 ms of accumulated error.
//   steady_clock is a single CPU monotonic clock that both threads read
//   independently — it gives both sides a shared time reference regardless of
//   which hardware device they belong to, so drift is eliminated.
// ============================================================================
void AudioEngine::processMicBuffer(const float* data, int numSamples, double bufferArrivalSec)
{
    // The buffer arrival timestamp is when the LAST sample in the buffer
    // reached the driver (the most recent sample = "now").
    // Sample i happened (numSamples - 1 - i) samples before the end,
    // so its true time = arrivalSec - (numSamples - 1 - i) / sampleRate.

    for (int i = 0; i < numSamples; ++i)
    {
        float inputSample = data[i];

        // ── Envelope follower (input thread only, no atomics needed) ─────────
        // Tracks a running estimate of signal energy.
        // Attack fast (envelopeAttack close to 1) so the envelope rises
        // quickly when a note hits; release slow so it decays after the note.
        float absIn = std::fabs(inputSample);
        if (absIn > envelopeFollower)
            envelopeFollower = envelopeAttack  * envelopeFollower + (1.0f - envelopeAttack)  * absIn;
        else
            envelopeFollower = envelopeRelease * envelopeFollower + (1.0f - envelopeRelease) * absIn;

        // Adaptive threshold = fixed base + current envelope.
        // When the string is still ringing (high envelope), the bar rises,
        // so a new onset must exceed the residual ring — not just the base.
        // This prevents repeated triggers from a single pluck.
        float adaptiveThreshold = onsetThreshold + envelopeFollower * 1.5f;

        samplesSinceLastOnset++;

        bool risingEdge = (absIn                        >  adaptiveThreshold) &&
                          (std::fabs(prevInputSample)   <= adaptiveThreshold);

        if (risingEdge && samplesSinceLastOnset >= debounceSamples)
        {
            samplesSinceLastOnset = 0;

            // Sample-accurate onset timestamp
            double onsetSec = bufferArrivalSec
                - static_cast<double>(numSamples - 1 - i) / sampleRate;
            int64_t onsetUs = static_cast<int64_t>(onsetSec * 1e6);

            // ── Find nearest metronome tick ───────────────────────────────
            // Read how many ticks the output thread has written.
            // Only read slots strictly below this index — they are fully committed.
            int writeIdx = tickWriteIndex.load(std::memory_order_acquire);

            if (writeIdx == 0)
            {
                prevInputSample = inputSample;
                continue;  // no ticks recorded yet
            }

            int64_t bestDiffUs = INT64_MAX;
            int     count      = std::min(writeIdx, kMaxTicks);

            for (int k = 0; k < count; ++k)
            {
                // Slot index for the k-th most recent tick
                int slot = (writeIdx - 1 - k) & kTickMask;
                int64_t tickUs = tickTimestampsUs[slot].load(std::memory_order_acquire);
                int64_t diff   = onsetUs - tickUs;

                if (std::abs(diff) < std::abs(bestDiffUs))
                    bestDiffUs = diff;
            }

            float errorMs = static_cast<float>(bestDiffUs) / 1000.0f;
            lastTimingErrorSamples = static_cast<int>(
                bestDiffUs * static_cast<int64_t>(sampleRate) / 1000000LL
            );

            // Console feedback
            std::cout << std::fixed << std::setprecision(1);
            if (std::fabs(errorMs) < 30.0f)
                std::cout << "*** PERFECT! ***\n";
            else if (errorMs > 0.0f)
                std::cout << "LATE  by " << errorMs << " ms\n";
            else
                std::cout << "EARLY by " << -errorMs << " ms\n";
        }

        prevInputSample = inputSample;
    }
}