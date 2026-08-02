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
          // blocks until done, sets calibratedLatencyMs
        std::cout << "Running... press ENTER to stop\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');

        std::cin.get();



    engine.stop();

    return 0;
}
