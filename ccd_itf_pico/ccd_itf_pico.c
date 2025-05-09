#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
// #include "hardware/clocks.h"

#include "pio_ccd.pio.h"
#include "string.h"

#include "ccd_commands.h"

#include "ccd_itf_common.h"



// #define ACLKPIN 22 // DATABASEPIN + 8 input,
// #define TGPIN 21
// #define RESETPIN 11
// #define CSPIN 9
// #define DATADBGPIN 7
// #define DATABASEPIN 12
// #define PIOLIB
// const PIO pio = pio0;
SM_def sm_data = {.pio = pio0,.offset = SMDEF_INVALID_VAL,.sm = SMDEF_INVALID_VAL};
SM_def sm_clock = {.pio = pio0,.offset = SMDEF_INVALID_VAL,.sm = SMDEF_INVALID_VAL};
// SM_def sm_stepper = {.pio = pio0,.offset = SMDEF_INVALID_VAL,.sm = SMDEF_INVALID_VAL};



void setup_gpiopins(){
    // Setup CS pin
    gpio_init(CSPIN);
    gpio_set_dir(CSPIN, GPIO_OUT);
    gpio_put(CSPIN, true); // CS idle high

    gpio_init(TGPIN);
    gpio_set_dir(TGPIN, GPIO_OUT);
    gpio_put(TGPIN, false); // TG idle low

    gpio_init(RESETPIN);
    gpio_set_dir(RESETPIN, GPIO_OUT);
    gpio_put(RESETPIN, false); // Reset assert
#ifdef LEDPIN
    gpio_init(LEDPIN);
    gpio_set_dir(LEDPIN, GPIO_OUT);
    gpio_put(LEDPIN, false); // LED off
#endif
}



void print_data(uint32_t* buf,uint32_t len,uint32_t start){
    double avg = 0;
    double divisor = (len-start);
    printf("P%d:",len-start);
    for(uint i = start;i<len;i++){
        // uint32_t d = (buf[i] & 0xff) << 8; // <<8? First byte
        //     d |= ((buf[i] >> 10)) & 0xff; // >>10? Second byte
        //     d |= (buf[i] & 0x300) << 8; // Clocks of first byte
        //     d |= (buf[i] & 0x30000) << 2; // Clocks of second byte
        uint32_t d = buf[i];
        //uint32_t val = ((d& 0xff) << 8) | ((d & 0xff00) >> 8) |  ((d& 0xff0000) << 8) | ((d & 0xff000000) >> 8); // TODO check byte order...
        // avg += (double)(val) / divisor;
        // if(i < start+100)
        // printf("%04x",val);
        printf("%08x",d);
    }
    printf("W\n");
}

/**
 * @brief Decodes a raw buffer in place into pixel data
 * 
 * @param buf 
 * @param len 
 */
void decode_data(uint32_t* buf,uint32_t len){

}


int main()
{
    stdio_init_all();

    setup_gpiopins();
    const uint32_t samplenum = 122400/2;
    uint32_t buffer[samplenum];
    memset( buffer, 0, sizeof(buffer) );

    setup_clock(&sm_clock,6000000.0,ACLKPIN);
    setup_data(&sm_data,DATABASEPIN);
    pio_sm_clkdiv_restart(sm_clock.pio,sm_clock.sm);
    pio_sm_clkdiv_restart(sm_data.pio,sm_data.sm);
    
    sleep_ms(1000);
 

    // First sampling 2 TG periods? 20k values?

    set_clock(&sm_clock,1); // Enable clock
    sleep_ms(100);
    send_ctrl_data_arr(&sm_data,startcommands,sizeof(startcommands)/sizeof(uint32_t));
    printf("State/Ping %d\n",read_ctrl_data_sm(&sm_data,0x61));
    sleep_ms(100);
    send_ctrl_data_arr(&sm_data,startcommands2,sizeof(startcommands2)/sizeof(uint32_t));
    sleep_ms(100);
    // send_ctrl_data_arr(setupCmds_600dpi,sizeof(setupCmds_600dpi)/sizeof(uint32_t));
    // send_ctrl_data_arr(block2,sizeof(block2)/sizeof(uint32_t));
    send_ctrl_data_arr(&sm_data,startcommands3,sizeof(startcommands3)/sizeof(uint32_t));
    send_ctrl_data_arr(&sm_data,register_settings,sizeof(register_settings)/sizeof(uint32_t));
    send_ctrl_data_arr(&sm_data,startcommands4,sizeof(startcommands4)/sizeof(uint32_t));
   
    // set_rgb_gain_ofs(0x7A,0x7A,0x7A,0x05,0x05,0x05); // Test low gain
    // printf("R %d\n",read_ctrl_data_sm(0x78));
    // printf("G %d\n",read_ctrl_data_sm(0x79));
    // printf("B %d\n",read_ctrl_data_sm(0x7A));
    sleep_ms(100);
    uint32_t tgperiod = set_resolution(&sm_data,CCD_Res_div8)/2; // Mainlines only
    set_rgb_gain_ofs(&sm_data,0x7D,0x7D,0x7D,0x3D,0x32,0x38); // Test dumped gain
    sleep_ms(100);

    // SM_def* smdef = &sm_data;
    // printf("SM:%d PIO: %d OFS:%d\n",smdef->sm,smdef->pio,smdef->offset);
    // pio_sm_set_consecutive_pindirs(sm_data.pio, sm_data.sm, DATABASEPIN+6, 2, true);
    
    // uint32_t tgperiod = 51830/2;
    while (true) {
        // printf("SM:%d PIO: %d OFS:%d\n",smdef->sm,smdef->pio,smdef->offset);
        // send_ctrl_data_sm(&sm_data,0x60,0x00,0x80); // Stop sampling
        start_sampling(&sm_data);
        sleep_ms(50); // Dump some data first
        memset( buffer, 0, sizeof(buffer) );
        readFrames(&sm_data,buffer,samplenum,tgperiod);
        // readData(&sm_data,buffer,samplenum);
        // sleep_ms(1000);
        stop_sampling(&sm_data);
        print_data(buffer,samplenum,samplenum-(tgperiod/2)); // Print last line
        
        // send_ctrl_data_sm(&sm_data,0x60,0x00,0x81); // Start sampling
        // sleep_ms(1500);
        while(getchar() != 'Q'){
            sleep_ms(1);
        }
    }
}
