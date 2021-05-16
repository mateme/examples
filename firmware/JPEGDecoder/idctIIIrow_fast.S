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

.global _idctIIIrow_fast

.thumb_func
_idctIIIrow_fast:

//PUSH {R1, R2, R3, R4, R5, R6, R7, R8, R9}
//PUSH {R1-R10}
//PUSH {LR}
//LDMH R0, {R1, R2, R3, R4, R5, R6, R7, R8}

LDRH R1, [R0, #0]
LDRH R2, [R0, #2]
LDRH R3, [R0, #4]
LDRH R4, [R0, #6]

LDRH R5, [R0, #8]
LDRH R6, [R0, #10]
LDRH R7, [R0, #12]
LDRH R8, [R0, #14]

//Check that all values after first sample are equal to zero
CBNZ R2, CalcFullDCT
CBNZ R3, CalcFullDCT
CBNZ R4, CalcFullDCT
CBNZ R5, CalcFullDCT
CBNZ R6, CalcFullDCT
CBNZ R7, CalcFullDCT

CBZ R1, AllSamplesAreZero

//MOV.W R9, #362
/*MOV.W R9, #128
SMULBB R1, R1, R9
ASR R1, R1 , #6 */
LSL R1, R1, #1

STRH R1, [R0, #0] //14
STRH R1, [R0, #2]
STRH R1, [R0, #4] //4
STRH R1, [R0, #6] //10
STRH R1, [R0, #8] //12
STRH R1, [R0, #10] //2
STRH R1, [R0, #12] //6
STRH R1, [R0, #14] //8

AllSamplesAreZero:
//POP {R1-R10}
BX LR

CalcFullDCT:
//STAGE 1
// 1/SQRT(2) = 0,707 2896/4096 (12 bits shift after multiplication)
MOV.W R9, #2896

//Process sample no. 3
SMULBB R3, R4, R9
//Process sample no. 5
SMULBB R6, R6, R9

//Process samples 1 and 7
MOV.W R9, R8
SUB R9, R2, R8
ADD R2, R8, R2

//STAGE 2
//Process samples no. 0 and 4
MOV.W R9, R5
SUB R5, R1, R5
ADD R1, R9, R1

//Process samples no. 1 and 3, shift right 12 bist sample 3 to complete multiplication from stage 1
MOV.W R9, R4, ASR #11
SUB R4, R2, R1 //, ASR #11
ADD R2, R1, R1 //, ASR #11

//Process samples no. 5 and 7
MOV.W R9, R6, ASR #11
SUB R6, R8, R9 //, ASR #11
ADD R8, R8, R9 //, ASR #11

//R4 contain T-sample 6, B-sample 2
PKHBT R1, R3, R7, LSL #16 //4 na 6
MOVW R8, #334 //167//167
MOVT R8, #138 //69 //69
SMUAD R7, R10, R9 //6
SMUSDX R3, R2, R9  //2

//STAGE3
//Process samples no. 0 and 6, sample 6 additional dividing by 256
MOV.W R9, R7, ASR #8
SUB R7, R1, R8
ADD R2, R1, R9

//Process samples no. 2 and 4, sample 2 additional dividing by 256
MOV.W R9, R3, ASR #8
SUB R3, R4, R9
ADD R5, R5, R9

//R4 contain T-sample 1, B-sample 7
PKHBT R10, R8, R2, LSL #16
MOV R9, #424 //212
MOVT R9, #284 //142
SMUADX R2, R10, R9 //1 ok...7?
SMUSD R8, R10, R9  //7... 1?

//R4 contain T-sample 3, B-sample 5
PKHBT R10, R4, R6, LSL #16
MOV R9, #500 //250
SMUSD R4, R10, R9  //3?

//STAGE 4
//Process samples no.  0 and 1
MOV.W R9, R2, ASR #8
RSB R2, R9, R1, LSL #1
ADD R1, R9, R1, LSL #1

//Process samples no. 4 and 5
MOV.W R9, R6, ASR #8
RSB R6, R9, R5, LSL #1
ADD R5, R9, R5, LSL #1

//Process samples no. 2 and 3
MOV.W R9, R4, ASR #8
ADD R3, R9, R3, LSL #1

//Process samples no. 6 and 7
MOV.W R9, R8, ASR #8
RSB R8, R9, R7, LSL #1
ADD R7, R9, R7, LSL #1


SMULBT R1, R1, R9
SMULBT R2, R2, R9
ASR R2, R2, #12*/
STRH R1, [R0, #0]
STRH R2, [R0, #14]

SMULBT R4, R4, R9
ASR R3, R3, #12
STRH R4, [R0, #10] //10


SMULBT R6, R6, R9
ASR R5, R5, #12
ASR R6, R6, #12*/
STRH R5, [R0, #2] //12
STRH R6, [R0, #12] //2


ASR R7, R7, #12
ASR R8, R8, #12*/
STRH R7, [R0, #6] //6
STRH R8, [R0, #8] //8

//POP {LR}
//POP {R1-R10}
BX LR

.end
