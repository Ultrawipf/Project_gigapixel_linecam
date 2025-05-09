#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "piolib.h"
#include "pio_ccd.h"
#include "string.h"


PIO pio; // Only one pio
int sm_clock = 255;
int sm_data = 255;

#define TRANSFERSIZE 122400 // One line 122400
#define  MAX( a, b ) ( ( a > b) ? a : b ) 
#define  MIN( a, b ) ( ( a < b) ? a : b ) 

int set_clock(int enable){
    if(sm_clock == 255){
        return 0;
    }
    if(enable){
        pio_sm_set_enabled(pio, sm_clock, true); // Enable SM
        return 1;
    }else{
        pio_sm_set_enabled(pio, sm_clock, false); // Disable SM
        pio_sm_set_pins(pio, sm_clock, 0);  // Disable pins
        return 1;
    }
}

int setup_clock(int argc, const char **argv){

    uint offset;

    uint gpio = 4;

    if (argc == 2)
        gpio = (uint)strtoul(argv[1], NULL, 0);


    sm_clock = pio_claim_unused_sm(pio, true);

    offset = pio_add_program(pio, &ccdclock_program);
    printf("Loaded ccd clock at %d, using sm %d, gpio %d\n", offset, sm_clock, gpio);

    pio_sm_clear_fifos(pio, sm_clock);

    ccdclock_program_init(pio, sm_clock, offset, gpio, 6000000.0); // Generate 6MHz clock

    set_clock(1);
    
    getchar();

    set_clock(0);
    return 0;
}

int setup_data(){
    uint offset;
    uint gpiobase = 17; // 9 pins

    sm_data = pio_claim_unused_sm(pio, true);
    pio_sm_config_xfer(pio, sm_data, PIO_DIR_FROM_SM, TRANSFERSIZE*4, 2);
    offset = pio_add_program(pio, &ccddata_program);
    printf("Loaded ccd data at %d, using sm %d, gpio %d\n", offset, sm_data, gpiobase);
    ccddata_program_init(pio, sm_data, offset, gpiobase, 60000000.0f); // 60000000

    pio_sm_clear_fifos(pio, sm_data);
    return 0;
}


int readData(uint32_t* buf,int samples){
    
    if(sm_data == 255){
        return 0;
    }
    // pio_sm_set_enabled(pio, sm_data, true); // Enable SM
    int ret;
    int ofs = 0;
    while(ofs < samples){
        int curlen = MIN(samples-ofs,TRANSFERSIZE);
        ret = pio_sm_xfer_data(pio, sm_data, PIO_DIR_FROM_SM, curlen*sizeof(uint32_t), buf+ofs);
        ofs += curlen;
    }
    
    // pio_sm_set_enabled(pio, sm_data, false); // Disable SM
    if(ret){
        printf("Transfer failed %d\n",ret);
        return 0;
    }
    return ofs;
}

int check_if_idle(uint32_t* buf,int samples){
    for(uint i = 0;i<samples;i++){
        if( (buf[i] & 0x3F3F) != 0){
            return 0;
        }
    }
    return 1;
}

int main(int argc, const char **argv)
{
    pio = pio0;
    // pio_clear_instruction_memory(pio);
    // pio_sm_unclaim(pio,0);
    // setup_clock(argc,argv);
    setup_data();

    uint32_t samples = TRANSFERSIZE;
    uint32_t buf[1][samples];
    memset( buf[0], 0, samples*sizeof(uint32_t) );
    // memset( buf[1], 0, samples*sizeof(uint32_t) );
    printf("Waiting\n");

    FILE *fptr;
    fptr = fopen("/tmp/dump.bin", "wb");

    pio_sm_set_enabled(pio, sm_data, true); // Enable SM
    // while((pio_sm_get_blocking(pio,sm_data) & 0x3F3F) == 0){
    //     sleep_ms(10);
    // }

    for(uint i = 0;i<100;i++){
        printf("Dat %d\n",pio_sm_get_blocking(pio,sm_data));
    }
    
    
    
    // Open a file in writing mode
    uint32_t tgcnt = 0;
    bool foundTg = false;
    // for(uint32_t line = 0; line < 500;line++){
    uint8_t curbuf = 0;
    do{
        // curbuf = curbuf++ % 2;
        printf("Sampling\n");
        memset( buf[curbuf], 0, samples*sizeof(uint32_t) );
        int amount = readData(buf[curbuf],samples);

        
        // for(uint i = 0;i<samples;i++){
        //     printf("%d ",buf[i]);
        // }
        printf("Sampled %d\n",amount);
        foundTg = false;

        /*
        Transform data for file. Output 16b+4 0xff +2 0xff +2
        */
        
        for(uint32_t i = 0;i<samples;i++){
            // uint32_t d = (buf[curbuf][i] & 0xff); // <<8?
            // d |= ((buf[curbuf][i] >> 2)) & 0xff00; // >>10?
            // d |= (buf[curbuf][i] & 0x300) << 8;
            // fwrite(&d, 3, 1, fptr);
            uint32_t d = (buf[curbuf][i] & 0xff); // 10b
            d |= ((buf[curbuf][i] >> 2)) & 0xff00; // >>10?
            d |= ((buf[curbuf][i] >> 4)) & 0xff0000; // >>10?
            d |= (buf[curbuf][i] & 0x20000000) >> 2; //TG3
            d |= (buf[curbuf][i] & 0x80000) << 7; //TG2
            d |= (buf[curbuf][i] & 0x200) << 16; //TG1
            
            
            fwrite(&d, 4, 1, fptr);
            // bool gotTg = buf[curbuf][i] & 0x200200;
            bool gotTg = buf[curbuf][i] & 0x20080200;
            if(gotTg){
                if(tgcnt > 100){
                    printf("TG! %d\n",tgcnt);
                    tgcnt = 0;
                    foundTg = true;
                }
            }else{
                tgcnt+=3;
            }
        }
        if(!foundTg && tgcnt > 57257){
            break;
        }
    }while(foundTg);
            
    pio_sm_set_enabled(pio, sm_data, false); // Disable SM

    // Close the file
    fclose(fptr); 
    

    return 0;
}


