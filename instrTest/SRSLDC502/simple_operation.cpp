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
        // SRS LDC 502 Laser Diode Controller
        // =================================================================
          {
            std::cout << "\n--- Connecting to LDC 502 ---\n";
            std::string ldcResource = "GPIB0::4::INSTR";
            LDC502 ldc(rm.handle(), ldcResource);

            if (!ldc.isOpen()) {
                throw std::runtime_error("Failed to connect to LDC laser.");
            }

           // std::cout << "Laser Controller Identity: " << ldc.query("*IDN") << std::endl;
            std::cout << "Laser Status: " << ldc.isOpen() << "\n";

            ldc.setCurrentSetpoint(250.0);
            ldc.getCurrentSetpoint();

            if (ldc.isLaserOutputEnabled()) {
                std::cout << "Laser output is enabled.\n";
            }
            else {
                std::cout << "Laser output is disabled. Enabling now...\n";
                ldc.setLaserOutput(true);
            }

            std::cout << "The laser status:" << ldc.isLaserOutputEnabled() << std::endl;
            std::cout << "The live current reading:" << ldc.getActualCurrent() << std::endl;
        }








    }
    catch (const std::exception& ex) {
        std::cerr << "\n[Runtime Exception]: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "\nApplication exiting." << std::endl;
    return 0;
}