#ifndef CCD_CTRL_H
#define CCD_CTRL_H

#include <string>
#include <vector>
#include <tuple>
#include <array>

enum class CCD_Resolution : unsigned int{
    Full=0,
    Div2=1,
    Div4=2,
    Div8=3,
    Div16=4
};

struct CCD_Parameters{
    uint32_t validPixelBegin = 333;
    uint32_t validPixelEnd = 44500;
    uint32_t samplesPerLine = 122400;
    uint32_t bytesPerColor = 1;
    uint32_t moveLines = 1;
    uint32_t colorOffset = 10; // In pixels. TODO higher resolutions
};

#ifndef DUMMY
#ifdef __cplusplus
extern "C" {
#endif

#include "../epson_ccd_pio/ccd_itf_common.h"
#include "../epson_ccd_pio/ccd_commands.h"

#ifdef __cplusplus
}
#endif

#endif

typedef uint8_t ccdpixel;

// TODO REMOVE
class CCDPixel{
public:
    uint8_t r;
    uint8_t g;
    uint8_t b;
    CCDPixel(uint8_t r,uint8_t g, uint8_t b) : r(r),g(g),b(b){}
};

class CCD_ImageCorrection{
public:
    CCD_ImageCorrection(uint32_t linelength,uint32_t bufferlength);
    ~CCD_ImageCorrection();

    void offsetLine(uint32_t start,uint32_t stride,int offset);
    void addLine(const std::vector<ccdpixel> &line);
    std::vector<ccdpixel> getLine(int bufferoffset);
    uint32_t incrementLine();

private:
    uint32_t getIdxOffset(int offset);
    uint32_t linelength;
    uint32_t bufferlength;
    uint32_t curIdx = 0; // Index wraps buffer
    std::vector<std::vector<ccdpixel>> buf;
};

class CCD_Ctrl{
public:
    CCD_Ctrl();
    ~CCD_Ctrl();

    void setClock(bool enable);
    void setupSm();
    bool initCCD();

    void startCapture();
    void endCapture();

    void captureFrame(uint32_t lines,std::string& filename,uint32_t tgperiod);
    void captureFrame(uint32_t lines,FILE *fptr,uint32_t tgperiod);
    std::vector<CCDPixel> decodeData(uint32_t* buf,uint32_t len);

    void captureData(uint32_t* buffer,uint32_t length,uint32_t tgperiod);

    void sendCtrlData(uint8_t cmd,uint8_t data1,uint8_t data2);
    void sendCtrlData(uint32_t data);
    void sendCtrlDataArr(const uint32_t* buf,uint32_t len);
    uint32_t readReg(uint8_t adr);

    static CCD_Parameters getParamsFromResolution(CCD_Resolution res);


    void setResolution(CCD_Resolution res);
    void setRgbGainOfs(uint8_t rOfs,uint8_t gOfs,uint8_t bOfs,uint8_t rGain,uint8_t gGain,uint8_t bGain);


    bool rawmode = true;

    const float moveAccel = 50;
    const float moveSpeed = 500;
    const float lineDistMM = 0.00125; // Pixel size (4-4.5um?)

    const int colorLineDistShift = 24;
    const int subLineDistShift = 2;
    const int mainLineDistShift = 12;

    const float mainLineDistMM = lineDistMM*12; // 12 lines
    const float subLineDistMM = lineDistMM*2; // 2 lines
    const float colorLineDistMM = lineDistMM*24; // 2 main lines


    const std::array<int,12> lineshiftsFullres = {colorLineDistShift*2,colorLineDistShift,0,
                                                   colorLineDistShift*2+subLineDistShift,colorLineDistShift*1+subLineDistShift,subLineDistShift,
                                                   colorLineDistShift*2+mainLineDistShift,colorLineDistShift*1+mainLineDistShift,mainLineDistShift,
                                                   colorLineDistShift*2+mainLineDistShift+subLineDistShift,colorLineDistShift*1+mainLineDistShift+subLineDistShift,mainLineDistShift+subLineDistShift
    };

protected:
#ifndef DUMMY
    SM_def sm_data;
    SM_def sm_clock;
#endif
    const int totalPixels = 122400;
    CCD_Resolution curRes;
    CCD_Parameters curParams;
    //uint32_t curLinelength = totalPixels;
    //uint32_t curLinedist = 4;
};

#if 0
// Dummy implementation does nothing
class CCD_Ctrl{
public:
    CCD_Ctrl(){};
    ~CCD_Ctrl(){};
    void setClock(bool enable){};
    void setupSm(){};
    bool initCCD(){return true;};

    void startCapture(){};
    void endCapture(){};

    void captureFrame(uint32_t lines,std::string& filename,uint32_t tgperiod){};
    void captureFrame(uint32_t lines,FILE *fptr,uint32_t tgperiod){};
    void decodeData(uint32_t* buf,uint32_t len){};

    void readFrames(uint32_t* buffer,uint32_t length,uint32_t tgperiod){};

    void sendCtrlData(uint8_t cmd,uint8_t data1,uint8_t data2){};
    void sendCtrlData(uint32_t data){};
    void sendCtrlDataArr(const uint32_t* buf,uint32_t len){};
    uint32_t readData(uint8_t adr){return 0;};

    uint32_t setResolution(CCD_Resolution res){curRes = res;};
    void setRgbGainOfs(uint8_t rOfs,uint8_t gOfs,uint8_t bOfs,uint8_t rGain,uint8_t gGain,uint8_t bGain){};

protected:
    const int totalPixels = 122400;
    CCD_Resolution curRes;
    uint32_t curLinelength = totalPixels;
};

#endif


#endif // CCD_CTRL_H
