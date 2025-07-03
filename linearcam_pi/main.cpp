#include <iostream>

#include "argparse.hpp"
#include "tmc2209_motor.h"
#include "constants_pi.h"

#include "cameracontrol.h"
#include "ccd_ctrl.h"
#include "string.h"


// Commandline test utility
struct ImgArgs : public argparse::Args {
    bool &nomove             = flag("s,static capture", "Do not move stepper");
    uint32_t &lines        = kwarg("l,lines", "Capture l lines").set_default(1);
    std::string &output    = arg("out,output file", "Image output or \"-\" for stdio").set_default("-");
    unsigned int &resdiv          = kwarg("r,res", "Resolution divider. 0-4 (Div0 to Div16). Default 3 = div8").set_default(3);
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
        /*
        SM_def sm_data = {.pio = pio0,.sm = SMDEF_INVALID_VAL,.offset = SMDEF_INVALID_VAL};
        SM_def sm_clock = {.pio = pio0,.sm = SMDEF_INVALID_VAL,.offset = SMDEF_INVALID_VAL};

        const uint32_t samplenum = 122400/2;
        uint32_t buffer[samplenum];
        memset( buffer, 0, sizeof(buffer) );

        setup_clock(&sm_clock,6000000.0,ACLKPIN);
        setup_data(&sm_data,DATABASEPIN);
        pio_sm_clkdiv_restart(sm_clock.pio,sm_clock.sm);
        pio_sm_clkdiv_restart(sm_data.pio,sm_data.sm);

        sleep_ms(1000);


        // First sampling 2 TG periods? 20k values?

        set_clock(&sm_clock,1); // Enable clock
        sleep_ms(100);
        send_ctrl_data_arr(&sm_data,startcommands,sizeof(startcommands)/sizeof(uint32_t));
        printf("State/Ping %d\n",read_ctrl_data_sm(&sm_data,0x61));
        sleep_ms(100);
        send_ctrl_data_arr(&sm_data,startcommands2,sizeof(startcommands2)/sizeof(uint32_t));
        sleep_ms(100);
        // send_ctrl_data_arr(setupCmds_600dpi,sizeof(setupCmds_600dpi)/sizeof(uint32_t));
        // send_ctrl_data_arr(block2,sizeof(block2)/sizeof(uint32_t));
        send_ctrl_data_arr(&sm_data,startcommands3,sizeof(startcommands3)/sizeof(uint32_t));
        send_ctrl_data_arr(&sm_data,register_settings,sizeof(register_settings)/sizeof(uint32_t));
        send_ctrl_data_arr(&sm_data,startcommands4,sizeof(startcommands4)/sizeof(uint32_t));

        sleep_ms(100);
        uint32_t tgperiod = set_resolution(&sm_data,CCD_Res_div8)/2; // Mainlines only
        set_rgb_gain_ofs(&sm_data,0x7D,0x7D,0x7D,0x3D,0x32,0x38); // Test dumped gain
        sleep_ms(100);

        FILE *fptr;
        fptr = fopen(args.imgargs.output.c_str(), "wb");
        start_sampling(&sm_data);
        for(int line = 0;line < args.imgargs.lines;line++){
            memset( buffer, 0, sizeof(buffer) );
            readFrames(&sm_data,buffer,samplenum,tgperiod);
            fwrite(buffer, 4, samplenum, fptr);
        }
        stop_sampling(&sm_data);
        fclose(fptr);
*/


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
        uint32_t period = params.samplesPerLine / 3;

        cam.setRgbGainOfs(0x7D,0x7D,0x7D,0x3D,0x32,0x38);

        FILE *fptr;
        fptr = fopen(args.imgargs.output.c_str(), "wb");

        cam.startCapture(); // Start CCD ouputting data
        //cam.captureFrame(args.imgargs.lines,fptr,period);

        for(int line = 0;line < args.imgargs.lines;line++){
            //auto data = cam.captureLine();
            //fwrite(data.data(),1 , data.size(), fptr);
            cam.captureFrame(1,fptr,period);
            cam.moveLines(params.moveLines);
            //std::cout << "Dlen " << data.size() << std::endl;

        }

        //getchar();
        fclose(fptr);
        cam.endCapture();

    }

    pinctrl.setEnablePin(false);
    return 0;
}
