#include <iostream>

#include "argparse.hpp"
#include "tmc2209_motor.h"
#include "constants_pi.h"

#include "cameracontrol.h"
#include "ccd_ctrl.h"

// Commandline test utility
struct ImgArgs : public argparse::Args {
    bool &nomove             = flag("s,static capture", "Do not move stepper");
    uint32_t &lines        = kwarg("l,lines", "Capture l lines").set_default(1);
    std::string &output    = arg("out,output file", "Image output or \"-\" for stdio").set_default("-");
};

struct StepperArgs : public argparse::Args {
    bool &home             = flag("h,home", "Home stepper");
    int &steps             = kwarg("s", "Move stepper s steps").set_default(0);
    std::optional<uint8_t> &microstep     = kwarg("ms", "Microstep multiplier options: ms256 = 0,ms128 = 1,ms64 = 2, ms32=3,ms16=4,ms8=5,ms4=6,ms2=7,fullstep=8");
    int &speed             = kwarg("v", "Speed v").set_default(500);
    int &accel             = kwarg("a", "Accel a").set_default(20);
    std::string& serial = kwarg("p","serial port, default /dev/ttyAMA0").set_default("/dev/ttyAMA0");
};


struct MyArgs : public argparse::Args {

    bool &verbose          = flag("v,verbose", "A flag to toggle verbose");

    ImgArgs &imgargs    = subcommand("img");
    StepperArgs &motorargs    = subcommand("mot");
};




int main(int argc, char* argv[]) {
    auto args = argparse::parse<MyArgs>(argc, argv);
    PinCtrl pinctrl;
    Motors motors;
    pinctrl.setEnablePin(true);

    if (args.verbose)
        args.print();      // prints all variables

    if(args.motorargs.is_valid){

        motors.slideMotor_p->verbose = args.verbose;
        std::cout << "Motor steps:" << args.motorargs.steps << std::endl;
        if(args.motorargs.home){
            motors.homeSlide();
        }
        if(args.motorargs.microstep.has_value()){
            //uint8_t mult = ( args.motorargs.microstep.value());

            TMC2209_motor::microstep ms = static_cast<TMC2209_motor::microstep>(args.motorargs.microstep.value());
            motors.slideMotor_p->setMicrostep(ms);
            std::cout << "Changing microstepping: " << (uint8_t)ms << std::endl;
        }
        if(args.motorargs.steps){
            motors.slideMotor_p->setEnable(true);
            motors.slideMotor_p->move(args.motorargs.steps,args.motorargs.speed,args.motorargs.accel);
            motors.slideMotor_p->setEnable(false);
        }
    }
    if(args.imgargs.is_valid){

        LinearCamCtrl cam(motors);
        bool inited = cam.initCCD();
        std::cout << "Init?: "<< inited << std::endl;


        std::cout << "State: "<< cam.readData(0x61) << std::endl;
        uint32_t period = cam.setResolution(CCD_Resolution::Div4);
        cam.setRgbGainOfs(0x7D,0x7D,0x7D,0x3D,0x32,0x38);
        FILE *fptr;
        fptr = fopen(args.imgargs.output.c_str(), "wb");

        cam.startCapture(); // Start CCD ouputting data

        for(int line = 0;line < args.imgargs.lines;line++){
            auto data = cam.captureLine();
            fwrite(data.data(),1 , data.size(), fptr);

        }
        //getchar();
        fclose(fptr);
        cam.endCapture();
    }

    pinctrl.setEnablePin(false);
    return 0;
}
