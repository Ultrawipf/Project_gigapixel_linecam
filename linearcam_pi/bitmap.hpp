#ifndef BITMAP_HPP
#define BITMAP_HPP
#include <fstream>    // std::*fstream
#include <memory>     // std::unique_ptr
#include <vector>
#include <cstdio>

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


#endif // BITMAP_HPP
