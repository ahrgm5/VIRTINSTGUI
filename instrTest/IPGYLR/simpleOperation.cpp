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
        // IPG YLR Laser
        // =================================================================
		{
            std::cout << "\n--- Connecting to IPG YLR Laser ---\n";
            std::string ylrResource = "TCPIP0::192.168.3.232::10001::SOCKET";
            ipgYLR laser(rm.handle(), ylrResource);

            if (!laser.isOpen()) {
                throw std::runtime_error("Could not open the IPG VISA session.");
            }

            std::cout << "Connected! Identity: " << laser.getIdentity() << "\n";
            std::cout << "Laser Status: " << laser.getStatus() << "\n";
        }






    }
    catch (const std::exception& ex) {
        std::cerr << "\n[Runtime Exception]: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "\nApplication exiting." << std::endl;
    return 0;
}