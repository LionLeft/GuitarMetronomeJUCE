#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "AudioEngine.h"
#include <thread>
#include <chrono>
#include <random>

int main()
{
    int bpm;
    juce::ScopedJuceInitialiser_GUI init;

    AudioEngine engine;

    int subdivisionChoice;
    while (true)
        {



        std::cout<<"Please give the bpm:\n";
        if (std::cin>>bpm&&bpm>0)
        break;
        std::cout<<"invalid bpm try again \n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        }
        std::cout<<"\n Please choose subdivision:\n ";
        std::cout<<"1 =Quarter notes\n";
        std::cout<<"2 =Half notes\n";
        std::cout<<"3 =Tripets\n";
        std::cout<<"4 =Eight  notes\n";
        std::cin >> subdivisionChoice;
        engine.setBPM((float)bpm);
        engine.setSubdivision(subdivisionChoice);
        engine.start();

        // Measure this computer's click-to-mic delay. Speakers must be audible
        // and the room quiet: the mic has to hear the metronome, not the guitar.
        std::cout << "Calibrating... keep quiet, speakers on.\n";
        engine.startCalibration();
        auto calStart = std::chrono::steady_clock::now();
        int clicksShown = 0;   // last progress value printed
        while (!engine.isCalibrationDone())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            // Printed from the main thread (not the audio thread) so console I/O
            // can never cause audio dropouts.
            int heard = engine.getCalibrationCount();
            if (heard != clicksShown)
            {
                clicksShown = heard;
                std::cout << "Click heard (" << heard << "/" << engine.getCalibrationTotal() << ")\n";
            }

            if (std::chrono::steady_clock::now() - calStart > std::chrono::seconds(30))
            {
                std::cout << "Couldn't hear the click. Raise the volume and retry.\n";
                break;
            }
        }
        if (engine.isCalibrationDone())
        {
            std::cout << "*** CALIBRATION SUCCESS! All " << engine.getCalibrationTotal()
                      << " clicks heard. ***\n";
            std::cout << "Latency: " << engine.getCalibrationOffsetUs() / 1000.0 << " ms\n";
        }

        std::cout << "Running... press ENTER to stop\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');

        std::cin.get();



    engine.stop();

    return 0;
}