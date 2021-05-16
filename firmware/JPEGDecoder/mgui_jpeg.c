#include "mgui_jpeg.h"
#include <stdio.h>

extern void convertblock8X8(unsigned short *img, unsigned short *y, unsigned short *cb, unsigned short *cr);
extern void convertblock8X16(unsigned short *img, unsigned short *y, unsigned short *cb, unsigned short *cr);
extern void convertblock16X8(unsigned short *img, unsigned short *y, unsigned short *cb, unsigned short *cr);
extern void convertblock16X16(unsigned short *img, unsigned short *y, unsigned short *cb, unsigned short *cr);

//Easy way to give zig zag order
const unsigned char ZigZag[64] =
{
	0,	1,	8,	16,	9,	2,	3,	10,
	17,	24,	32,	25,	18,	11,	4,	5,
	12,	19,	26,	33,	40,	48,	41,	34,
	27,	20,	13,	6,	7,	14,	21,	28,
	35,	42,	49,	56,	57,	50,	43,	36,
	29,	22,	15,	23,	30,	37,	44,	51,
	58,	59,	52,	45,	38,	31,	39,	46,
	53,	60,	61,	54,	47,	55,	62,	63,
};

unsigned char *input;

static int DCPredictor[3];
static unsigned char *ptr;
static unsigned char byte, index;
static unsigned char JPEGSubsampling;// = JPEG_SUBSAMP_NONE;
static unsigned short JPEGImageWidth, JPEGImageHeight;
static unsigned short Xdensity, Ydensity, Xthumbnail, Ythumbnail;

static JPEGHuffman JPEGHuffmanTables[2][2];//[Table class][Table destination]
static JPEGScanHeader JPEGScanSequence[3];
static JPEGComponentHeader JPEGComponentHeaders[JPEG_COMPONENT_HEADER_SLOTS];

static unsigned short JPEGQuantizationTable[JPEG_QUANTTABLE_SLOTS][64];
static inline int NextBit(void);
static inline int GetValue(int bits);
static void DecodeMCU(int comp, signed short *buffer);
static inline int YCbCrToRGB(signed short y, signed short cb, signed short cr);

//Buffers for decoded part of MCU
static signed short BufferY[4][64];
static signed short BufferCb[64];
static signed short BufferCr[64];

static int MCUCounter;

static unsigned short GetHalfWord(void)
{
    register int tmp = *ptr++<<8;
    return tmp+*ptr++;
}
static unsigned short x,y;

int GUIJPEGDecode(const unsigned char *picture, int x, int y)
{
    int i, j, p, l, len, xs, ys;
    unsigned char unit, Tc, Th;
    unsigned short size, code;

    unsigned char bits[16];

    DCPredictor[0] = 0;
    DCPredictor[1] = 0;
    DCPredictor[2] = 0;

    xs = x;
    ys = y;

    JPEGSubsampling = JPEG_SUBSAMP_NONE;

    ptr = (unsigned char*)picture;

    //Check that first market is SOI (0xFFD8)
    if(*ptr++ != 0xFF)
        return MGUI_JPEG_ERR_NO_SOI_MARKER;

    if(*ptr++ != 0xD8)
        return MGUI_JPEG_ERR_NO_SOI_MARKER;

    for(;;)
    {
        if(*ptr++ != 0xFF)
            return MGUI_JPEG_ERR_NO_MARKER;

        switch(*ptr++)
        {
            case 0xE0://APP0 marker
                //Get size of this marker
                size = GetHalfWord();

                //Skip first letters JF...
                ptr += 2;

                if(*ptr++ != 'I')
                    return MGUI_JPEG_ERR_IN_MARKER;
                if(*ptr++ != 'F')
                    return MGUI_JPEG_ERR_IN_MARKER;
                if(*ptr++ != 0x00)
                    return MGUI_JPEG_ERR_IN_MARKER;
                if(*ptr++ > MGUI_JPEG_VER_H)
                    return MGUI_JPEG_ERR_UNSUPPORTED_VERSION;
                if(*ptr++ > MGUI_JPEG_VER_L)
                    return MGUI_JPEG_ERR_UNSUPPORTED_VERSION;

                unit = *ptr++;

                Xdensity  = GetHalfWord();
                Ydensity  = GetHalfWord();
                Xthumbnail = *ptr++;
                Ythumbnail = *ptr++;

                size -= 16;
                ptr += size;
            break;

            case 0xFE://Comment marker
            	size = GetHalfWord();

				//Here you can read comment to some array...
				//...print out
				//...save
				//...or something else:)

                ptr += (size-2);
            break;

            case 0xE1://APP1 marker
            case 0xE2:
            case 0xE3:
            case 0xE4:
            case 0xE5:
            case 0xE6:
            case 0xE7:
            case 0xE8:
            case 0xE9:
            case 0xEA:
            case 0xEB:
            case 0xEC:
            case 0xED:
            case 0xEE:
            case 0xEF:
                  //Get size of this marker
                i = *--ptr;
                ptr++;

                size  = GetHalfWord();

                size -= 2;
                ptr += size;
            break;

            case 0xDB://Define quantization table(s) marker
                //Get size of this marker
                size  = GetHalfWord();

                for(len=size-2;len>0; len -= 65)
                {
                    //For 1 table should be 16bits width
                    p = *ptr>>4;
                    //j contain index of quantization table, we should check that we can save to this location
                    j = *ptr++&0x0F;

                    if(j > (JPEG_QUANTTABLE_SLOTS-1))
                        return MGUI_JPEG_ERR_NOMORE_QUANT_SLOTS;

                    if(!p)
                    {
                        for(i=0; i < 64;)
                        {
                            JPEGQuantizationTable[j][i++] = *ptr++;
                            JPEGQuantizationTable[j][i++] = *ptr++;
                            JPEGQuantizationTable[j][i++] = *ptr++;
                            JPEGQuantizationTable[j][i++] = *ptr++;
                            JPEGQuantizationTable[j][i++] = *ptr++;
                            JPEGQuantizationTable[j][i++] = *ptr++;
                            JPEGQuantizationTable[j][i++] = *ptr++;
                            JPEGQuantizationTable[j][i++] = *ptr++;
                        }
                    }
                    else
                        return MGUI_JPEG_ERR_INVALID_QUANT_SIZE;
                }
            break;

            case 0xD8:
                ptr++;
            break;

            case 0xC0://Start of frame marker Baseline DCT
                //Get size of this marker
                size  = GetHalfWord();

                p = *ptr++;

                //Get image dimensions
                JPEGImageHeight  = GetHalfWord();
                JPEGImageWidth   = GetHalfWord();

                //Number of components in frame
                j = *ptr++;

                if(j != JPEG_COMPONENT_HEADER_SLOTS)
                    return MGUI_JPEG_ERR_NOMORE_COMP_SLOTS;

                for(i=0; i < j; ++i)
                {
                    p = *ptr++;

                    if(p != (i+1))
                        return MGUI_JPEG_ERR_NOMORE_COMP_SLOTS;

                    JPEGComponentHeaders[i].Hi = (*ptr&0xF0)>>4;
                    JPEGComponentHeaders[i].Vi = (*ptr++)&0x0F;
                    JPEGComponentHeaders[i].Qi = *ptr++;
                }

                if(JPEGComponentHeaders[0].Hi == 2 && JPEGComponentHeaders[0].Vi == 2 && JPEGComponentHeaders[1].Hi == 1 && JPEGComponentHeaders[1].Vi == 1)
                    JPEGSubsampling = JPEG_SUBSAMP_16X16;
                else if(JPEGComponentHeaders[0].Hi == 1 && JPEGComponentHeaders[0].Vi == 1 && JPEGComponentHeaders[1].Hi == 1 && JPEGComponentHeaders[1].Vi == 1)
                    JPEGSubsampling = JPEG_SUBSAMP_8X8;
            break;

            case 0xC4://Define huffman table marker
                //Get size of this marker
                size  = GetHalfWord();

                for(len=size-2;len>0;)
                {
                    //Get table slot
                    Tc = *ptr>>4;
                    Th = *ptr++&0x0F;

                    if(Th > 1)
                        return MGUI_JPEG_ERR_NOMORE_HUFF_SLOTS;

                    //Get number of codes
                    for(i=0; i<16; ++i)
                        bits[i] = *ptr++;

                    p = 0;
                    code = 0;
                    len -= 17;

                    //Tc-ACDC    Th-table for defined type
                    for(i=0; i<16; ++i)
                    {
                        l = bits[i];

                        if(l == 0)
                            JPEGHuffmanTables[Tc][Th].MaxCode[i] = -1;
                        else
                        {
                            JPEGHuffmanTables[Tc][Th].ValPtr[i]  = p;
                            JPEGHuffmanTables[Tc][Th].MinCode[i] = code;

                            for(j=0; j<l; ++j)
                            {
                                JPEGHuffmanTables[Tc][Th].Symbol[p++] = *ptr++;
                                ++code;
                                --len;
                            }

                            //Change LSB in code from 1 to 0
                            JPEGHuffmanTables[Tc][Th].MaxCode[i] = code - 1;
                        }

                        code <<= 1;
                    }
                }
            break;

            case 0xDA://Start of scan marker
                //Get size of this marker
                size  = GetHalfWord();

                if(size != 12)
                    return MGUI_JPEG_ERR_SOS_MARKER;
                if(*ptr++ != 3)
                    return MGUI_JPEG_ERR_SOS_MARKER;

                for(i=0; i<3; ++i)
                {
                    if((i+1) != *ptr++)
                        return MGUI_JPEG_ERR_SOS_MARKER;

                    j = *ptr++;

                    JPEGScanSequence[i].Tdj = j>>4;
                    JPEGScanSequence[i].Taj = j&0x0F;
                }

                j = (int)*ptr++;
                j += (int)*ptr++;
                j += (int)*ptr++;

                if(j != 63)
                    return MGUI_JPEG_ERR_SOS_MARKER;

                goto EncodeImage;
            break;

            case 0xD9://EOI marker
                return MGUI_JPEG_SUCCESS;
            break;

            default:
                return MGUI_JPEG_ERR_UNSUPPORTED_MARKER;
            break;
        }
    }

    EncodeImage:
        //mscounter = 0;

    switch(JPEGSubsampling)
    {
        case JPEG_SUBSAMP_8X8:
            for(y=0; y<(JPEGImageHeight>>3); ++y)
            {
                for(x=0; x<(JPEGImageWidth>>3);++x)
                {
                    DecodeMCU(0, &BufferY[0][0]);//Y
                    DecodeMCU(1, BufferCb);//Cb
                    DecodeMCU(2, BufferCr);//Cr

                    _idctIII_fast(&BufferY[0][0]);
                    _idctIII_fast(BufferCb);
                    _idctIII_fast(BufferCr);
                }
            }
        break;

        case JPEG_SUBSAMP_8X16:
            //...modify convertblock8X16 function
        break;

        case JPEG_SUBSAMP_16X8:
            //...modify convertblock16X8 function
        break;

        case JPEG_SUBSAMP_16X16:
            for(y=0; y<(JPEGImageHeight/8); y+=2)
            {
                for(x=0; x<(JPEGImageWidth/8);x += 2)
                {
                    int k;
                    unsigned short IMG[64];
                    MCUCounter += 4;

                    DecodeMCU(0, &BufferY[0][0]);//Y
                    DecodeMCU(0, &BufferY[1][0]);//Y
                    DecodeMCU(0, &BufferY[2][0]);//Y
                    DecodeMCU(0, &BufferY[3][0]);//Y
                    DecodeMCU(1, BufferCb);//Cb
                    DecodeMCU(2, BufferCr);//Cr

                    _idctIII_fast(BufferCb);
                    _idctIII_fast(BufferCr);
					_idctIII_fast(&BufferY[0][0]);

                    convertblock16X16(IMG, &BufferY[0][0], &BufferCb[0], &BufferCr[0]);

                    LCD_WriteImage2(x*8+xs, y*8+ys, 8, 8, IMG);
                    _idctIII_fast(&BufferY[1][0]);

                    convertblock16X16(IMG, &BufferY[1][0], &BufferCb[4], &BufferCr[4]);
                    LCD_WriteImage2(x*8+8+xs, y*8+ys, 8, 8, IMG);
                    _idctIII_fast(&BufferY[2][0]);

                    convertblock16X16(IMG, &BufferY[2][0], &BufferCb[32], &BufferCr[32]);

                    LCD_WriteImage2(x*8+xs, y*8+8+ys, 8, 8, IMG);
                    _idctIII_fast(&BufferY[3][0]);

                    convertblock16X16(IMG, &BufferY[3][0], &BufferCb[36], &BufferCr[36]);

                    LCD_WriteImage2(x*8+8+xs, y*8+8+ys, 8, 8, IMG);
                }

            }
            index = 0;
        break;

        default:

        break;
    }
}

static inline int NextBit(void)
{
    if(index--)
    {
        byte <<= 1;
        return (byte>>7);//bitbanding?
    }
    else
    {
        byte  = *ptr++;
        index = 7;

        if(byte == 0xFF)
        {
            if(*ptr++ != 0x00)
            {
                if(*ptr == 0xD9)
                    ++ptr;
                else
                {
                    while(1){};
                }
            }
        }

        return (byte>>7);//bitbanding?
    }
}

static inline int GetValue(int bits)
{
    register int i;
    int val = 0;

    for(i=0; i<bits; ++i)
        	val = (val<<1) | NextBit();

	if(val < (1 << (bits - 1)))
		val += (-1 << bits) + 1;

    return val;
}

void convertblock(unsigned short *img, unsigned short *y, unsigned short *cb, unsigned short *cr)
{
    //STAGE 1
    *(img+0) = ycbcr2rgbII(*(y+0), *cb, *cr);
    *(img+1) = ycbcr2rgbII(*(y+1), *cb, *cr);
    *(img+8) = ycbcr2rgbII(*(y+8), *cb, *cr);
    *(img+9) = ycbcr2rgbII(*(y+9), *cb, *cr);

    *(img+2) = ycbcr2rgbII(*(y+2), *(cb+1), *(cr+1));
    *(img+3) = ycbcr2rgbII(*(y+3), *(cb+1), *(cr+1));
    *(img+10) = ycbcr2rgbII(*(y+10), *(cb+1), *(cr+1));
    *(img+11) = ycbcr2rgbII(*(y+11), *(cb+1), *(cr+1));

    *(img+4) = ycbcr2rgbII(*(y+4), *(cb+2), *(cr+2));
    *(img+5) = ycbcr2rgbII(*(y+5), *(cb+2), *(cr+2));
    *(img+12) = ycbcr2rgbII(*(y+12), *(cb+2), *(cr+2));
    *(img+13) = ycbcr2rgbII(*(y+13), *(cb+2), *(cr+2));

    *(img+6) = ycbcr2rgbII(*(y+6), *(cb+3), *(cr+3));
    *(img+7) = ycbcr2rgbII(*(y+7), *(cb+3), *(cr+3));
    *(img+14) = ycbcr2rgbII(*(y+14), *(cb+3), *(cr+3));
    *(img+15) = ycbcr2rgbII(*(y+15), *(cb+3), *(cr+3));

    //STAGE 2
    *(img+16) = ycbcr2rgbII(*(y+16), *(cb+8), *(cr+8));
    *(img+17) = ycbcr2rgbII(*(y+17), *(cb+8), *(cr+8));
    *(img+24) = ycbcr2rgbII(*(y+24), *(cb+8), *(cr+8));
    *(img+25) = ycbcr2rgbII(*(y+25), *(cb+8), *(cr+8));

    *(img+18) = ycbcr2rgbII(*(y+18), *(cb+9), *(cr+9));
    *(img+19) = ycbcr2rgbII(*(y+19), *(cb+9), *(cr+9));
    *(img+26) = ycbcr2rgbII(*(y+26), *(cb+9), *(cr+9));
    *(img+27) = ycbcr2rgbII(*(y+27), *(cb+9), *(cr+9));

    *(img+20) = ycbcr2rgbII(*(y+20), *(cb+10), *(cr+10));
    *(img+21) = ycbcr2rgbII(*(y+21), *(cb+10), *(cr+10));
    *(img+28) = ycbcr2rgbII(*(y+28), *(cb+10), *(cr+10));
    *(img+29) = ycbcr2rgbII(*(y+29), *(cb+10), *(cr+10));

    *(img+22) = ycbcr2rgbII(*(y+22), *(cb+11), *(cr+11));
    *(img+23) = ycbcr2rgbII(*(y+23), *(cb+11), *(cr+11));
    *(img+30) = ycbcr2rgbII(*(y+30), *(cb+11), *(cr+11));
    *(img+31) = ycbcr2rgbII(*(y+31), *(cb+11), *(cr+11));

    //STAGE 3
    *(img+32) = ycbcr2rgbII(*(y+32), *(cb+16), *(cr+16));
    *(img+33) = ycbcr2rgbII(*(y+33), *(cb+16), *(cr+16));
    *(img+40) = ycbcr2rgbII(*(y+40), *(cb+16), *(cr+16));
    *(img+41) = ycbcr2rgbII(*(y+41), *(cb+16), *(cr+16));

    *(img+34) = ycbcr2rgbII(*(y+34), *(cb+17), *(cr+17));
    *(img+35) = ycbcr2rgbII(*(y+35), *(cb+17), *(cr+17));
    *(img+42) = ycbcr2rgbII(*(y+42), *(cb+17), *(cr+17));
    *(img+43) = ycbcr2rgbII(*(y+43), *(cb+17), *(cr+17));

    *(img+36) = ycbcr2rgbII(*(y+36), *(cb+18), *(cr+18));
    *(img+37) = ycbcr2rgbII(*(y+37), *(cb+18), *(cr+18));
    *(img+44) = ycbcr2rgbII(*(y+44), *(cb+18), *(cr+18));
    *(img+45) = ycbcr2rgbII(*(y+45), *(cb+18), *(cr+18));

    *(img+38) = ycbcr2rgbII(*(y+38), *(cb+19), *(cr+19));
    *(img+39) = ycbcr2rgbII(*(y+39), *(cb+19), *(cr+19));
    *(img+46) = ycbcr2rgbII(*(y+46), *(cb+19), *(cr+19));
    *(img+47) = ycbcr2rgbII(*(y+47), *(cb+19), *(cr+19));

    //STAGE 4
    *(img+48) = ycbcr2rgbII(*(y+48), *(cb+24), *(cr+24));
    *(img+49) = ycbcr2rgbII(*(y+49), *(cb+24), *(cr+24));
    *(img+56) = ycbcr2rgbII(*(y+56), *(cb+24), *(cr+24));
    *(img+57) = ycbcr2rgbII(*(y+57), *(cb+24), *(cr+24));

    *(img+50) = ycbcr2rgbII(*(y+50), *(cb+25), *(cr+25));
    *(img+51) = ycbcr2rgbII(*(y+51), *(cb+25), *(cr+25));
    *(img+58) = ycbcr2rgbII(*(y+58), *(cb+25), *(cr+25));
    *(img+59) = ycbcr2rgbII(*(y+59), *(cb+25), *(cr+25));

    *(img+52) = ycbcr2rgbII(*(y+52), *(cb+26), *(cr+26));
    *(img+53) = ycbcr2rgbII(*(y+53), *(cb+26), *(cr+26));
    *(img+60) = ycbcr2rgbII(*(y+60), *(cb+26), *(cr+26));
    *(img+61) = ycbcr2rgbII(*(y+61), *(cb+26), *(cr+26));

    *(img+54) = ycbcr2rgbII(*(y+54), *(cb+27), *(cr+27));
    *(img+55) = ycbcr2rgbII(*(y+55), *(cb+27), *(cr+27));
    *(img+62) = ycbcr2rgbII(*(y+62), *(cb+27), *(cr+27));
    *(img+63) = ycbcr2rgbII(*(y+63), *(cb+27), *(cr+27));
}

void DecodeMCU(int comp, signed short *buffer)
{
    int i, k, j;
    unsigned int val;
    signed short *bptr = buffer;
    signed short code;
    //Conv(i, &code, buffer, buffer);

    code = NextBit();

    for(i=0; i<64; ++i)
        *bptr++ = 0;

    for(i=0; i<16; ++i)
    {
        //JPEGHuffmanTables[0] - first in scan is DC component, Tdj have DC huffman table
        if(code <= JPEGHuffmanTables[0][JPEGScanSequence[comp].Tdj].MaxCode[i])
        {
            //obtain code
            /*j = JPEGHuffmanTables[0][JPEGScanSequence[comp].Tdj].ValPtr[i];
            j = j+code-JPEGHuffmanTables[0][JPEGScanSequence[comp].Tdj].MinCode[i];
            val = JPEGHuffmanTables[0][JPEGScanSequence[comp].Tdj].Symbol[j];*/

            code -= JPEGHuffmanTables[0][JPEGScanSequence[comp].Tdj].MinCode[i];
            val   = JPEGHuffmanTables[0][JPEGScanSequence[comp].Tdj].Symbol[JPEGHuffmanTables[0][JPEGScanSequence[comp].Tdj].ValPtr[i]+code];

            break;
        }
        code = (code << 1) | NextBit();
    }

    //k = getval(2);
    k = val ? GetValue(val) : 0;

    *buffer = k*(int)JPEGQuantizationTable[JPEGComponentHeaders[comp].Qi][0]+DCPredictor[comp];
    DCPredictor[comp] = *buffer;

    for(k=1; k<64; ++k)
    {
        code = NextBit();
        for(i=0; i<16; ++i)
        {
            //JPEGHuffmanTables[] - second in scan are AC components, Taj have AC huffman table
            if(code <= JPEGHuffmanTables[1][JPEGScanSequence[comp].Taj].MaxCode[i])
            {
                //obtain code
                /*j = JPEGHuffmanTables[1][JPEGScanSequence[comp].Taj].ValPtr[i];
                j = j+code-JPEGHuffmanTables[1][JPEGScanSequence[comp].Taj].MinCode[i];
                val = JPEGHuffmanTables[1][JPEGScanSequence[comp].Taj].Symbol[j];*/
                code -= JPEGHuffmanTables[1][JPEGScanSequence[comp].Taj].MinCode[i];
                val   = JPEGHuffmanTables[1][JPEGScanSequence[comp].Taj].Symbol[JPEGHuffmanTables[1][JPEGScanSequence[comp].Taj].ValPtr[i]+code];

                //j   = (val>>4)&0x0F;
                j   = val>>4;
                val = val&0x0F;

                break;
            }
            code = (code << 1) | NextBit();
        }

        //Skip zero values if it's necessary
        //if(j > 15)
        k += j;

        if(!val)
        {
			if(j == 15)
				continue;
			else
				break;
        }

        *(buffer+ZigZag[k]) = GetValue(val)*JPEGQuantizationTable[JPEGComponentHeaders[comp].Qi][k];
    }
}
