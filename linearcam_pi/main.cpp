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
#include <array>
#include <chrono>
#include <map>
#include <cstdio>

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
    int &period            = kwarg("p,period", "Line period = exposure in us, 0 = shortest. Raised to the measured minimum").set_default(0);
    bool &freerun          = flag("free", "Start lines as fast as possible (old behaviour, exposure varies)");
    std::string &csv       = kwarg("c,csv", "Write per line timestamps to a csv file").set_default("");
    int &mm = kwarg("m", "Home, then move to position in mm before capturing. Image center is at 53").set_default(0);
    float &freq            = kwarg("f,freq", "CCD clock in Hz, 0 = CCDCLOCK").set_default(0.0f);
    float &dataclk         = kwarg("y,dataclk", "Data SM clock in Hz, 0 = 18 x CCD clock, default PIODATACLOCK").set_default(-1.0f);
    int &sampledelay       = kwarg("t,sampledelay", "Data SM cycles from clock edge to sample, -1 = as assembled, default PIOSAMPLEDELAY").set_default(-100);
    int &tgdelay           = kwarg("g,tgdelay", "Data SM cycles from clock edges to TG edges, -1 = as assembled, default PIOTGDELAY").set_default(-100);
};

struct PioArgs : public argparse::Args {
    uint32_t &lines        = kwarg("l,lines", "Capture l lines").set_default(20);
    unsigned int &resdiv   = kwarg("r,res", "Resolution divider. 0-4 (Div0 to Div16). Default 4 = preview").set_default(4);
    std::string &output    = kwarg("o,output", "Dump raw 32 bit words of all lines to a file").set_default("");
    std::string &dmactrl   = kwarg("d,dmactrl", "Override RX DMACTRL after setup, e.g. 0x80000108").set_default("");
    uint32_t &skip         = kwarg("k,skip", "Lines skipped by the lane statistics, the first lines after start are not settled").set_default(5);
    int &offset            = kwarg("O,offset", "AFE offset for all channels (rofs/gofs/bofs), -1 = 0x7E/0x77/0x7E").set_default(-1);
    float &freq            = kwarg("f,freq", "CCD clock in Hz, 0 = CCDCLOCK").set_default(0.0f);
    float &dataclk         = kwarg("y,dataclk", "Data SM clock in Hz, 0 = 18 x CCD clock, default PIODATACLOCK").set_default(-1.0f);
    int &delay             = kwarg("t,delay", "Data SM cycles from clock edge to sample, -1 = as assembled, default PIOSAMPLEDELAY").set_default(-100);
    int &tgdelay           = kwarg("g,tgdelay", "Data SM cycles from clock edges to TG edges, -1 = as assembled, default PIOTGDELAY").set_default(-100);
};

struct CalibArgs : public argparse::Args {
    uint32_t &lines        = kwarg("l,lines", "Lines per exposure").set_default(60);
    int &exposure          = kwarg("e,exposure", "First exposure in us, 0 = base exposure of each resolution").set_default(0);
    float &ratio           = kwarg("r,ratio", "Second exposure = ratio x first").set_default(1.6f);
    int &mm                = kwarg("m", "Home, then move to position in mm. Image center is at 53").set_default(0);
};

struct GammaArgs : public argparse::Args {
    std::string &input    = arg("in,input file", "16 bit png with gAMA chunk");
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
    GammaArgs &gammaargs    = subcommand("gamma");
    CalibArgs &calibargs    = subcommand("expcalib");
    PioArgs &pioargs    = subcommand("pio");
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
        if(t.freq > 0) CCD_Ctrl::ccdClockHz = t.freq;
        if(t.dataclk >= 0) CCD_Ctrl::dataClockHz = t.dataclk;
        if(t.sampledelay > -100) CCD_Ctrl::sampleDelay = t.sampledelay;
        if(t.tgdelay > -100) CCD_Ctrl::tgDelay = t.tgdelay;
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

        cam.pngCompressionLevel = t.compression;
        cam.linePeriodUs = t.period;
        cam.fixedLinePeriod = !t.freerun;
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
        const LinearCamCtrl::ExposureStats& es = cam.exposureStats;
        printf("Exposure: period %d us (requested %d, shortest %d), %u fixed lines, %u late, max late %d us\n",
               es.periodUs,es.requestedUs,es.minPeriodUs,es.lines,es.lateLines,es.maxLateUs);

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
    if(args.pioargs.is_valid){
        // Raw readout diagnostics. Checks the 32 marker words, zero/repeated words, byte lane alignment and readout time
        PioArgs& t = args.pioargs;
        if(t.freq > 0) CCD_Ctrl::ccdClockHz = t.freq;
        if(t.dataclk >= 0) CCD_Ctrl::dataClockHz = t.dataclk;
        if(t.delay > -100) CCD_Ctrl::sampleDelay = t.delay;
        if(t.tgdelay > -100) CCD_Ctrl::tgDelay = t.tgdelay;
        CCD_Ctrl ccd;
        LinearCamCtrl cam(motors,ccd);
        pinctrl.setEnablePin(false);
        sleep_ms(100);
        pinctrl.setEnablePin(true);
        ccd.setClock(true);
        sleep_ms(500);
        if(!ccd.initCCD()){
            std::cout << "CCD init failed" << std::endl;
            return 1;
        }
        CCD_Resolution res = static_cast<CCD_Resolution>(std::min(4u,t.resdiv));
        ccd.setResolution(res);
        if(t.offset >= 0){
            const uint8_t o = t.offset;
            ccd.setRgbGainOfs({o,o,o,0x38,0x32,0x38});
        }else{
            ccd.setRgbGainOfs({0x7E,0x77,0x7E,0x38,0x32,0x38});
        }
        const CCD_Parameters params = ccd.getCurParams();
        const uint32_t words = params.samplesPerLine / SAMPLESPERBUFFER;
        std::cout << "CCD clock " << CCD_Ctrl::ccdClockHz << " Hz, data SM " << (CCD_Ctrl::dataClockHz > 0 ? CCD_Ctrl::dataClockHz : CCD_Ctrl::ccdClockHz*18.0f) << " Hz, sample delay " << CCD_Ctrl::sampleDelay << ", TG delay " << CCD_Ctrl::tgDelay << std::endl;
        std::cout << "clk_sys " << CCD_Ctrl::sysClockHz() << " Hz, res " << t.resdiv << ", " << words << " words/line, lines " << t.lines << std::endl;
        if(!t.dmactrl.empty()){
            const uint32_t ctrl = std::stoul(t.dmactrl,nullptr,0);
            printf("Setting RX DMACTRL 0x%08x\n",ctrl);
            ccd.setRxDmaCtrl(ctrl);
        }
        printf("RX DMACTRL 0x%08x (low bits: FIFO DREQ threshold)\n",ccd.getRxDmaCtrl());

        std::vector<uint32_t> dump((size_t)words*t.lines,0xDEADBEEF);
        std::vector<double> readus;
        ccd.startCapture();
        for(uint32_t l = 0;l<t.lines;l++){
            const auto t0 = std::chrono::steady_clock::now();
            ccd.captureData(dump.data() + (size_t)l*words,words,words);
            readus.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now() - t0).count());
        }
        ccd.endCapture();
        ccd.setClock(false);

        if(!t.output.empty()){
            FILE* f = fopen(t.output.c_str(),"wb");
            if(f){
                fwrite(dump.data(),sizeof(uint32_t),dump.size(),f);
                fclose(f);
            }
        }

        const uint32_t markers = 32;
        const uint32_t lag = res == CCD_Resolution::Full ? 6 : 3; // Words between two samples of the same color/row
        const uint32_t skip = std::min<uint32_t>(t.skip,t.lines-1); // First lines after start may contain stale charge
        uint32_t markerOk = 0, untouched = 0, zeros = 0, maxRun = 0;
        double lanesq[4] = {0,0,0,0};
        double lanesum[4] = {0,0,0,0};
        uint64_t lanen = 0;
        for(uint32_t l = 0;l<t.lines;l++){
            const uint32_t* w = dump.data() + (size_t)l*words;
            bool ok = true;
            for(uint32_t i = 0;i<markers;i++){
                if(w[i] != markers-1-i) ok = false;
            }
            markerOk += ok;
            uint32_t run = 1;
            for(uint32_t i = markers;i<words;i++){
                untouched += w[i] == 0xDEADBEEF;
                zeros += w[i] == 0;
                run = (w[i] == w[i-1]) ? run+1 : 1;
                maxRun = std::max(maxRun,run);
                if(l >= skip && i+lag < words){
                    for(int b = 0;b<4;b++){
                        const double d = (double)((w[i] >> (8*b)) & 0xff) - (double)((w[i+lag] >> (8*b)) & 0xff);
                        lanesq[b] += d*d;
                        lanesum[b] += (w[i] >> (8*b)) & 0xff;
                    }
                    lanen++;
                }
            }
        }
        printf("Markers ok: %u/%u lines\n",markerOk,t.lines);
        printf("Data words: zero %u, never written %u, longest run of equal words %u\n",zeros,untouched,maxRun);
        printf("Byte lane (3=first sample) mean / rms diff to the same color %u words later. MSB lanes 3 and 1 should be smooth:\n",lag);
        for(int b = 3;b>=0;b--){
            printf("  lane %d: mean %6.1f  rms diff %6.2f\n",b,lanesum[b]/std::max<uint64_t>(1,lanen),std::sqrt(lanesq[b]/std::max<uint64_t>(1,lanen)));
        }
        printStats("Readout",readus);
        printf("First words of line %u:",skip);
        for(uint32_t i = 0;i<48 && i<words;i++){
            printf("%s%08x",i % 8 ? " " : "\n  ",dump[(size_t)skip*words+i]);
        }
        printf("\n");
    }
    if(args.calibargs.is_valid){
        // Exposure calibration of every resolution on a static scene: readout vs timing model,
        // black level per channel and sensitivity relative to full resolution.
        // Two exposures T and r*T of the same pixels: s2 + b = r * (s1 + b)  ->  b = (s2 - r*s1) / (r - 1)
        CalibArgs& t = args.calibargs;
        CCD_Ctrl ccd;
        LinearCamCtrl cam(motors,ccd);
        pinctrl.setEnablePin(false);
        sleep_ms(100);
        pinctrl.setEnablePin(true);
        ccd.setClock(true);
        sleep_ms(500);
        if(!ccd.initCCD()){
            std::cout << "CCD init failed" << std::endl;
            return 1;
        }
        if(t.mm){
            motors.homeSlide();
            motors.slideMotor_p->setEnable(true);
            motors.slideMotor_p->moveToMM(t.mm,cam.moveSpeed,cam.moveAccel);
        }
        const float ratio = t.ratio;
        struct Result{ float readout, model, base, minPeriod, sens; std::array<float,3> black; double fullScale; };
        std::map<int,Result> results;
        const char* tmpfile = "/tmp/expcalib.raw";
        for(int r = 4;r >= 0;r--){
            CCD_Resolution res = static_cast<CCD_Resolution>(r);
            ccd.setResolution(res);
            ccd.setRgbGainOfs({0x7E,0x77,0x7E,0x38,0x32,0x38});
            const CCD_Parameters p = ccd.getCurParams();
            const uint32_t width = p.validPixelEnd - p.validPixelBegin;
            const float base = cam.baseExposureUs(p);
            const int t1 = t.exposure > 0 ? t.exposure : (int)std::ceil(base/1000.0f)*1000;
            const int t2 = t1 * ratio;
            std::array<std::vector<double>,2> mean;
            float readout = 0, minPeriod = 0;
            for(int e = 0;e < 2;e++){
                cam.linePeriodUs = e == 0 ? t1 : t2;
                cam.recordTiming = true;
                cam.captureImage(tmpfile,t.lines,0,std::nullopt,true);
                cam.recordTiming = false;
                if(e == 0){
                    double sum = 0;
                    size_t n = 0;
                    for(size_t i = LinearCamCtrl::periodCalibLines;i<cam.lineTimings.size();i++){
                        sum += (cam.lineTimings[i].readEnd - cam.lineTimings[i].readStart) / 1000.0;
                        n++;
                    }
                    readout = n ? sum / n : 0;
                    minPeriod = cam.exposureStats.minPeriodUs;
                }
                // Mean of every sample over all lines
                std::ifstream f(tmpfile,std::ios::binary);
                std::vector<uint16_t> line(width*3);
                mean[e].assign(width*3,0.0);
                uint32_t lines = 0;
                while(f.read(reinterpret_cast<char*>(line.data()),line.size()*2)){
                    for(size_t i = 0;i<line.size();i++) mean[e][i] += line[i];
                    lines++;
                }
                for(double& v : mean[e]) v /= std::max<uint32_t>(1,lines);
            }
            std::remove(tmpfile);
            Result res_{readout,CCD_Ctrl::readoutUs(p),base,minPeriod,0,{0,0,0},0};
            double signal = 0;
            size_t used = 0;
            for(int c = 0;c < 3;c++){
                std::vector<double> b;
                for(size_t i = c;i<mean[0].size();i += 3){
                    if(mean[0][i] > 300 && mean[1][i] < 60000){
                        b.push_back((mean[1][i] - ratio*mean[0][i]) / (ratio - 1.0));
                    }
                }
                std::sort(b.begin(),b.end());
                res_.black[c] = b.empty() ? 0 : b[b.size()/2];
                for(size_t i = c;i<mean[0].size();i += 3){
                    if(mean[1][i] < 60000){
                        signal += (mean[0][i] + res_.black[c]) / t1;
                        used++;
                    }
                }
            }
            res_.fullScale = used ? signal / used : 0; // counts per us including the black level
            results[r] = res_;
        }
        const char* names[] = {"Full","Div2","Div4","Div8","Preview"};
        printf("CCD clock %.0f Hz, exposure ratio %.2f\n",CCD_Ctrl::ccdClockHz,ratio);
        printf("res      readout   model    meas/model  base exp  min period  sensitivity  black R/G/B\n");
        for(int r = 4;r >= 0;r--){
            Result& x = results[r];
            x.sens = results[0].fullScale > 0 ? x.fullScale / results[0].fullScale : 0;
            printf("%-8s %8.1f %8.1f   %6.3f    %8.0f  %8.0f     %6.3f      %5.0f %5.0f %5.0f\n",names[r],x.readout,x.model,x.readout/x.model,x.base,x.minPeriod,x.sens,x.black[0],x.black[1],x.black[2]);
        }
        printf("Presets (getParamsFromResolution):\n");
        for(int r = 4;r >= 0;r--){
            Result& x = results[r];
            printf("  %-8s .sensitivity = %.3ff\n",names[r],x.sens);
        }
    }
    if(args.gammaargs.is_valid){
        // Compares the spng gamma decode (old low res path) with the 16 bit display LUT (new path)
        FILE* f = fopen(args.gammaargs.input.c_str(),"rb");
        if(!f){
            std::cout << "Cannot open " << args.gammaargs.input << std::endl;
            return 1;
        }
        const double gamma = png::getPngGamma(f);
        std::cout << "File gamma: " << gamma << std::endl;
        if(gamma <= 0){
            fclose(f);
            return 1;
        }
        auto decode = [&](spng_format fmt,int flags,std::vector<uint8_t>& out){
            rewind(f);
            spng_ctx* ctx = spng_ctx_new(0);
            spng_set_png_file(ctx,f);
            spng_set_chunk_limits(ctx,1024*1024*64,1024*1024*64);
            size_t len = 0;
            spng_decoded_image_size(ctx,fmt,&len);
            out.resize(len);
            int res = spng_decode_image(ctx,out.data(),len,fmt,flags);
            spng_ihdr ihdr;
            spng_get_ihdr(ctx,&ihdr);
            spng_ctx_free(ctx);
            if(res != SPNG_OK) std::cout << "Decode error " << spng_strerror(res) << std::endl;
            return ihdr;
        };
        std::vector<uint8_t> old8,raw16;
        spng_ihdr ihdr = decode(SPNG_FMT_RGBA8,SPNG_DECODE_GAMMA,old8);
        decode(SPNG_FMT_RGBA16,0,raw16);
        fclose(f);
        const std::vector<uint8_t> lut = png::gammaLut16to8(gamma);
        const uint16_t* px16 = reinterpret_cast<const uint16_t*>(raw16.data());
        const size_t samples = (size_t)ihdr.width*ihdr.height*4;
        // Difference per output level of the new curve. Shadows differ because spng cuts to 8 bit before the curve
        int maxdiff = 0;
        size_t count = 0,diffcount = 0;
        double sumdiff = 0;
        std::array<int,256> maxdiffAt{};
        for(size_t i = 0;i<samples;i++){
            if(i % 4 == 3) continue; // Alpha
            const int n = lut[px16[i]];
            const int d = std::abs(n - (int)old8[i]);
            maxdiff = std::max(maxdiff,d);
            maxdiffAt[n] = std::max(maxdiffAt[n],d);
            sumdiff += d;
            if(d > 1) diffcount++;
            count++;
        }
        printf("%ux%u: mean |new-old| %.3f, max %d, samples off by >1: %.3f%%\n",ihdr.width,ihdr.height,sumdiff/count,maxdiff,100.0*diffcount/count);
        for(int lvl = 0;lvl<256;lvl+=32){
            int m = 0;
            for(int i = lvl;i<lvl+32;i++) m = std::max(m,maxdiffAt[i]);
            printf("  new value %3d-%3d: max diff %d\n",lvl,lvl+31,m);
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
