#ifndef CCD_CTRL_H
#define CCD_CTRL_H

#include <string>
#include <vector>
#include <tuple>
#include <array>
#include <span>

#define FRAMEDATAOFFSET 0 // 1 for 8b mode
#define SAMPLESPERBUFFER 2 // 2 for 16b, 3 for 8b

enum class CCD_Resolution : unsigned int{
    Full=0,
    Div2=1,
    Div4=2,
    Div8=3,
    Preview=4
};

struct CCD_Parameters{
    uint32_t validPixelBegin = 333;
    uint32_t validPixelEnd = 44500;
    uint32_t samplesPerLine = 122400;
    //uint32_t bytesPerColor = 1;
    uint32_t moveLines = 1;
    uint32_t lineBuffers = 0;
    std::vector<int> lineOffsets;
};

struct CCD_GainOfs{
    uint8_t rOfs=0x80;
    uint8_t gOfs=0x80;
    uint8_t bOfs=0x80;
    uint8_t rGain=0x07;
    uint8_t gGain=0x07;
    uint8_t bGain=0x07;
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

typedef uint16_t ccdpixel; // uint8_t or uint16_t
static_assert(sizeof(ccdpixel) <= 2,"ccdpixel must be 1 or 2 bytes long");



// TODO REMOVE
// class CCDPixel{
// public:
//     uint8_t r;
//     uint8_t g;
//     uint8_t b;
//     CCDPixel(uint8_t r,uint8_t g, uint8_t b) : r(r),g(g),b(b){}
// };


class CCD_ImageCorrection{
public:
    CCD_ImageCorrection(uint32_t linelength,uint32_t bufferlength);
    ~CCD_ImageCorrection();

    void offsetLine(uint32_t start,uint32_t stride,int offset);
    void addLine(const std::vector<ccdpixel> line);
    void addLineBuf(const uint32_t* line, uint32_t len);
    std::vector<ccdpixel> getLine(int bufferoffset);
    int32_t incrementLine();

    //static uint32_t getOffsetForLine(CCD_Resolution res,uint32_t line);


private:
    int32_t getIdxOffset(int offset);
    int32_t linelength;
    int32_t bufferlength;
    int32_t curIdx = 0; // Index wraps buffer
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
    void captureFrame(uint32_t lines,FILE *fptr,uint32_t tgperiod,bool reverse = false);

    void captureData(uint32_t* buffer,uint32_t length,uint32_t tgperiod);

    void sendCtrlData(uint8_t cmd,uint8_t data1,uint8_t data2);
    void sendCtrlData(uint32_t data);
    void sendCtrlDataArr(const uint32_t* buf,uint32_t len);
    uint32_t readReg(uint8_t adr);

    static CCD_Parameters getParamsFromResolution(CCD_Resolution res);

    static void copyRawBufferToFramebuffer(const uint32_t* rawbuf,uint32_t rawbuflen,ccdpixel* framebuf, uint32_t framebuflen,uint8_t offset=FRAMEDATAOFFSET,bool reverse = false);


    void setResolution(CCD_Resolution res);
    //void setRgbGainOfs(uint8_t rOfs,uint8_t gOfs,uint8_t bOfs,uint8_t rGain,uint8_t gGain,uint8_t bGain);
    void setRgbGainOfs(CCD_GainOfs ccdGainOfs);

    CCD_Parameters getCurParams();
    CCD_GainOfs getCurGainOfs();


    bool rawmode = true;

    static constexpr float lineDistMM = 0.001; // 0.001? 0.00125? // Pixel size (4-4.5um?)

    static constexpr int colorLineDistShift = 98; //96,98? 103? //22*4; // Ok 24*4?. 80 for 0.0011
    static constexpr int subLineDistShift = 8; // Ok, 6 for 0.0011
    static constexpr int mainLineDistShift = 50; // 40 for 0.0011

    // static constexpr float mainLineDistMM = lineDistMM*12; // 12 lines
    // static constexpr float subLineDistMM = lineDistMM*2; // 2 lines
    // static constexpr float colorLineDistMM = lineDistMM*24; // 2 main lines

    static constexpr int maxLineMovementSkip = 12; // MoveLines of lowest resolution


    //Execute line shifts for every line
    // static constexpr const std::array<int,12> lineshiftsFullres = {colorLineDistShift*2,colorLineDistShift,0,
    //                                                colorLineDistShift*2+subLineDistShift,colorLineDistShift*1+subLineDistShift,subLineDistShift,
    //                                                colorLineDistShift*2+mainLineDistShift,colorLineDistShift*1+mainLineDistShift,mainLineDistShift,
    //                                                colorLineDistShift*2+mainLineDistShift+subLineDistShift,colorLineDistShift*1+mainLineDistShift+subLineDistShift,mainLineDistShift+subLineDistShift
    // };

    // Retune offsets if skip changes

    static constexpr int colorLineDistShiftFullres = 100;//98;
    static constexpr const std::array<int,12> lineshiftsFullres = {
    colorLineDistShiftFullres*2+subLineDistShift,colorLineDistShiftFullres*1+subLineDistShift,subLineDistShift,
    colorLineDistShiftFullres*2+mainLineDistShift+subLineDistShift,colorLineDistShiftFullres*1+mainLineDistShift+subLineDistShift,mainLineDistShift+subLineDistShift,
    colorLineDistShiftFullres*2,colorLineDistShiftFullres,0,
    colorLineDistShiftFullres*2+mainLineDistShift,colorLineDistShiftFullres*1+mainLineDistShift,mainLineDistShift,
    };

    // Tested and working! ?! maybe?
    static constexpr int colorLineDistShiftHalfres = colorLineDistShift;
    static constexpr int subLineDistShiftHalfres = 8;
    static constexpr const std::array<int,6> lineshiftsHalfres = {

        colorLineDistShiftHalfres*2,colorLineDistShiftHalfres,0,
        colorLineDistShiftHalfres*2+subLineDistShiftHalfres,colorLineDistShiftHalfres*1+subLineDistShiftHalfres,subLineDistShiftHalfres
     };

    // Tested and working!
    static constexpr const std::array<int,3> lineshiftsQuarterres = {colorLineDistShift*2,colorLineDistShift,0}; // 96?

protected:
#ifndef DUMMY
    SM_def sm_data;
    SM_def sm_clock;
#endif
    const int totalPixels = 122400;
    CCD_Resolution curRes;
    CCD_Parameters curParams;
    CCD_GainOfs curGainOfs;
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
