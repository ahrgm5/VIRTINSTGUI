#include <iostream>
#include <exception>
#include <stdexcept>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

// Hardware and control headers
#include <vrm.hpp>
#include <velmex.hpp>

int main() {
    try {
        std::cout << "--- Initializing VISA Resource Manager ---\n";
        VisaResourceManager rm;


        {
            // =================================================================
            // Velmex VXM Motor Controller
            // =================================================================
            {
                std::cout << "\n--- Connecting to Velmex ---\n";
                std::string port = "ASRL10::INSTR";

                // 1. Connects automatically using the default settings
                VelmexVXM vxm(rm.handle(), port, VelmexVXM::defaultVelmexSettings());

                if (!vxm.isOpen()) {
                    throw std::runtime_error("Failed to open Velmex on " + port);
                }
                std::cout << "Velmex Connected! Status: " << vxm.isOpen() << std::endl;

                // 2. Clear out any startup noise
                vxm.write("C");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));

                char axis[3] = { 'X', 'Y', 'Z' };

                for (int i = 0; i < 3; i++) {

                    std::cout << "Moving Velmex " << axis[i] << " from Position: " << vxm.getPosition(i + 1);

                    std::string cmd = "G, C, I" + std::to_string(i + 1) + "M-800, R";
					vxm.write(cmd);

                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    std::cout << " ------> " << vxm.getPosition(i + 1) << '\n' << std::endl;

                }
            }
        }

/*
        {
            std::cout << "\n--- Connecting to Velmex ---\n";

            ChannelSettings settings = VelmexVXM::defaultVelmexSettings();


            VelmexVXM vxm(rm.handle(), "ASRL10::INSTR", settings);

            int baudRate = 19200;
            vxm.setBaudRate(baudRate);

         //   std::cout << " At baud rate of " << std::to_string(baudRate) << "\nSystem responds with:\n" << vxm.query("X") +"\n" << std::endl;

            std::cout << "Velmex Status:" << vxm.isOpen() << std::endl;



            std::cout << "Velmex 1: " << vxm.query("C,X") << std::endl;
            std::cout << "Velmex 2: " << vxm.query("C,Y") << std::endl;
            std::cout << "Velmex 3: " << vxm.query("C,Z") << std::endl;

            vxm.query("G, C, I1M800, R");
            vxm.query("G, C, I2M800, R");
            vxm.query("G, C, I3M800, R");

            std::cout << "MOVING ....." << std::endl;

        }

*/




    }
    catch (const std::exception& ex) {
        std::cerr << "\n[Runtime Exception]: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "\nApplication exiting." << std::endl;
    return 0;
}