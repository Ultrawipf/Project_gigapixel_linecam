#ifndef TMC22_9_MOTOR_H
#define TMC22_9_MOTOR_H



#include <string>
#define TMC2209
#ifndef DUMMY
#define TMC2209
#include <wiringPi.h>
#include <wiringSerial.h>
//#include <termios.h>
#else
#include "wiringpi_dummy/wiringPi.h"
#include "wiringpi_dummy/wiringSerial.h"
//#include <wiringPi.h>
#endif

class TMC2209_motor
{
public:
    enum class microstep : uint8_t {ms256 = 0,ms128 = 1,ms64 = 2, ms32=3,ms16=4,ms8=5,ms4=6,ms2=7,fullstep=8,pin=0xff};

//#pragma pack(1)
    union TMC2209_cmd64
    {
        struct
        {
            uint64_t sync : 8;
            uint64_t addr : 8;
            uint64_t reg : 7;
            uint64_t rw : 1; // 1 for write, 0 for read
            uint64_t data : 32;
            uint64_t crc : 8;
        };
        char asbytes[8];
    };

//#pragma pack(1)
    union TMC2209_cmd32
    {
        struct
        {
            uint32_t sync : 8;
            uint32_t addr : 8;
            uint32_t reg : 7;
            uint32_t rw : 1; // 1 for write, 0 for read
            uint32_t crc : 8;
        };
        char asbytes[4];
    };

    union TMC2209_chopconf{
        struct{
            uint32_t toff : 4;
            uint32_t hstrt : 3;
            uint32_t hend : 4;
            uint32_t _res : 4;
            uint32_t tbl : 2;
            uint32_t vsense : 1;
            uint32_t _res2 : 6;
            uint32_t mres : 4;
            uint32_t intpol : 1;
            uint32_t dedge : 1;
            uint32_t diss2g : 1;
            uint32_t dioss2vs : 1;
        };
        uint32_t asint;
    };
    union TMC2209_gconf{
        struct{
            uint32_t iscale : 1;
            uint32_t int_rsense : 1;
            uint32_t spreadcycle : 1;
            uint32_t invdir : 1;
            uint32_t idx_otpw : 1;
            uint32_t idx_step : 1;
            uint32_t pdn_dis : 1;
            uint32_t mstep_reg : 1;
            uint32_t multistep_filt : 1;
            uint32_t testmode : 1;
            uint32_t _res : 22;
        };
        uint32_t asint;
    };
    union TMC2209_coolconf{
        struct{
            uint32_t semin : 4;
            uint32_t seup : 2;
            uint32_t _rsv2 : 1;
            uint32_t semax : 4;
            uint32_t _rsv1 : 1;
            uint32_t sedn : 2;
            uint32_t seimin : 1;
        };
        uint32_t asint;
    };
    union TMC2209_pinsconf{
        struct{
            uint32_t enn : 1;
            uint32_t rsv2 : 1;
            uint32_t ms1 : 1;
            uint32_t ms2 : 1;
            uint32_t diag : 1;
            uint32_t rsv : 1;
            uint32_t pdnuart : 1;
            uint32_t spread : 1;
            uint32_t dir : 1;
            uint32_t rsv3 : 14;
            uint32_t version : 7;
        };
        uint32_t asint;
    };

    TMC2209_motor(int step,int dir, float mmToSteps, int en=-1,int diag=-1, const std::string& serial="",uint8_t addr = 0);
    ~TMC2209_motor();
    void init();
    void setUartAddr(uint8_t addr);

    int move(int64_t steps,float speed,float accel);
    void moveTo(int64_t position,float speed,float accel);
    float moveMM(float mm,float speed,float accel);
    void moveToMM(float mm,float speed,float accel);

    inline void updatePosFromSteps(int step);

    bool ping();

    bool home(int64_t maxSteps); // Home motor using stallguard, returns false on error
    void setEnable(bool enable);

    // Control funcs
    void setStallguard(uint8_t val);
    void setMicrostep(microstep ms);
    void setInterpolation(bool en);
    void setCoolstepConf(TMC2209_coolconf conf);
    void setCoolstepTh(uint32_t th);

    int getStepMult();

    int64_t getPos();
    void setPos(int64_t pos);
    float getActualPos();

    float stepsToMM(int64_t steps);

    // Uart
    void writeData(uint8_t reg,uint32_t data);
    uint32_t readData(uint8_t reg);

    bool openSerial(const std::string& path);
    void closeSerial();

    bool invdir = false;
    bool verbose = false;

    uint8_t homeSG = 20;
    uint32_t homeCoolstepTh = 100*8;
    float homeSpeed = 2500; // Tune these

    float uminToStepFreq(float speed,float stepsPerRotation);

    bool updateConfigFromChip();

    bool isMoving();
    bool wasHomed();

protected:
    int speedToUs(float speed);

    //Uart helpers
    uint8_t calculateCrc(char* data, uint8_t size);
    uint32_t reverseData(uint32_t data);

    inline void doStep(int delayus);

    int stepsPerRotation = 200;

    //void diagIsrCb();
private:
    const int step_pin;
    const int dir_pin;
    const int en_pin;
    const int diag_pin;
    const float speedmultramp = 10;
    uint8_t addr;
    const std::string serialportname;
    int serialport = 0;

    bool enabled = false;
    bool movingFlag = false;
    bool homed = false;

    float mmToSteps;

    static constexpr uint8_t syncbyte = 0x55; // Different than the reply of 0x05

    static constexpr int baudrate = 115200;
    bool initialized = false;

    TMC2209_chopconf curchopconf;
    TMC2209_gconf curgconf;
    TMC2209_coolconf curcoolconf;
    int64_t pos = 0; // step counter
    float targetPosMM = 0.0f; // target position in MM for error calculation
    TMC2209_pinsconf curpinsconf;

    float stepMult = 8;
};

#endif // TMC22_9_MOTOR_H
