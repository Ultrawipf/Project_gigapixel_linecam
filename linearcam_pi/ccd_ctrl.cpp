#include "ccd_ctrl.h"
#include <array>
#include <vector>

#include "cameracontrol.h"
#ifndef DUMMY
#include <wiringPi.h>
#else
#include "wiringpi_dummy/wiringPi.h"
#endif
#include <iostream>
#include <algorithm>

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

    //pinMode(TGPIN, OUTPUT);
    //digitalWrite(TGPIN, false); // TG idle low

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

// Transforms raw samples into an RGB framebuffer
void CCD_Ctrl::copyRawBufferToFramebuffer(const uint32_t* rawbuf,uint32_t rawbuflen,ccdpixel* framebuf, uint32_t framebuflen,uint8_t offset,bool bgr,bool reverse){
    int i = 0;
    int rawbufidx = 0;
    int rawbufcnt = 0;
    if(reverse)
        rawbufidx = rawbuflen;
    const uint32_t samplesPerBuffer = SAMPLESPERBUFFER;
    while(i < offset){
        framebuf[i++] = 0;
    }
    while(i < framebuflen && rawbufcnt < rawbuflen){
        if(!bgr){ // Works for raw data
            framebuf[i++] = (rawbuf[rawbufidx] & 0xff0000) >> 16;
            framebuf[i++] = (rawbuf[rawbufidx] & 0xff00) >> 8;
            framebuf[i++] = (rawbuf[rawbufidx] & 0xff);
        }else{ // Works for bitmap when inverted, applies additional offset!

            framebuf[i++] = (rawbuf[rawbufidx] & 0xff);
            framebuf[i++] = (rawbuf[rawbufidx] & 0xff00) >> 8;
            framebuf[i++] = (rawbuf[rawbufidx] & 0xff0000) >> 16;

        }
        rawbufcnt++;
        if(reverse)
            rawbufidx--;
        else
            rawbufidx++;
    }
    // Pad if required
    while(i < framebuflen){
        framebuf[i++] = 0;
    }
}

// Writes raw image data to file
void CCD_Ctrl::captureFrame(uint32_t lines,FILE *fptr,uint32_t tgperiod,bool bgr,bool reverse){
    //uint32_t offset = FRAMEDATAOFFSET;
    const uint32_t samplesPerBuffer = SAMPLESPERBUFFER;
    int samples = tgperiod;
    uint32_t buffer[samples] = {0};
    ccdpixel framebuf[samples*3];

    for(uint32_t l = 0;l<lines;l++){
        readFrames(&sm_data,buffer,tgperiod,tgperiod);
        // fwrite((uint32_t*)buffer, 3, samples, fptr);
        //const uint8_t zero = 0;
        //fwrite(&zero, 1, offset, fptr);
        // for(uint32_t i = 0;i<tgperiod;i++){
        //     uint32_t pxl = ((buffer[i] & 0xff0000) >> 16 ) | ((buffer[i] & 0xff00)) |( (buffer[i] & 0xff) << 16); // BGR to RGB
        //     fwrite(&pxl, samplesPerBuffer, 1, fptr);
        // }
        //fwrite(&zero, 1, samplesPerBuffer-offset, fptr);
        copyRawBufferToFramebuffer(buffer,samples,framebuf,samples*3,bgr ? 3-FRAMEDATAOFFSET : FRAMEDATAOFFSET,bgr,reverse);
        fwrite(&framebuf, samples*3, 1, fptr);
    }
    // Close the file
    
}

void CCD_Ctrl::setResolution(CCD_Resolution res){
    curRes = res;
    set_resolution(&sm_data,static_cast<CCD_Resolution_t>(res));
    curParams = getParamsFromResolution(res);
}

CCD_Parameters CCD_Ctrl::getParamsFromResolution(CCD_Resolution res){
    std::vector<int> lineOffsets;
    switch(res){
    case CCD_Resolution::Full: // Main+Sub x2
        lineOffsets.assign(lineshiftsFullres.begin(),lineshiftsFullres.end());
        return CCD_Parameters{
            .validPixelBegin = 333,
            .validPixelEnd = 40800,
            .samplesPerLine = 130548,
            .bytesPerColor = 1,
            .moveLines = 1,
            .lineBuffers = 193,
            .lineOffsets = lineOffsets,
        };
        break;
    case CCD_Resolution::Div2: // Main+Sub x1
        lineOffsets.assign(lineshiftsHalfres.begin(),lineshiftsHalfres.end());
        std::for_each(lineOffsets.begin(),lineOffsets.end(),[](int &v){v /= 2;});
        return CCD_Parameters{
            .validPixelBegin = 333,
            .validPixelEnd = 40800/2,
            .samplesPerLine = 65280,
            .bytesPerColor = 1,
            .moveLines = 2,
            .lineBuffers = 97,
            .lineOffsets=lineOffsets, // /2
        };
        break;
    case CCD_Resolution::Div4: // Main only
        lineOffsets.assign(lineshiftsQuarterres.begin(),lineshiftsQuarterres.end());
        std::for_each(lineOffsets.begin(),lineOffsets.end(),[](int &v){v /= 4;});
        return CCD_Parameters{
            .validPixelBegin = 333,
            .validPixelEnd = 40800/4,
            .samplesPerLine = 32640,
            .bytesPerColor = 1,
            .moveLines = 4,
            .lineBuffers = colorLineDistShift*2+1,
            .lineOffsets = lineOffsets // /4
        };
        break;
    case CCD_Resolution::Div8: // Main w. skip
        lineOffsets.assign(lineshiftsQuarterres.begin(),lineshiftsQuarterres.end());
        std::for_each(lineOffsets.begin(),lineOffsets.end(),[](int &v){v /= 8;});
        return CCD_Parameters{
            .validPixelBegin = 333,
            .validPixelEnd = 40800/8,
            .samplesPerLine = 18000,
            .bytesPerColor = 1,
            .moveLines = 8,
            .lineBuffers = colorLineDistShift+1,
            .lineOffsets = lineOffsets // /8
        };
        break;
    case CCD_Resolution::Div16: // Mainw. skip
        lineOffsets.assign(lineshiftsQuarterres.begin(),lineshiftsQuarterres.end());
        std::for_each(lineOffsets.begin(),lineOffsets.end(),[](int &v){v /= 16;});
        return CCD_Parameters{
            .validPixelBegin = 333,
            .validPixelEnd = 40800/16,
            .samplesPerLine = 9000,
            .bytesPerColor = 1,
            .moveLines = 16,
            .lineBuffers = colorLineDistShift/2 +1,
            .lineOffsets = lineOffsets // /16
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


CCD_ImageCorrection::CCD_ImageCorrection(uint32_t linelength,uint32_t bufferlength)
    : linelength(linelength),bufferlength(bufferlength)
{
    this->buf.resize(bufferlength); // Add default vectors
    for(int i = 0;i<bufferlength;i++){
        std::vector<ccdpixel>& line = this->buf[i];
        line.resize(linelength,0); // Fill with 0
    }
    std::cout << "Image correction, Len "<<linelength << " buffers " << bufferlength << std::endl;
}


CCD_ImageCorrection::~CCD_ImageCorrection(){

}

int32_t CCD_ImageCorrection::incrementLine(){
    curIdx = getIdxOffset(1);
    return curIdx;
}

int32_t CCD_ImageCorrection::getIdxOffset(int offset){
    int32_t idx = (curIdx + offset) % bufferlength;
    return idx < 0 ? idx + bufferlength : idx;

}

void CCD_ImageCorrection::addLine(const std::vector<ccdpixel> line){
    buf[curIdx] = line;
}

void CCD_ImageCorrection::addLineBuf(const uint32_t* line, uint32_t len){
    //buf[curIdx].insert(buf.begin(),0,FRAMEDATAOFFSET); // Pad front bytes
    // bool bgr = true;
    // CCD_Ctrl::copyRawBufferToFramebuffer(line,len,buf[curIdx].data(),buf[curIdx].size(),bgr ? 3-FRAMEDATAOFFSET : FRAMEDATAOFFSET,bgr,true);
    CCD_Ctrl::copyRawBufferToFramebuffer(line,len,buf[curIdx].data(),buf[curIdx].size());
}

void CCD_ImageCorrection::offsetLine(uint32_t start,uint32_t stride,int offset){
    // Move every stride entry by offset lines in the buffer starting with start to the end
//for i in range(start, len(array[line]), period):
    if(offset == 0){
        return; // Do nothing
    }
//        array[line-offset][i] = array[line][i]
    int32_t offsetIdx = getIdxOffset(offset);
    //std::cout << "Offsetting: " << start << " by " << offset << " from " << curIdx << " to " << offsetIdx << std::endl;

    for(int i = start; i < buf[curIdx].size();i+=stride){
        buf[offsetIdx][i] = buf[curIdx][i];
    }
}



std::vector<ccdpixel> CCD_ImageCorrection::getLine(int bufferoffset){
    //std::cout << "Returning line "<< getIdxOffset(bufferoffset) << std::endl;
    return buf[getIdxOffset(bufferoffset)];
}


