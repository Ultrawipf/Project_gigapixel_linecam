// Constants
// Startup commands sampled from scanner before first calibration sampling
// First block. Setup and constants
extern const uint32_t startcommands[25];
// Second block
extern const uint32_t startcommands2[12];
// Before sampling starts
extern const uint32_t startcommands3[87];
extern const uint32_t register_settings[130];
extern const uint32_t startcommands4[6];

// Sent before every scan and after almost every command
extern const uint32_t block1[5];
// Sent between sampling sections.
extern const uint32_t block2[6];
// Resolutions
extern const uint32_t setupCmds_300dpi[217] ;
extern const uint32_t setupCmds_600dpi[217] ;
extern const uint32_t setupCmds_1200dpi[217];
extern const uint32_t setupCmds_2400dpi[217];
extern const uint32_t setupCmds_4800dpi[217];
