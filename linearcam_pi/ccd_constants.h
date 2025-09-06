#include "pico/stdlib.h"
#include "constants_pi.h"

#define ACLKPIN 27 // DATABASEPIN + 8 input,
#define TGPIN 25
//#define RESETPIN 8 // 5
#define CSPIN 26
//#define DATADBGPIN 1
#define DATABASEPIN 16
//#define LEDPIN 4

#define CCDCLOCK 5000000.0f // 6MHz normal
#define PIODATACLOCK CCDCLOCK*15.0f//60000000.0f


#define BYTESPERCOLOR 1
