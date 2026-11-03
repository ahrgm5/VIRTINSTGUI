#ifndef PM1__D_HPP
#define PM1__D_HPP


#include <instrument.hpp>


class PM100D : public Instrument{

public:

    ChannelSettings defaultPM100DSettings;
    ChannelSettings defaultSettings() const override{ return defaultPM100DSettings;}

    explicit PM100D(ViSession defaultRM) : Instrument(defaultRM){}

    PM100D(ViSession defaultRM, const std::string &resourceName, const ChannelSettings & settings = {}) : Instrument(defaultRM){

        if(!resourceName.empty()){

            open(resourceName, settings);
        }
    }


    double readPower();
    double readLambda();

    void setLambda();
    void setRange();


};
#endif // PM1__D_HPP
