#include "tmc2209_motor.h"
#include <iostream>
#include <math.h>
#include "constants_pi.h"

#ifdef TMC2209
TMC2209_motor::TMC2209_motor(int step,int dir, int en,int diag,const std::string& serial,uint8_t addr)
    : step_pin(step),dir_pin(dir),en_pin(en),diag_pin(diag),serialportname(serial),addr(addr)
{
    std::cout << "Motor serial: " << serial << std::endl;
    //updateConfigFromChip();
}

TMC2209_motor::~TMC2209_motor(){
    setEnable(false);
}

void TMC2209_motor::init(){
    wiringPiSetupGpio();
    if(step_pin>=0)
        pinMode(step_pin, OUTPUT);
    if(dir_pin>=0)
        pinMode(dir_pin, OUTPUT);
    if(en_pin>=0)
        pinMode(en_pin, OUTPUT);
    if(diag_pin>=0){
        pinMode(diag_pin, INPUT);
    }
    if(!digitalRead(RESETPIN)){
        pinMode(RESETPIN, OUTPUT);
        digitalWrite(RESETPIN, 1);
    }
    updateConfigFromChip();
}

bool TMC2209_motor::updateConfigFromChip(){
    if(serialportname.empty()){
        return false;
    }
    writeData(0x01,0x07);
    curgconf.asint = readData(0x00);
    curchopconf.asint = readData(0x6C);
    curStepMult = 256 >> curchopconf.mres;
    curcoolconf.asint = readData(0x42);
    curpinsconf.asint = readData(0x06);
    if(verbose){
        printf("Gconf: %x\n",curgconf.asint);
        printf("Chopconf: %x\n",curchopconf.asint);
        printf("Coolconf: %x\n",curcoolconf.asint);
        printf("Pinsconf: %x\n",curpinsconf.asint);
    }

    initialized = true;
    return true;
}

int64_t TMC2209_motor::getPos(){
    return this->pos;
}
void TMC2209_motor::setPos(int64_t pos){
    this->pos = pos;
}

bool TMC2209_motor::isMoving(){
    return this->movingFlag;
}

// UART Helper functions ------------------------
void TMC2209_motor::setUartAddr(uint8_t addr){
    this->addr = addr;
}

/**
 * @brief TMC2209_motor::writeData writes a register
 * @param reg Register to write
 * @param data Data to write
 */
void TMC2209_motor::writeData(uint8_t reg,uint32_t data){
    if(this->serialportname.empty()){return;}
    TMC2209_cmd64 buf = {syncbyte,this->addr,reg,1,reverseData(data),0};
    buf.crc = calculateCrc(buf.asbytes,8);
    // Send via uart
    if(verbose)
        printf("SND: ");
    if(!openSerial(serialportname)){
        return;
    }
    for(uint8_t i = 0;i<8;i++){
        serialPutchar(serialport,buf.asbytes[i]);
        if(verbose)
            printf("%x, ",buf.asbytes[i]);
    }
    if(verbose)
        printf("\n");

    closeSerial();
}

/**
 * @brief TMC2209_motor::readData reads data from driver
 * @param reg Register to read
 * @return Data field or 0xffffffff on error
 */
uint32_t TMC2209_motor::readData(uint8_t reg){
    TMC2209_cmd32 buf = {syncbyte,this->addr,reg,0,0};
    // Send via uart
    if(!openSerial(serialportname)){
        return 0xffffffff;
    }
    serialFlush(serialport);
    buf.crc = calculateCrc(buf.asbytes,4);
    if(verbose)
        printf("SND: ");
    for(uint8_t i = 0;i<4;i++){
        serialPutchar(serialport,buf.asbytes[i]);
        if(verbose)
            printf("%x ",buf.asbytes[i]);
    }
    TMC2209_cmd64 reply = {0};
    //int d;
    if(verbose)
        printf("\nRCV: ");
    int cnt = 6;
    // Wait for sync
    while((serialGetchar(serialport)) != 0x05 && cnt--){ }
    if(!cnt){
        return 0xffffffff;
    }
    for(uint8_t i = 1;i<8;i++){
        reply.asbytes[i] = serialGetchar(serialport);
        if(verbose)
            printf("%x ",reply.asbytes[i]);
    }
    if(verbose)
        printf("\n");
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
        fprintf (stderr, "Unable to open serial device: %d, %s\n", (errno),path.c_str()) ;
        return false;
    }
    return true;
}

void TMC2209_motor::closeSerial(){
    serialClose(serialport);
}

// ----------------------------------

// Control
bool TMC2209_motor::ping(){
#ifdef DUMMY
    return true;
#else
    updateConfigFromChip();
    return curpinsconf.version != 0xffff && curpinsconf.version != 0x00;
#endif
}

void TMC2209_motor::setStallguard(uint8_t val){
    writeData(0x40,val&0x7f);
}

void TMC2209_motor::setCoolstepTh(uint32_t th){
    writeData(0x14,th&0xFFFFF);
}

void TMC2209_motor::setMicrostep(microstep ms){
    if(!initialized){ updateConfigFromChip(); }
    if(ms == microstep::pin){
        curgconf.mstep_reg = 0;
    }else{
        curgconf.mstep_reg = 1;
        curchopconf.mres = static_cast<uint8_t>(ms);
    }
    writeData(0x6C,curchopconf.asint);
    writeData(0x00,curgconf.asint);
    curStepMult = 256 >> curchopconf.mres;
}

void TMC2209_motor::setInterpolation(bool en){
    if(!initialized){ updateConfigFromChip(); }
    curchopconf.intpol = en;
    writeData(0x6C,curchopconf.asint);
}

void TMC2209_motor::moveTo(int64_t position,float speed,float accel){
    int64_t dist = position - this->pos;
    move(dist,speed,accel);
}

int TMC2209_motor::getStepMult(){
    return curStepMult;
}

float TMC2209_motor::uminToStepFreq(float speed,float stepsPerRotation){
    return (speed / 60.0f) * stepsPerRotation;
}

void TMC2209_motor::doStep(int delayus){
    //if(verbose)
    //    std::cout << "STEP: " << delayus << std::endl;
    digitalWrite(step_pin, 1);
    delayMicroseconds(delayus);
    digitalWrite(step_pin, 0);
    delayMicroseconds(delayus);
}

int TMC2209_motor::speedToUs(float speed){
    speed = std::max<float>(speed,1.0f);
    return 1000000.0f / (speed*2.0);
}

bool TMC2209_motor::home(int64_t maxSteps){
    if(diag_pin<0)
        return false;
    movingFlag = true;
    setCoolstepTh(homeCoolstepTh);
    setStallguard(homeSG);
    int8_t dir = maxSteps < 0 ? -1 : 1;
    digitalWrite (dir_pin, invdir != maxSteps < 0);
    maxSteps = abs(maxSteps);
    int delayus = speedToUs(homeSpeed);
    while(maxSteps > 0)
    {
        doStep(delayus);
        pos += dir;
        maxSteps--;
        if(digitalRead(diag_pin)){
            pos = 0;
            movingFlag = false;
            homed = true;
            return true;
        }
    }
    movingFlag = false;
    homed = false;
    return false;
}

bool TMC2209_motor::wasHomed(){
    return homed;
}

// Blocking
int TMC2209_motor::move(int64_t steps,float speed,float accel){
    if(step_pin < 0 || dir_pin < 0 || speed < 1 || accel < 1){
        return 0;
    }
    bool lastEnable = enabled;
    setEnable(true);
    movingFlag = true;

    if(speed < 0){
        steps = -steps;
        speed = abs(speed);
    }
    int8_t dir = steps < 0 ? -1 : 1;
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

    int64_t totalsteps = steps;
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
        pos += dir;
        steps--;
    }

    // Constant
    delayus = speedToUs(speed);
    while(steps > accelsteps)
    {
        doStep(delayus);
        pos += dir;
        steps--;
    }
    // Decelerate
    while(steps > 0)
    {
        delayus = speedToUs(speed * sin(1.57079632f * ((curaccelstep-- + speedmultramp)/(accelsteps_f+speedmultramp))));
        doStep(delayus);
        pos += dir;
        steps--;
    }


    setEnable(lastEnable);
    movingFlag = false;
    return totalsteps-steps;
}

void TMC2209_motor::setEnable(bool enable){
    if(en_pin >= 0)
        digitalWrite (en_pin, !enable);
    enabled = enable;
}

#endif



