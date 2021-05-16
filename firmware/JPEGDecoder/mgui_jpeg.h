#ifndef MGUI_JPEG_H_INCLUDED
    #define MGUI_JPEG_H_INCLUDED

    #include "mgui_conf.h"

    #if MGUI_USE_JPEG > 0
        #include "mgui.h"

    //Decoder resources
    #define MGUI_JPEG_VER_H 0x01
    #define MGUI_JPEG_VER_L 0x02

    #define MGUI_JPEG_SUCCESS                  0
    #define MGUI_JPEG_ERR_NO_MARKER           -1
    #define MGUI_JPEG_ERR_NO_SOI_MARKER       -2
    #define MGUI_JPEG_ERR_UNSUPPORTED_MARKER  -3
    #define MGUI_JPEG_ERR_IN_MARKER           -4
    #define MGUI_JPEG_ERR_UNSUPPORTED_VERSION -5
    #define MGUI_JPEG_ERR_NOMORE_QUANT_SLOTS  -6
    #define MGUI_JPEG_ERR_NOMORE_COMP_SLOTS   -7
    #define MGUI_JPEG_ERR_NOMORE_HUFF_SLOTS   -8
    #define MGUI_JPEG_ERR_SOS_MARKER          -9
    #define MGUI_JPEG_ERR_INVALID_SUBSAMP     -10
    #define MGUI_JPEG_ERR_INVALID_QUANT_SIZE  -11

    #define JPEG_COMPONENT_HEADER_SLOTS        3
    #define JPEG_QUANTTABLE_SLOTS              4

    #define JPEG_SUBSAMP_NONE   0
    #define JPEG_SUBSAMP_8X8    1
    #define JPEG_SUBSAMP_8X16   2
    #define JPEG_SUBSAMP_16X8   3
    #define JPEG_SUBSAMP_16X16  4

    unsigned char *input;

    typedef struct{
        unsigned char Hi;
        unsigned char Vi;
        unsigned char Qi;
    }JPEGComponentHeader;

    typedef struct{
        unsigned char ValPtr[16];
        signed short MinCode[16];
        signed short MaxCode[16];
        unsigned char Symbol[255];
    }JPEGHuffman;

    typedef struct{
        unsigned char Tdj;
        unsigned char Taj;
    }JPEGScanHeader;

    int GUIJPEGDecode(const unsigned char *picture, int x, int y);
    #endif
#endif
