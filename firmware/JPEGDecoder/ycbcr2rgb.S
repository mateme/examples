/*
 * ycbcr2rgb.S
 *
 *  Created on: 15-07-2014
 *      Author: Mateusz
 */

.cpu cortex-m4
.fpu softvfp
.syntax unified
.thumb
.text


.global ycbcr2rgbII

//R0-Y
//R1-Cb
//R2-Cr
//R0-Return value in format RGB565 or RGB888

.thumb_func
ycbcr2rgbII:

//PUSH {R3, R10, R11}
ADD R0, 128
LSL R0, R0, #8

//RED
SMLATB R10, R9, R2, R0 //R10=R2xR3+R0
USAT R10, #5, R10, ASR #11//#8

//GREEN
SMLATB R11, R8, R2, R0 //R10=R2xR3+R0
SMLABB R11, R8, R1, R11 //R10=R2xR3+R0
USAT R11, #6, R11, ASR #10//#8

MOVT R3, #0xFF49

PKHBT R2, R2, R1 //Writes bottom halfword of R2 to bottom halfword of R2, writes top halfword of R1, unshifted, to top halfword of R2
SMLAD R11, R3, R2, R0
USAT R11, #6, R11, ASR #10//#8*/

//BLUE

SMLABB R0, R9, R1, R0 //R10=R2xR3+R0
USAT R0, #5, R0, ASR #11//#8

//Convert to format RGB565
ORR.W R0, R0, R11, LSL #5
ORR.W R0, R0, R10, LSL #11

//POP {R3, R10, R11}
BX LR

.end
