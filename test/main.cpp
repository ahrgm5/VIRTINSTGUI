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
 /*		{
            std::cout << "\n--- Connecting to IPG YLR Laser ---\n";
            std::string ylrResource = "TCPIP0::192.168.3.232::10001::SOCKET";
            ipgYLR laser(rm.handle(), ylrResource);

            if (!laser.isOpen()) {
                throw std::runtime_error("Could not open the IPG VISA session.");
            }

            std::cout << "Connected! Identity: " << laser.getIdentity() << "\n";
            std::cout << "Laser Status: " << laser.getStatus() << "\n";
        }
*/		
		
/*
        // =================================================================
        // Arroyo 5240 Temperature Controller
        // =================================================================
        {
            std::cout << "\n--- Connecting to Arroyo ---\n";
            std::string arroyoResource = "ASRL4::INSTR";
            Arroyo5240 arroyo(rm.handle(), arroyoResource);

            if (!arroyo.isOpen()) {
                throw std::runtime_error("Could not open the Arroyo VISA session.");
            }

            std::cout << "Instrument: " << arroyo.getIdentity();
            std::cout << "Beep response: " << arroyo.beep();

            arroyo.setTemperatureSetpoint(30.0);
            std::cout << "Temperature setpoint: " << arroyo.getTemperatureSetpoint() << " C\n";
            std::cout << "Current Temperature: " << arroyo.getCurrentTemperature() << " C\n";
        }
*/


        // =================================================================
        // Velmex VXM Motor Controller
        // =================================================================
        {
            std::cout << "\n--- Connecting to Velmex ---\n";

            ChannelSettings settings = VelmexVXM::defaultVelmexSettings();


            VelmexVXM vxm(rm.handle(), "ASRL6::INSTR", settings);


            vxm.setBaudRate(38400);
            std::cout << "Velmex Status:" << vxm.isOpen() << std::endl;



            std::cout << "Velmex 1: " << vxm.query("C,X") << std::endl;
            std::cout << "Velmex 2: " << vxm.query("C,Y") << std::endl;
            std::cout << "Velmex 3: " << vxm.query("C,Z") << std::endl;

            vxm.query("G, C, I1M-800, R");
            vxm.query("G, C, I2M-800, R");
            vxm.query("G, C, I3M-800, R");

            std::cout << "MOVING ....." << std::endl;

        }


/*
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
*/



/*
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

*/





    }
    catch (const std::exception& ex) {
        std::cerr << "\n[Runtime Exception]: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "\nApplication exiting." << std::endl;
    return 0;
}