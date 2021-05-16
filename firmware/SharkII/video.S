#define __SFR_OFFSET 0
#define __AVR_ATMEGA32__
#include <avr/io.h>
#include <avr/sfr_defs.h>

.EQU SYNC,  0x00
.EQU BLACK, 0x40
.EQU GRAY,  0x80
.EQU WHITE, 0xC0

.EQU VerticalSync , 0
.EQU NormalLine   , 1
.EQU EmptyLinePrev, 2
.EQU EmptyLinePost, 3

.global Action
.comm Action, 1

.global BufferAddress
.comm BufferAddress, 2

.global VerticalMul
.comm VerticalMul, 1

.global EmptyLinePrv
.comm EmptyLinePrv, 1

.global EmptyLinePst
.comm EmptyLinePst, 1

.global Line
.comm Line, 1

.global Buffer
.comm Buffer, 1752

.global TimerTick
.comm TimerTick, 1

.global TimerTick2
.comm TimerTick2, 1

.global PadStatus
.comm PadStatus, 1

.global ChannelATime
.comm ChannelATime, 1

.global __vector_11
__vector_11:

push r0
in   r0, 0x3F
push r16
push r17
push r26
push r27

ldi r16, 129
out TCNT0, r16//9
ldi r16, BLACK
out PORTA, r16//11

//Sprawdzenie jaka akcje mamy do wykonania
lds r17, Action//13

cpi r17, EmptyLinePost ;1
breq _EmptyLinePostPre ;1
cpi r17, VerticalSync
breq _VerticalSyncPre
cpi r17, NormalLine
breq _NormalLine

rjmp _EmptyLinePrev

_VerticalSyncPre:
	rjmp _VerticalSync

_EmptyLinePostPre:
	rjmp _EmptyLinePost ;2
	
_PreNormalLine:
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop//18 nopow
	nop

nop
nop
nop
nop
_NormalLine://18


	ldi r16, SYNC
	out PORTA, r16//w 22 cyklu od wektora przerwan+4przejscia do przerwania = 26cyc=1625ns~1,65us
	
	ldi r16, 132
	out TCNT0, r16	
	
	ldi r16, 22
	waitloop4_7us:	
		dec	r16
	brne waitloop4_7us
	nop
	
	
	//BackPorch 5.6us kolor czarny
	ldi r16, BLACK
	out PORTA, r16

	nop
	nop
	nop
	lds r18, Line
	inc r18	
	
	ldi r16, 49
	waitloop5_6us:
		dec r16
	brne waitloop5_6us
		
	/////////////////Linia///////////////////
	cpi r18, 10;1+
	brcc NormalVideo;2
		ldi  r26, lo8(Buffer+1728)
		ldi  r27, hi8(Buffer+1729)
		nop
		rjmp NoVideo
	NormalVideo:
		lds  r26, BufferAddress
		lds  r27, BufferAddress+1
	NoVideo:
		ldi r17, 12
		
	nop
	nop

	_DrawLine:
		ld r16, X+ ;2
		nop;1
		nop;1///----4
		
		out PORTA, r16 ;1
		lsl r16 ;1
		lsl r16 ;1
		nop
		nop
		nop
		nop///-------11

		out PORTA, r16 ;1
		lsl r16 ;1
		lsl r16 ;1		
		nop
		nop
		nop
		nop//--18

		out PORTA, r16 ;1
		lsl r16 ;1
		lsl r16 ;1
		nop
		nop
		nop
		nop//-----25

		out PORTA, r16 ;1

		ld r16, X+ ;2
		nop
		nop
		nop
		nop//------32
		
		
		out PORTA, r16 ;1
		lsl r16 ;1
		lsl r16 ;1
		nop
		nop
		nop
		nop//--39

		out PORTA, r16 ;1
		lsl r16 ;1
		lsl r16 ;1		
		nop
		nop
		nop
		nop//--46

		out PORTA, r16 ;1
		lsl r16 ;1
		lsl r16 ;1
		nop
		nop
		nop
		nop//---52

		out PORTA, r16 ;1

		dec r17 ; 1 ; 54 od poczatku//--54
	brne _DrawLine

	ldi r16, BLACK;1
	nop
	nop
	out PORTA, r16;1
	
	cpi r18, 10;1
	brcc _NormalRun;2  
		nop
		nop
		nop
		nop
		nop
		rjmp _NoChangeBufferAddress
	_NormalRun:
		lds r17, VerticalMul;2
		inc r17;1///////////////////////////////////++6cycles
	/////933
	//##Kwik
	cpi r17, 3 ;1
	brne _NoChangeBufferAddress ;1/2
		sts BufferAddress, r26;2
		sts BufferAddress+1, r27;2
		ldi r17, 0;1
		sts VerticalMul, r17;2
		rjmp _NoExtraDelay
		
	_NoChangeBufferAddress:
		sts VerticalMul, r17;2
		nop
		nop
		nop
		nop
		nop
		nop
		//////////////////////////////+11cycles
		////////////944
	_NoExtraDelay:
		sts Line, r18;2
		cpi r18, 225;1
		brne _NoChangeAction;2
		ldi r17, EmptyLinePost
		sts Action, r17
		rjmp _End
		
	_NoChangeAction://++5 cycles//949
		ldi r16, 18;1//<-1 cycle /// bylo 28!!!!!!!!!!1+++6/20!!!!!!!!!!!!!!!!
		waitloop11_7us://///////+++
			dec r16;1
		brne waitloop11_7us;2///+++17*3+2=50
		//nop// nop do wyrownania
		nop
		nop
		rjmp _PreNormalLine;2//2 cycles
	
_VerticalSync://20
    //ldi r16, BLACK
	//out PORTA, r16
	//spraw synca

	//nop
	//nop
		ldi r16, SYNC
	out PORTA, r16//22
	// Przeladowanie timera, aby uzyskac 128us
	
	lds r16, TimerTick
	inc r16
	sts TimerTick, r16

	//tutaj sync
	nop
	nop
	nop
	ldi r16, SYNC
	out PORTA, r16//22
	
	sts Line, r16
	sts EmptyLinePrv, r16
	sts EmptyLinePst, r16
	
	lds r16, TimerTick2
	inc r16
	sts TimerTick2, r16
	
	ldi r16, EmptyLinePrev
	sts Action, r16
	ldi r16, 0xFF
	out SPDR, r16
		
	//***Sounds control***//
	lds r16, ChannelATime
	cpi r16, 0
	breq NoSound
		dec r16
		sts ChannelATime, r16
		ldi r16, 0x0D
		out TCCR1B, r16
		rjmp _End
	NoSound:
		ldi r16, 0x08
		out TCCR1B, r16
	//*********************//
	
	rjmp _End
_EmptyLinePost:

	sbi PORTB, 0

	nop
	ldi r16, SYNC
	out PORTA, r16

	ldi r16, 23
	waitloop4_7usel:	
		dec	r16
		brne waitloop4_7usel
		
	ldi r16, BLACK
	out PORTA, r16

	lds r18, EmptyLinePst
	inc r18
	sts EmptyLinePst, r18
	cpi r18, 36
	brne _NoChangeActionPst
	ldi r17, VerticalSync
	sts Action, r17
	cbi PORTB, 0
	_NoChangeActionPst:
		
	rjmp _End
	
_EmptyLinePrev:
nop
//nop
	ldi r16, SYNC

	ldi r16, 23
	waitloop4_7uselp:	
		dec	r16
		brne waitloop4_7uselp
		
	ldi r16, BLACK
	out PORTA, r16

	lds r18, EmptyLinePrv
	inc r18
	sts EmptyLinePrv, r18
	cpi r18, 49
	brne _NoChangeActionPrv
	ldi r17, NormalLine
	sts Action, r17
	in r17, SPDR
	sts PadStatus, r17
	_NoChangeActionPrv:

_End:

	pop r27
	pop r26
	pop r18
	pop r17
	pop r16
	out	0x3F, r0
	pop	r0

reti