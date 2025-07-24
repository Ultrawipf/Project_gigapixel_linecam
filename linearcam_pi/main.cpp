#include <iostream>

#include "argparse.hpp"
#include "tmc2209_motor.h"
#include "constants_pi.h"

#include "cameracontrol.h"
#include "ccd_ctrl.h"
#include "string.h"
#include "bitmap.hpp"
#include <fstream>
#include <iterator>

// Commandline test utility
struct ImgArgs : public argparse::Args {
    bool &nomove             = flag("s,static capture", "Do not move stepper");
    uint32_t &lines        = kwarg("l,lines", "Capture l lines").set_default(1);
    std::string &output    = arg("out,output file", "Image output or \"-\" for stdio").set_default("-");
    unsigned int &resdiv          = kwarg("r,res", "Resolution divider. 0-4 (Div0 to Div16). Default 3 = div8").set_default(3);
    bool &raw             = flag("d,raw", "Save as raw data instead of bitmap");
};

struct StepperArgs : public argparse::Args {
    bool &home             = flag("h,home", "Home stepper");
    int &steps             = kwarg("s", "Move stepper s steps").set_default(0);
    int &mm                = kwarg("m", "Move stepper distance mm").set_default(0);
    int &pos                = kwarg("p", "Move stepper to position in mm").set_default(0);
    std::optional<uint8_t> &microstep     = kwarg("ms", "Microstep multiplier options: ms256 = 0,ms128 = 1,ms64 = 2, ms32=3,ms16=4,ms8=5,ms4=6,ms2=7,fullstep=8");
    int &speed             = kwarg("v", "Speed v").set_default(500);
    int &accel             = kwarg("a", "Accel a").set_default(20);
    std::string& serial = kwarg("port","serial port, default /dev/ttyAMA0").set_default("/dev/ttyAMA0");
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


    if (args.verbose)
        args.print();      // prints all variables

    if(args.motorargs.is_valid){
        pinctrl.setEnablePin(true);
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
        if(args.motorargs.mm){
            motors.slideMotor_p->setEnable(true);
            motors.slideMotor_p->moveMM(args.motorargs.mm,args.motorargs.speed,args.motorargs.accel);
            motors.slideMotor_p->setEnable(false);
        }
        if(args.motorargs.pos){
            motors.slideMotor_p->setEnable(true);
            if(!motors.slideMotor_p->wasHomed())
                motors.homeSlide();
            motors.slideMotor_p->moveToMM(args.motorargs.pos,args.motorargs.speed,args.motorargs.accel);
            motors.slideMotor_p->setEnable(false);
        }
    }
    if(args.imgargs.is_valid){

        LinearCamCtrl cam(motors);
        pinctrl.setEnablePin(false);
        sleep_ms(100);
        pinctrl.setEnablePin(true);

        cam.setClock(true);
        sleep_ms(500);
        bool inited = cam.initCCD();
        std::cout << "Init?: "<< inited << std::endl;



        sleep_ms(100);
        std::cout << "State: "<< cam.readReg(0x61) << std::endl;
        unsigned int respres = std::min(4u,args.imgargs.resdiv);
        CCD_Resolution res = static_cast<CCD_Resolution>(respres);
        cam.setResolution(res);
        CCD_Parameters params = cam.getParamsFromResolution(res);
        std::cout << "Respreset: "<< respres << " move " << params.moveLines << std::endl;
        uint32_t period = params.samplesPerLine / SAMPLESPERBUFFER;

        cam.setRgbGainOfs(0x7E,0x77,0x7E,0x38,0x32,0x38);

        if(args.imgargs.raw){
            FILE *fptr;
            fptr = fopen(args.imgargs.output.c_str(), "wb");

            cam.startCapture(); // Start CCD ouputting data
            //cam.captureFrame(args.imgargs.lines,fptr,period);

            for(int line = 0;line < args.imgargs.lines;line++){
                cam.captureFrame(1,fptr,period);
                cam.moveLines(params.moveLines);
            }

            fclose(fptr);
            cam.endCapture();
        }else{

            // Must set resolution and gains before
            cam.captureImage(args.imgargs.output,args.imgargs.lines,0,true);
        }
        std::cout << "Captured lines: "<< args.imgargs.lines << std::endl;

    }

    pinctrl.setEnablePin(false);
    return 0;
}
