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

            Zaber motionStage(rm.handle(), "ASRL5::INSTR");

            int numberOfDevices = 4;

            motionStage.write("/0 driver enable");
            motionStage.write("/0 activate");

            std::cout << motionStage.isOpen() << std::endl;
            std::cout << "Gripper Axis Position: " << motionStage.getPosition(3) << std::endl;


           // motionStage.write("/3 stop");

            std::this_thread::sleep_for(std::chrono::seconds(5));


            motionStage.moveAbsolute(3, 0);
            motionStage.moveAbsolute(3, 12000000);

            std::this_thread::sleep_for(std::chrono::seconds(5));

            std::cout << "Axis 3 Motor Position" << motionStage.getPosition(3) << std::endl;
            std::cout << "Axis 4 Motor Position" << motionStage.getPosition(4) << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(5));

            motionStage.home(3);
            motionStage.home(4);



          }






    }
    catch (const std::exception& ex) {
        std::cerr << "\n[Runtime Exception]: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "\nApplication exiting." << std::endl;
    return 0;
}