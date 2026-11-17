#include <iostream>
#include <exception>
#include <stdexcept>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

// Hardware and control headers
#include <vrm.hpp>
#include <zaber.hpp> 
#include <velmex.hpp>
#include <ldc.hpp>
#include <ipg.hpp>
#include <remoteControl.hpp>

int main() {
    try {
        std::cout << "--- Initializing VISA Resource Manager ---\n";
        VisaResourceManager rm;



        // =================================================================
        // Zaber Motion Stages
        // =================================================================
          {
            std::cout << "\n--- Connecting to Zabers ---\n";

            Zaber motionStage(rm.handle(), "ASRL5::INSTR", Zaber::defaultZaberSettings());

            int numberOfDevices = 4;

            motionStage.write("/1 1 driver enable");
            motionStage.write("/1 1 activate");

            std::cout << motionStage.isOpen() << std::endl;
            std::cout << "Gripper Axis Position: " << motionStage.getPosition(1) << std::endl;


           // motionStage.write("/3 stop");

            std::this_thread::sleep_for(std::chrono::seconds(2));


            motionStage.moveAbsolute(1, 40000);
            //motionStage.moveAbsolute(3, 12000000);

            std::this_thread::sleep_for(std::chrono::seconds(2));

            std::cout << "Axis 3 Motor Position: " << motionStage.getPosition(3) << std::endl;
            std::cout << "Axis 4 Motor Position: " << motionStage.getPosition(4) << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(2));

            motionStage.home(1);
            motionStage.home(0);



          }






    }
    catch (const std::exception& ex) {
        std::cerr << "\n[Runtime Exception]: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "\nApplication exiting." << std::endl;
    return 0;
}