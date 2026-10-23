#include <iostream>

#include "argparse.hpp"
#include "tmc2209_motor.h"
#include "constants_pi.h"

#include "cameracontrol.h"
#include "ccd_ctrl.h"
#include "string.h"
#include "imagehelpers.hpp"
#include <fstream>
#include <iterator>
#include <cmath>
#include <algorithm>

// Commandline test utility
struct ImgArgs : public argparse::Args {
    bool &nomove             = flag("s,static capture", "Do not move stepper");
    uint32_t &lines        = kwarg("l,lines", "Capture l lines").set_default(1);
    std::string &output    = arg("out,output file", "Image output or \"-\" for stdio").set_default("img.png");
    unsigned int &resdiv          = kwarg("r,res", "Resolution divider. 0-4 (Div0 to Div16). Default 3 = div8").set_default(3);
    bool &raw             = flag("d,raw", "Save as raw data instead of bitmap");
    int &mm = kwarg("m", "Move stepper distance mm").set_default(0);
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

struct TimingArgs : public argparse::Args {
    bool &nomove             = flag("s,static", "Do not move stepper");
    uint32_t &lines        = kwarg("l,lines", "Capture l lines").set_default(300);
    std::string &output    = kwarg("o,output", "Image file. .png encodes during the capture, other extensions write raw").set_default("/tmp/timing.raw");
    unsigned int &resdiv          = kwarg("r,res", "Resolution divider. 0-4 (Div0 to Div16). Default 3 = div8").set_default(3);
    int &delay             = kwarg("e,delay", "Extra sleep per line in us (current UI exposure)").set_default(0);
    int &compression       = kwarg("z,compress", "PNG compression level for .png output").set_default(0);
    int &period            = kwarg("p,period", "Fixed line period in us (0 = off). Must exceed readout + processing").set_default(0);
    std::string &csv       = kwarg("c,csv", "Write per line timestamps to a csv file").set_default("");
    int &mm = kwarg("m", "Home, then move to position in mm before capturing. Image center is at 53").set_default(0);
};

struct MiscArgs : public argparse::Args {
    std::string &input    = arg("in,input file", "Image input");
    std::string &output    = arg("out,output file", "Image output").set_default("out.png");
};




struct MyArgs : public argparse::Args {

    bool &verbose          = flag("v,verbose", "A flag to toggle verbose");

    ImgArgs &imgargs    = subcommand("img");
    StepperArgs &motorargs    = subcommand("mot");
    TimingArgs &timingargs    = subcommand("timing");
    MiscArgs &miscargs = subcommand("test");
};



// Prints min/mean/max/std/p99 of a list of durations in us
static void printStats(const char* name,std::vector<double> v){
    if(v.empty()){
        std::cout << name << ": no data" << std::endl;
        return;
    }
    double sum = 0;
    for(double x : v) sum += x;
    const double mean = sum / v.size();
    double var = 0;
    for(double x : v) var += (x-mean)*(x-mean);
    std::sort(v.begin(),v.end());
    const double p99 = v[std::min<size_t>(v.size()-1,(size_t)(v.size()*0.99))];
    printf("%-14s n=%5zu  min %9.1f  mean %9.1f  max %9.1f  std %8.1f  p99 %9.1f us\n",name,v.size(),v.front(),mean,v.back(),std::sqrt(var/v.size()),p99);
}

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
        CCD_Ctrl ccd;
        LinearCamCtrl cam(motors,ccd);
        pinctrl.setEnablePin(false);
        sleep_ms(100);
        pinctrl.setEnablePin(true);

        cam.ccd.setClock(true);
        sleep_ms(500);
        bool inited = cam.ccd.initCCD();
        std::cout << "Init?: "<< inited << std::endl;

        if(args.imgargs.mm){
            motors.homeSlide();
            motors.slideMotor_p->moveMM(args.imgargs.mm,cam.moveSpeed,cam.moveAccel);
        }


        float curPos = motors.slideMotor_p->getActualPos();
        sleep_ms(100);
        std::cout << "State: "<< cam.ccd.readReg(0x61) << std::endl;
        unsigned int respres = std::min(4u,args.imgargs.resdiv);
        CCD_Resolution res = static_cast<CCD_Resolution>(respres);
        cam.ccd.setResolution(res);
        CCD_Parameters params = cam.ccd.getParamsFromResolution(res);
        std::cout << "Respreset: "<< respres << " move " << params.moveLines << std::endl;
        uint32_t period = params.samplesPerLine / SAMPLESPERBUFFER;

        cam.ccd.setRgbGainOfs({0x7E,0x77,0x7E,0x38,0x32,0x38});

        if(args.imgargs.raw){
            FILE *fptr;
            fptr = fopen(args.imgargs.output.c_str(), "wb");

            cam.startCapture(); // Start CCD ouputting data
            //cam.captureFrame(args.imgargs.lines,fptr,period);

            for(int line = 0;line < args.imgargs.lines;line++){
                cam.ccd.captureFrame(1,fptr,period);
                cam.moveLines(params.moveLines);
            }

            fclose(fptr);
            cam.endCapture();
        }else{

            // Must set resolution and gains before
            cam.captureImage(args.imgargs.output,args.imgargs.lines);
        }
        std::cout << "Captured lines: "<< args.imgargs.lines << std::endl;
        motors.slideMotor_p->moveToMM(curPos,cam.moveSpeed,cam.moveAccel);
    }
    if(args.timingargs.is_valid){
        // Measures the line timing of a real capture. Exposure of a line = time between two TG pulses
        TimingArgs& t = args.timingargs;
        CCD_Ctrl ccd;
        LinearCamCtrl cam(motors,ccd);
        pinctrl.setEnablePin(false);
        sleep_ms(100);
        pinctrl.setEnablePin(true);
        cam.ccd.setClock(true);
        sleep_ms(500);
        if(!cam.ccd.initCCD()){
            std::cout << "CCD init failed" << std::endl;
            return 1;
        }
        if(t.mm){
            motors.homeSlide();
            motors.slideMotor_p->setEnable(true);
            motors.slideMotor_p->moveToMM(t.mm,cam.moveSpeed,cam.moveAccel);
        }
        const float startPos = motors.slideMotor_p->getActualPos();

        CCD_Resolution res = static_cast<CCD_Resolution>(std::min(4u,t.resdiv));
        cam.ccd.setResolution(res);
        cam.ccd.setRgbGainOfs({0x7E,0x77,0x7E,0x38,0x32,0x38});
        CCD_Parameters params = cam.ccd.getCurParams();
        std::cout << "Res " << t.resdiv << " samples/line " << params.samplesPerLine << " movelines " << params.moveLines
                  << " lines " << t.lines << " delay " << t.delay << "us period " << t.period << "us out " << t.output << std::endl;

        cam.linePeriodUs = t.period;
        cam.recordTiming = true;
        cam.captureImage(t.output,t.lines,t.delay,std::nullopt,t.nomove);
        cam.recordTiming = false;

        const std::vector<LineTiming>& lt = cam.lineTimings;
        std::vector<double> tg,read,move;
        for(size_t i = 0;i<lt.size();i++){
            read.push_back((lt[i].readEnd - lt[i].readStart)/1000.0);
            if(!t.nomove){
                move.push_back((lt[i].moveEnd - lt[i].moveStart)/1000.0);
            }
            if(i > 0){
                tg.push_back((lt[i].readStart - lt[i-1].readStart)/1000.0);
            }
        }
        std::cout << "Lines timed: " << lt.size() << std::endl;
        printStats("TG interval",tg);
        printStats("Readout",read);
        printStats("Motor move",move);

        if(!t.csv.empty()){
            std::ofstream csv(t.csv);
            csv << "line,readStart_ns,readEnd_ns,moveStart_ns,moveEnd_ns\n";
            for(size_t i = 0;i<lt.size();i++){
                csv << i << ',' << lt[i].readStart << ',' << lt[i].readEnd << ',' << lt[i].moveStart << ',' << lt[i].moveEnd << '\n';
            }
        }
        if(!t.nomove){
            motors.slideMotor_p->setEnable(true);
            motors.slideMotor_p->moveToMM(startPos,cam.moveSpeed,cam.moveAccel);
            motors.slideMotor_p->setEnable(false);
        }
    }
    if(args.miscargs.is_valid && !args.miscargs.input.empty() && !args.miscargs.output.empty()){

        auto progresscb = [](float val){
            std::cout << "\r" <<  std::to_string(val * 100.0f) << "%       " << std::flush;
        };

        std::cout << "Imgtest: "<< args.miscargs.input << " out: " <<  args.miscargs.output << std::endl;
        FILE* infile = fopen(args.miscargs.input.c_str(),"rb");
        FILE* outfile = fopen((args.miscargs.output).c_str(),"wb");
        png::pngReencode(infile,outfile,1,5,SPNG_FMT_RGB8,true,progresscb);
        fclose(infile);
        fclose(outfile);
        std::cout << "Done" << std::endl;
    }


    pinctrl.setEnablePin(false);
    return 0;
}
