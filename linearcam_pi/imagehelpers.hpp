#ifndef IMAGEHELPERS_H
#define IMAGEHELPERS_H
#include <fstream>    // std::*fstream
#include <memory>     // std::unique_ptr
#include <vector>
#include <cstdio>

#include <spng.h>
#include <iostream>
#include <optional>
#include <functional>
#include <assert.h>
#include <cstring>
#include <cmath>
#include <algorithm>

// Test different options
#define DEFAULT_FILTER SPNG_FILTER_CHOICE_SUB

namespace bmp {

#pragma pack(push, 1)
struct BitmapHeader_s {
    /* Bitmap file header structure */
    std::uint16_t magic = 0x4D42;       /* Magic number for file always BM which is 0x4D42 */
    std::uint32_t file_size = 54;   /* Size of file */
    std::uint16_t reserved1 = 0;   /* Reserved */
    std::uint16_t reserved2 = 0;   /* Reserved */
    std::uint32_t offset_bits = 54; /* Offset to bitmap data */
    /* Bitmap file info structure */
    std::uint32_t size = 40;              /* Size of info header */
    std::int32_t width = 0;              /* Width of image */
    std::int32_t height = 0;             /* Height of image */
    std::uint16_t planes = 1;            /* Number of color planes */
    std::uint16_t bits_per_pixel = 24;    /* Number of bits per pixel */
    std::uint32_t compression = 0;       /* Type of compression to use */
    std::uint32_t size_image = 0;        /* Size of image data */
    std::int32_t x_pixels_per_meter = 3780; /* X pixels per meter */
    std::int32_t y_pixels_per_meter = 3780; /* Y pixels per meter */
    std::uint32_t clr_used = 0;          /* Number of colors used */
    std::uint32_t clr_important = 0;     /* Number of important colors */
};
#pragma pack(pop)
// Sanity check
static_assert(sizeof(BitmapHeader_s) == 54, "Bitmap header size must be 54 bytes");

// Bitmap in BGR format

class BitmapHeader{
public:
    BitmapHeader(int32_t width,int32_t height,uint32_t bpp = 24)
    {
        header.height = height;
        header.width = width;
        if(width*4ll*(height+1) >= 0xffffffff){
            header.size_image = 0; // Ignore header, write rest anyways
        }else{
            header.size_image = (width*height)* (bpp/8);
        }
        header.bits_per_pixel = bpp;
        header.file_size = 54 + header.size_image;
    };

    void writeHeader(std::ofstream& f){
        f.write((char *) &header, 54);
    }

    void writeHeader(FILE* f){
        fwrite((char *) &header, 54,1,f);
    }

BitmapHeader_s header;


};

}

namespace png {

static void pngSetDefaultOptionsProgressive(spng_ctx* ctx, uint32_t width,uint32_t height, uint8_t bits = 16,float gamma = 1.0,uint8_t compressionLevel = PNG_COMPRESSION_LEVEL){

    struct spng_ihdr ihdr = {.width =width,.height=height,.bit_depth=bits,.color_type = SPNG_COLOR_TYPE_TRUECOLOR,.compression_method=0,.filter_method=0,.interlace_method=0}; /* zero-initialize to set valid defaults */
    spng_set_ihdr(ctx, &ihdr);
    spng_set_option(ctx,SPNG_IMG_COMPRESSION_LEVEL,compressionLevel);
    spng_set_option(ctx,SPNG_IMG_MEM_LEVEL,9); // Max memory
    spng_set_option(ctx,SPNG_FILTER_CHOICE,DEFAULT_FILTER);
    spng_phys phys = {52638,52638,1};
    spng_set_phys(ctx,&phys);
    //spng_set_srgb(ctx,0); // Gamma ignored if set
    spng_set_gama(ctx,gamma); // Set gamma for preview
    spng_encode_chunks(ctx);
    spng_encode_image(ctx, 0, 0, SPNG_FMT_PNG, SPNG_ENCODE_PROGRESSIVE | SPNG_ENCODE_FINALIZE); // Will finalize on last line

}

struct PngImageInfo{
    spng_phys phys;
    spng_ihdr ihdr;
};

static PngImageInfo getPngInfo(FILE* file){
    spng_ctx* ctx = spng_ctx_new(0);
    spng_set_png_file(ctx, file);

    PngImageInfo info;
    spng_get_ihdr(ctx,&info.ihdr);
    spng_get_phys(ctx,&info.phys);
    spng_ctx_free(ctx);
    return info;
}

// Display gamma used by SPNG_DECODE_GAMMA. Output = input^(1/(fileGamma*screenGamma))
static constexpr double screenGamma = 2.2;

// Reads the gAMA chunk of a png. Returns 0 if the file has none
static double getPngGamma(FILE* file){
    spng_ctx* ctx = spng_ctx_new(0);
    spng_set_png_file(ctx, file);
    double gamma = 0;
    if(spng_get_gama(ctx,&gamma) != SPNG_OK){
        gamma = 0;
    }
    spng_ctx_free(ctx);
    return gamma;
}

// Maps 16 bit linear samples to 8 bit display values with the same curve as SPNG_DECODE_GAMMA.
// spng reduces to 8 bit before applying the curve, this keeps the full input precision for the shadows
static std::vector<uint8_t> gammaLut16to8(double fileGamma){
    std::vector<uint8_t> lut(65536);
    const double exponent = 1.0 / (fileGamma * screenGamma);
    for(size_t i = 0;i<lut.size();i++){
        lut[i] = (uint8_t)std::min(255.0,std::round(std::pow((double)i / 65535.0,exponent) * 255.0));
    }
    return lut;
}

// Encodes a raw image buffer to png
static void rawToPng(FILE* infile,FILE* outfile,uint32_t height,uint32_t width,uint8_t bytesPerSample = 1,float gamma = 1.0,uint8_t compressionLevel = 0,std::optional<std::function<void(float)>> progresscb = std::nullopt){
    spng_ctx* ctx = spng_ctx_new(SPNG_CTX_ENCODER);
    spng_set_png_file(ctx, outfile);

    // Allocate line buffer
    int channels = 3;
    int bytesPerLine = width*bytesPerSample*channels;
    uint8_t* linebuf = (uint8_t*)std::malloc(bytesPerLine);

    png::pngSetDefaultOptionsProgressive(ctx,width,height,bytesPerSample*8,gamma,compressionLevel);
    spng_set_option(ctx,SPNG_IMG_COMPRESSION_LEVEL,compressionLevel);

    for(uint32_t line = 0;line<height;line++){
        // read linewidth*bytesPerSample*3
        size_t len = std::fread(linebuf,1,bytesPerLine,infile);
        int res = spng_encode_row(ctx, linebuf, bytesPerLine);
        if(res != SPNG_OK){
            std::cout << "PNG encode error: " << std::to_string(res) << std::endl;
            break;
        }
        if(progresscb.has_value()){
            (progresscb.value())( (float)line / (float)(height-1));
        }
    }
    std::free(linebuf);
    spng_ctx_free(ctx);
}

// Reencodes a png line by line with different options. Gamma correction not possible in all modes
static void pngReencode(FILE* infile,FILE* outfile,uint32_t downsample = 0,uint8_t compressionLevel = 0,spng_format format = SPNG_FMT_RGB8,bool gammacorrect = true,std::optional<std::function<void(float)>> progresscb = std::nullopt){
    unsigned bytes_per_pixel;
    uint8_t bits = 8;


    int step = downsample+1;
    //int channels = 3;

    // Decoder

    spng_ctx* inctx = spng_ctx_new(0);
    spng_set_png_file(inctx,infile);
    size_t limit = 1024 * 1024 * 64;
    spng_set_chunk_limits(inctx, limit, limit);

    int decodeflags = SPNG_DECODE_PROGRESSIVE;
    if(gammacorrect){
        std::cout << "Decode gamma on " << std::endl;
        decodeflags |= (int)SPNG_DECODE_GAMMA;
    }
    spng_decode_image(inctx,0,0,format,decodeflags);

    spng_ihdr ihdrin;
    spng_get_ihdr(inctx,&ihdrin);
    // spng_row_info row_info = {0};
    // spng_get_row_info(inctx, &row_info);
    uint32_t inlines = ihdrin.height;

    uint8_t channels = 3;
    switch(ihdrin.color_type){
    case SPNG_COLOR_TYPE_GRAYSCALE:
        channels = 1;
        break;
    case SPNG_COLOR_TYPE_INDEXED: //???
    case SPNG_COLOR_TYPE_TRUECOLOR:
        channels = 3;
        break;

    case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
        channels = 2;
        break;
    case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
        channels = 4;
        break;
    }

    switch(format)
    {
    case SPNG_FMT_RGBA8:
    case SPNG_FMT_GA16:
        bytes_per_pixel = 4;
        bits = 16;
        break;
    case SPNG_FMT_RGBA16:
        bytes_per_pixel = 8;
        bits = 8;
        break;
    case SPNG_FMT_RGB8:
        bytes_per_pixel = 3;
        bits = 8;
        break;
    case SPNG_FMT_GA8:
        bytes_per_pixel = 2;
        bits = 8;

        break;
    case SPNG_FMT_G8:
        bytes_per_pixel = 1;
        bits = 8;

        break;
    case SPNG_FMT_PNG:
    case SPNG_FMT_RAW:

    default:
        bytes_per_pixel = 1 * (ihdrin.bit_depth/8) * channels;
        bits = ihdrin.bit_depth;
        break;
    }


    /// Encoder
    spng_ctx* outctx = spng_ctx_new(SPNG_CTX_ENCODER);
    spng_set_png_file(outctx, outfile);

    uint32_t linelenout = ihdrin.width / step;
    struct spng_ihdr ihdr = {.width =linelenout,.height=inlines/step,.bit_depth=bits,.color_type = ihdrin.color_type,.compression_method=0,.filter_method=0,.interlace_method=0}; /* zero-initialize to set valid defaults */
    spng_set_ihdr(outctx, &ihdr);
    spng_set_option(outctx,SPNG_IMG_COMPRESSION_LEVEL,compressionLevel);
    spng_set_option(outctx,SPNG_IMG_MEM_LEVEL,9); // Max memory
    spng_phys phys = {52638,52638,1};
    spng_set_phys(outctx,&phys);
    if(!gammacorrect){
        double gm;
        if(spng_get_gama(inctx,&gm)==SPNG_OK){
            spng_set_gama(outctx,gm); // Set gamma for preview
        }

    }

    spng_encode_chunks(outctx);
    spng_encode_image(outctx, 0, 0, SPNG_FMT_PNG, SPNG_ENCODE_PROGRESSIVE | SPNG_ENCODE_FINALIZE); // Will finalize on last line


    /// Decode
    //uint8_t linebuf[ihdrin.width*bytes_per_pixel];
    uint8_t* linebuf = (uint8_t*)std::malloc(ihdrin.width * bytes_per_pixel * sizeof(uint8_t));
    if(linebuf == NULL){
        std::cout << "Can not allocate memory... failing " << std::endl;
        return;
    }


    std::cout << "Saving new png " << std::to_string(ihdr.width) << "x" << std::to_string(ihdr.height) << std::endl;
    for(uint32_t y = 0;y<inlines-1;y += 1){
        if(y % step != 0){
            spng_decode_scanline(inctx,linebuf,ihdrin.width*bytes_per_pixel); // Ugly hack... throw away all the data.

            //update_row_info(inctx); // increment row
            continue;
        }

        auto res = spng_decode_row(inctx,linebuf,ihdrin.width*bytes_per_pixel);

        // Downsample
        uint32_t linebufptr = 0;
        for(uint32_t x = 0 ; x<ihdrin.width*bytes_per_pixel ; x+= step*bytes_per_pixel){
            for(int i = 0;i<bytes_per_pixel;i++){
                linebuf[linebufptr++] = linebuf[x+i];
            }
        }
        //std::cout << std::to_string(linebuf[0]) << std::endl;
        auto encoderes = spng_encode_row(outctx, linebuf, linelenout*bytes_per_pixel); // TODO data transform
        if(progresscb.has_value()){
            (progresscb.value())( (float)y / (float)(inlines-2));
        }
        if(res != SPNG_OK){
            std::cout << "Done early?! " << spng_strerror(res) << " row " << std::to_string(y) << std::endl;
            break;
        }
        if(encoderes != SPNG_OK){
            std::cout << "Encode done early?! " << spng_strerror(encoderes) << " row " << std::to_string(y) << std::endl;
            break;
        }
    }

    spng_ctx_free(outctx);
    spng_ctx_free(inctx);
    std::free(linebuf);
}

}
#endif // IMAGEHELPERS_H
