#include <pm100D.hpp>


ChannelSettings defaultPM100DSettings(){

    ChannelSettings settings;
    settings.timeoutMs = 3000;
    settings.terminationCharacter = '\n';

    return settings;
}


double readPower(){

    double watts;
    return watts;
}


double readLambda(){

    double wavelength;
    return wavelength;
}


void setLambda(double newlambda){}

void setRange(double decimals_spaces){}