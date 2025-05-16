#include "tmc2209_motor.h"
#include <iostream>
#include <math.h>
#include "constants_pi.h"

#ifdef TMC2209
TMC2209_motor::TMC2209_motor(int step,int dir, int en,int diag,const std::string& serial,uint8_t addr)
    : step_pin(step),dir_pin(dir),en_pin(en),diag_pin(diag),serialportname(serial),addr(addr)
{
    wiringPiSetupGpio();
    if(step>=0)
        pinMode(step, OUTPUT);
    if(dir>=0)
        pinMode(dir, OUTPUT);
    if(en>=0)
        pinMode(en, OUTPUT);
    if(diag>=0){
        pinMode(diag, INPUT);
    }
    if(!digitalRead(RESETPIN)){
        pinMode(RESETPIN, OUTPUT);
        digitalWrite(RESETPIN, 1);
    }
    if(!serial.empty()){
        // Ping
        uint32_t conf = readData(0x00);
        std::cout << "CONF: " << conf << std::endl;
    }
}


void TMC2209_motor::setUartAddr(uint8_t addr){
    this->addr = addr;
}

void TMC2209_motor::writeData(uint8_t reg,uint32_t data){
    if(this->serialportname.empty()){return;}
    TMC2209_cmd64 buf = {syncbyte,this->addr,reg,1,reverseData(data),0};
    buf.crc = calculateCrc(buf.asbytes,8);
    // Send via uart
    if(!openSerial(serialportname)){
        return;
    }
    for(uint8_t i = 0;i<8;i++){
        serialPutchar(serialport,buf.asbytes[i]);
    }

    closeSerial();
}

uint32_t TMC2209_motor::readData(uint8_t reg){
    TMC2209_cmd32 buf = {syncbyte,this->addr,reg,0,0};
    // Send via uart
    if(!openSerial(serialportname)){
        return 0xffffffff;
    }
    serialFlush(serialport);
    buf.crc = calculateCrc(buf.asbytes,4);
    for(uint8_t i = 0;i<4;i++){
        serialPutchar(serialport,buf.asbytes[i]);
    }
    TMC2209_cmd64 reply = {0};
    //int d;
    int cnt = 6;
    // Wait for sync
    while((serialGetchar(serialport)) != 0x05 && cnt--){ }
    if(!cnt){
        return 0xffffffff;
    }
    for(uint8_t i = 1;i<8;i++){
        reply.asbytes[i] = serialGetchar(serialport);
    }

    closeSerial();
    return reverseData(reply.data);
}

// CRC814
uint8_t TMC2209_motor::calculateCrc(char* data, uint8_t size){
    uint8_t crc = 0;
    uint8_t b=0;
    for (uint8_t i=0; i<(size - 1); ++i){
        b = data[i];
        for (uint8_t j=0; j<8; ++j){
            if ((crc >> 7) ^ (b & 0x01)){
                crc = (crc << 1) ^ 0x07;
            }else{
                crc = crc << 1;
            }
            b = b >> 1;
        }
    }
    return crc;
}

// Reverse data field bits
uint32_t TMC2209_motor::reverseData(uint32_t data){
    uint32_t reversed_data = 0;
    uint8_t right_shift;
    uint8_t left_shift;
    for (uint8_t i=0; i<4; ++i){
        right_shift = (4 - i - 1) * 8;
        left_shift = i * 8;
        reversed_data |= ((data >> right_shift) & 0xff) << left_shift;
    }
    return reversed_data;
}

bool TMC2209_motor::openSerial(const std::string& path){
    if ((serialport = serialOpen (path.c_str(), baudrate)) < 0)
    {
        fprintf (stderr, "Unable to open serial device: %d\n", (errno)) ;
        return false;
    }
    return true;
}

void TMC2209_motor::closeSerial(){
    serialClose(serialport);
}

// Control functions
void TMC2209_motor::setStallguard(uint8_t val){
    writeData(0x40,val&0x7f);
}

float TMC2209_motor::uminToStepFreq(float speed,float stepsPerRotation){
    return (speed / 60.0f) * stepsPerRotation;
}

void TMC2209_motor::doStep(int delayus){
    if(verbose)
        std::cout << "STEP: " << delayus << std::endl;
    digitalWrite(step_pin, 1);
    delayMicroseconds(delayus);
    digitalWrite(step_pin, 0);
    delayMicroseconds(delayus);
}

int TMC2209_motor::speedToUs(float speed){
    speed = std::max<float>(speed,1.0f);
    return 1000000.0f / (speed*2.0);
}

bool TMC2209_motor::home(int maxSteps){
    if(diag_pin<0)
        return false;
    setStallguard(homeSG);

    digitalWrite (dir_pin, invdir != maxSteps < 0);
    maxSteps = abs(maxSteps);
    int delayus = speedToUs(homeSpeed);
    while(maxSteps > 0)
    {
        doStep(delayus);
        maxSteps--;
        if(digitalRead(diag_pin)){
            return true;
        }
    }
    return false;
}

// Blocking
int TMC2209_motor::move(int steps,float speed,float accel){
    if(step_pin < 0 || dir_pin < 0 || speed < 1 || accel < 1){
        return 0;
    }
    //setEnable(true);
    if(speed < 0){
        steps = -steps;
        speed = abs(speed);
    }
    digitalWrite (dir_pin, invdir != steps < 0);
    steps = abs(steps);
    int delayus;
    // Limit speed if not possible to reach with acceleration
    int accelsteps = speed / accel;
    if(2*accelsteps > steps){
        speed = accel * steps/2;
        if(verbose)
            std::cout << "Limiting speed:" << speed << std::endl;
        accelsteps = speed / accel;
    }

    int totalsteps = steps;
    float curaccelstep = 0;
    float accelsteps_f = accelsteps;
    if(verbose)
        std::cout << "Accel steps:" << accelsteps << std::endl;

    // Accelerate
    while(steps > totalsteps-accelsteps)
    {
        float stp = ((++curaccelstep+speedmultramp)/(accelsteps_f+speedmultramp));
        float newspeed = speed * sin(1.57079632f * stp);
        delayus = speedToUs(newspeed);
        doStep(delayus);
        steps--;
    }

    // Constant
    delayus = 1000000.0f / (speed*2.0f);
    while(steps > accelsteps)
    {
        doStep(delayus);
        steps--;
    }
    // Decelerate
    while(steps > 0)
    {
        delayus = speedToUs(speed * sin(1.57079632f * ((curaccelstep-- + speedmultramp)/(accelsteps_f+speedmultramp))));
        doStep(delayus);
        steps--;
    }


    //setEnable(false);
    return totalsteps-steps;
}

void TMC2209_motor::setEnable(bool enable){
    if(en_pin >= 0)
        digitalWrite (en_pin, !enable);
}

#endif



