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
    #pragma pack(1)
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

    #pragma pack(1)
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

public:
    TMC2209_motor(int step,int dir, int en=-1,int diag=-1, const std::string& serial="",uint8_t addr = 0);
    void setUartAddr(uint8_t addr);
    int move(int steps,float speed,float accel);
    bool home(int maxSteps); // Home motor using stallguard, returns false on error
    void setEnable(bool enable);

    // Control funcs
    void setStallguard(uint8_t val);

    // Uart
    void writeData(uint8_t reg,uint32_t data);
    uint32_t readData(uint8_t reg);

    bool openSerial(const std::string& path);
    void closeSerial();

    bool invdir = false;
    bool verbose = false;

    uint8_t homeSG = 20;
    float homeSpeed = 2000; // Tune these

    float uminToStepFreq(float speed,float stepsPerRotation);

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
    const std::string& serialportname;
    int serialport = 0;

    static constexpr uint8_t syncbyte = 0x55; // Different than the reply of 0x05

    static constexpr int baudrate = 115200;
};

#endif // TMC22_9_MOTOR_H
