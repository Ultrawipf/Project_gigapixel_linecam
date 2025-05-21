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
    pinctrl.setEnablePin(true);

    if (args.verbose)
        args.print();      // prints all variables

    if(args.motorargs.is_valid){
        Motors motors;
        motors.slideMotor_p->verbose = args.verbose;
        std::cout << "Motor steps:" << args.motorargs.steps << std::endl;
        if(args.motorargs.home){
            motors.homeSlide();
        }
        if(args.motorargs.steps){
            motors.slideMotor_p->setEnable(true);
            motors.slideMotor_p->move(args.motorargs.steps,args.motorargs.speed,args.motorargs.accel);
            motors.slideMotor_p->setEnable(false);
        }
    }
    if(args.imgargs.is_valid){
        CCD_Ctrl ccd;
        ccd.setClock(true);
        bool inited = ccd.initCCD();

        std::cout << "Init?: "<< inited << std::endl;

        std::cout << "State: "<< ccd.readData(0x61) << std::endl;
        uint32_t period = ccd.setResolution(CCD_Resolution::Div8);
        ccd.setRgbGainOfs(0x7D,0x7D,0x7D,0x3D,0x32,0x38);
        ccd.captureFrame(args.imgargs.lines, args.imgargs.output,period);
        //getchar();
        ccd.setClock(false);
    }

    pinctrl.setEnablePin(false);
    return 0;
}
