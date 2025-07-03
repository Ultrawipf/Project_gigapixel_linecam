#include "ccd_ctrl.h"
#include <array>


#include "cameracontrol.h"
#ifndef DUMMY
#include <wiringPi.h>
#else
#include "wiringpi_dummy/wiringPi.h"
#endif
#include <iostream>

#ifndef DUMMY
//SM_def sm_data = {.pio = pio0,.offset = SMDEF_INVALID_VAL,.sm = SMDEF_INVALID_VAL};
//SM_def sm_clock = {.pio = pio0,.offset = SMDEF_INVALID_VAL,.sm = SMDEF_INVALID_VAL};
// SM_def sm_stepper = {.pio = pio0,.offset = SMDEF_INVALID_VAL,.sm = SMDEF_INVALID_VAL};

CCD_Ctrl::CCD_Ctrl()
    : sm_data({.pio = pio0,.sm = SMDEF_INVALID_VAL,.offset = SMDEF_INVALID_VAL})
    ,sm_clock({.pio = pio0,.sm = SMDEF_INVALID_VAL,.offset = SMDEF_INVALID_VAL})
{
    wiringPiSetupGpio();
    // Setup CS pin
    pinMode(CSPIN, OUTPUT);
    digitalWrite(CSPIN, true); // CS idle high

    pinMode(TGPIN, OUTPUT);
    digitalWrite(TGPIN, false); // TG idle low

#ifdef LEDPIN
    pinMode(LEDPIN, OUTPUT);
    digitalWrite(LEDPIN, false); // LED off
#endif

    setupSm();
}
CCD_Ctrl::~CCD_Ctrl()
{
    setClock(false);
}

void CCD_Ctrl::setupSm(){
    setup_clock(&sm_clock,6000000.0,ACLKPIN);
    setup_data(&sm_data,DATABASEPIN);
    pio_sm_clkdiv_restart(sm_clock.pio,sm_clock.sm);
    pio_sm_clkdiv_restart(sm_data.pio,sm_data.sm);
}

void CCD_Ctrl::setClock(bool enable){
    std::cout << "CCD clock " << enable << std::endl;
    set_clock(&sm_clock,enable);
}

void CCD_Ctrl::startCapture(){
    start_sampling(&sm_data);


}

void CCD_Ctrl::captureData(uint32_t* buffer,uint32_t length,uint32_t tgperiod){
    readFrames(&sm_data,buffer,length,tgperiod);
}

void CCD_Ctrl::endCapture(){
    stop_sampling(&sm_data);
}


void CCD_Ctrl::sendCtrlData(uint8_t cmd,uint8_t data1,uint8_t data2){
    send_ctrl_data_sm(&sm_data,cmd,data1,data2);
}

void CCD_Ctrl::sendCtrlData(uint32_t data){
    send_ctrl_data_sm_32(&sm_data,data);
}

void CCD_Ctrl::sendCtrlDataArr(const uint32_t* buf,uint32_t len){
    send_ctrl_data_arr(&sm_data,buf,len);
}

void CCD_Ctrl::captureFrame(uint32_t lines,std::string& filename,uint32_t tgperiod){
    FILE *fptr;
    if(filename == "-"){
        fptr = stdout;
    }else{
        fptr = fopen(filename.c_str(), "ab");
    }
    captureFrame(lines,fptr,tgperiod);
    fclose(fptr);
}

void CCD_Ctrl::captureFrame(uint32_t lines,FILE *fptr,uint32_t tgperiod){
    uint32_t offset = 2;
    const uint32_t samplesPerBuffer = 3;
    int samples = tgperiod;
    uint32_t buffer[samples] = {0};

    //start_sampling(&sm_data);
    //sleep_ms(50); // Dump some data first
    //std::cout << "Samples " << samples << " TG "  << tgperiod << std::endl;
   // bool tgfound = false;


    for(uint32_t l = 0;l<lines;l++){
        readFrames(&sm_data,buffer,tgperiod,tgperiod);
        // fwrite((uint32_t*)buffer, 3, samples, fptr);
        const uint8_t zero = 0;
        fwrite(&zero, 1, offset, fptr);
        for(uint32_t i = 0;i<tgperiod;i++){
            uint32_t pxl = ((buffer[i] & 0xff0000) >> 16 ) | ((buffer[i] & 0xff00)) |( (buffer[i] & 0xff) << 16); // BGR to RGB
            fwrite(&pxl, samplesPerBuffer, 1, fptr);
        }
        fwrite(&zero, 1, samplesPerBuffer-offset, fptr);
    }
    // Close the file
    
    //stop_sampling(&sm_data);

}

void CCD_Ctrl::setResolution(CCD_Resolution res){
    curRes = res;
    set_resolution(&sm_data,static_cast<CCD_Resolution_t>(res));
    curParams = getParamsFromResolution(res);
}

CCD_Parameters CCD_Ctrl::getParamsFromResolution(CCD_Resolution res){
    switch(res){
    case CCD_Resolution::Full: // Main+Sub x2
        return CCD_Parameters{
            .validPixelBegin = 333,
            .validPixelEnd = 40800,
            .samplesPerLine = 130548,
            .bytesPerColor = 1,
            .moveLines = 1,
            .colorOffset = 0
        };
        break;
    case CCD_Resolution::Div2: // Main+Sub x1
        return CCD_Parameters{
            .validPixelBegin = 333,
            .validPixelEnd = 40800/2,
            .samplesPerLine = 65280,
            .bytesPerColor = 1,
            .moveLines = 2,
            .colorOffset = 0
        };
        break;
    case CCD_Resolution::Div4: // Main only
        return CCD_Parameters{
            .validPixelBegin = 333,
            .validPixelEnd = 40800/4,
            .samplesPerLine = 32640,
            .bytesPerColor = 1,
            .moveLines = 4,
            .colorOffset = 20
        };
        break;
    case CCD_Resolution::Div8: // Main w. skip
        return CCD_Parameters{
            .validPixelBegin = 333,
            .validPixelEnd = 40800/8,
            .samplesPerLine = 27648,
            .bytesPerColor = 1,
            .moveLines = 8,
            .colorOffset = 10
        };
        break;
    case CCD_Resolution::Div16: // Mainw. skip
        return CCD_Parameters{
            .validPixelBegin = 333,
            .validPixelEnd = 40800/16,
            .samplesPerLine = 27648,
            .bytesPerColor = 1,
            .moveLines = 16,
            .colorOffset = 5
        };
        break;
    }
}

void CCD_Ctrl::setRgbGainOfs(uint8_t rOfs,uint8_t gOfs,uint8_t bOfs,uint8_t rGain,uint8_t gGain,uint8_t bGain){
    set_rgb_gain_ofs(&sm_data, rOfs, gOfs, bOfs, rGain, gGain, bGain);
}

bool CCD_Ctrl::initCCD(){
    
    setClock(true);
    sleep_ms(100);
    send_ctrl_data_arr(&sm_data,startcommands,sizeof(startcommands)/sizeof(uint32_t));
    //sleep_ms(100);
    uint32_t state1 = read_ctrl_data_sm(&sm_data,0x61);
    //sleep_ms(100);
    send_ctrl_data_arr(&sm_data,startcommands2,sizeof(startcommands2)/sizeof(uint32_t));
    //sleep_ms(100);
    send_ctrl_data_arr(&sm_data,startcommands3,sizeof(startcommands3)/sizeof(uint32_t));
    send_ctrl_data_arr(&sm_data,register_settings,sizeof(register_settings)/sizeof(uint32_t));
    send_ctrl_data_arr(&sm_data,startcommands4,sizeof(startcommands4)/sizeof(uint32_t));

    uint32_t state = read_ctrl_data_sm(&sm_data,0x61);
    std::cout << "CCD reply1 " << state1 << " reply 2 "  << state << std::endl;
    return state1 != 0 && (state != 0 && state != 0xffff);
}

uint32_t CCD_Ctrl::readReg(uint8_t adr){
    return read_ctrl_data_sm(&sm_data,adr);
}
#else
CCD_Ctrl::CCD_Ctrl(){};
CCD_Ctrl::~CCD_Ctrl(){};
void CCD_Ctrl::setClock(bool enable){};
void CCD_Ctrl::setupSm(){};
bool CCD_Ctrl::initCCD(){return true;};

void CCD_Ctrl::startCapture(){};
void CCD_Ctrl::endCapture(){};

void CCD_Ctrl::captureFrame(uint32_t lines,std::string& filename,uint32_t tgperiod){};
void CCD_Ctrl::captureFrame(uint32_t lines,FILE *fptr,uint32_t tgperiod){};

void CCD_Ctrl::captureData(uint32_t* buffer,uint32_t length,uint32_t tgperiod){};

void CCD_Ctrl::sendCtrlData(uint8_t cmd,uint8_t data1,uint8_t data2){};
void CCD_Ctrl::sendCtrlData(uint32_t data){};
void CCD_Ctrl::sendCtrlDataArr(const uint32_t* buf,uint32_t len){};
uint32_t CCD_Ctrl::readReg(uint8_t adr){return 0;};

uint32_t CCD_Ctrl::setResolution(CCD_Resolution res){curRes = res; return 0;};
void CCD_Ctrl::setRgbGainOfs(uint8_t rOfs,uint8_t gOfs,uint8_t bOfs,uint8_t rGain,uint8_t gGain,uint8_t bGain){};

#endif


std::vector<CCDPixel> CCD_Ctrl::decodeData(uint32_t* buf,uint32_t len){
    std::vector<CCDPixel> line;
    int samples = len*3;

    uint32_t offset = curParams.validPixelBegin;
    uint32_t pixelend = curParams.validPixelEnd;
    line.reserve(pixelend-offset);
    assert(len >= pixelend+offset);
    int i = offset;
    uint32_t samplecount; // Got 4 samples per buf
    //uint8_t* bytebuf = reinterpret_cast<uint8_t*>(buf);
    while(i < pixelend){

        //line.push_back(bytebuf[i]);
        uint32_t d = buf[i]; // r,g,b or BGR?
        line.emplace_back((d >> 16) & 0xff,(d >> 8) & 0xff,d & 0xff); // Save as bgr

        i++;
    }
    return line;
}
