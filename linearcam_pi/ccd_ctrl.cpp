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
    int samples = tgperiod;
    uint32_t buffer[samples] = {0};

    start_sampling(&sm_data);
    sleep_ms(50); // Dump some data first
    


    for(uint32_t l = 0;l<lines;l++){
        readFrames(&sm_data,buffer,samples,tgperiod);
        for(uint32_t i = 0;i<samples;i++){
            fwrite(&buffer[i], 4, 1, fptr);
        }
    }
    // Close the file
    
    stop_sampling(&sm_data);

}

uint32_t CCD_Ctrl::setResolution(CCD_Resolution res){
    curRes = res;
    curLinelength = set_resolution(&sm_data,static_cast<CCD_Resolution_t>(res));
    return curLinelength;
}

void CCD_Ctrl::setRgbGainOfs(uint8_t rOfs,uint8_t gOfs,uint8_t bOfs,uint8_t rGain,uint8_t gGain,uint8_t bGain){
    set_rgb_gain_ofs(&sm_data, rOfs, gOfs, bOfs, rGain, gGain, bGain);
}

bool CCD_Ctrl::initCCD(){
    
    setClock(true);
    sleep_ms(10);
    send_ctrl_data_arr(&sm_data,startcommands,sizeof(startcommands)/sizeof(uint32_t));
    uint32_t state1 = read_ctrl_data_sm(&sm_data,0x61);
    send_ctrl_data_arr(&sm_data,startcommands2,sizeof(startcommands2)/sizeof(uint32_t));
    send_ctrl_data_arr(&sm_data,startcommands3,sizeof(startcommands3)/sizeof(uint32_t));
    send_ctrl_data_arr(&sm_data,register_settings,sizeof(register_settings)/sizeof(uint32_t));
    send_ctrl_data_arr(&sm_data,startcommands4,sizeof(startcommands4)/sizeof(uint32_t));

    uint32_t state = read_ctrl_data_sm(&sm_data,0x61);
    std::cout << "CCD reply1 " << state1 << " reply 2 "  << state << std::endl;
    return state1 != 0 && (state != 0 && state != 0xffff);
}

uint32_t CCD_Ctrl::readData(uint8_t adr){
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
uint32_t CCD_Ctrl::readData(uint8_t adr){return 0;};

uint32_t CCD_Ctrl::setResolution(CCD_Resolution res){curRes = res; return 0;};
void CCD_Ctrl::setRgbGainOfs(uint8_t rOfs,uint8_t gOfs,uint8_t bOfs,uint8_t rGain,uint8_t gGain,uint8_t bGain){};

#endif


std::vector<uint8_t> CCD_Ctrl::decodeData(uint32_t* buf,uint32_t len){
    std::vector<uint8_t> line;
    int samples = len*4;
    line.reserve(samples);

    int i = 0;
    uint32_t samplecount; // Got 4 samples per buf
    uint8_t* bytebuf = reinterpret_cast<uint8_t*>(buf);
    while(i < samples){

        line.push_back(bytebuf[i]);

        i++;
    }
    return line;
}
