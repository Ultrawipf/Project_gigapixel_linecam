#include "pico/stdlib.h"
#include "constants_pi.h"

#define ACLKPIN 27 // DATABASEPIN + 8 input,
#define TGPIN 25
//#define RESETPIN 8 // 5
#define CSPIN 26
//#define DATADBGPIN 1
#define DATABASEPIN 16
//#define LEDPIN 4

// Measured on kernel 6.18 / piolib 2026: alignment is error free up to 8.3 MHz, but above ~6.3 MHz
// the AFE no longer settles fully (more noise, less signal). 6 MHz is the sensor's design clock.
// Older kernels need 5 MHz because the DMA could not keep up
#define CCDCLOCK 6000000.0f
// Data SM at the full system clock, no fractional divider jitter. The sample delay below is in its cycles (5 ns)
#define PIODATACLOCK 200000000.0f
// Data SM cycles between a clock edge and sampling. Valid 2..9 at 6 MHz, 6 is the center
#define PIOSAMPLEDELAY 6


#define BYTESPERCOLOR 1
