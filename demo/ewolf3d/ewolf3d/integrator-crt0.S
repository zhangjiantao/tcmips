/* .text is used instead of .section .text so it works with arm-aout too.  */
	.text
	.global _start
	.type _start, %function
_start:
	/*  Issue Angel SWI to read stack info */
	mov	r0, #0x16
	adr	r1, .LC0	/*  point at ptr to 4 words to receive data */
	swi 0x123456
	ldr	r0, .LC0	/*  point at values read */
	ldr	r1, [r0, #8]
	mov	sp, r1
	ldr	r1, [r0, #12]
	
	/* Zero the memory in the .bss section.  */
	mov 	a2, #0			/* Second arg: fill value */
	mov	fp, a2			/* Null frame pointer */
	mov	r7, a2			/* Null frame pointer for Thumb */
	
	ldr	a1, .LC1		/* First arg: start of memory block */
	ldr	a3, .LC2	
	sub	a3, a3, a1		/* Third arg: length of block */
	
	bl	memset
	mov	r0, #0		/*  no arguments  */
	mov	r1, #0		/*  no argv either */

	bl	main

	b .

.LC0:
	.word	__heap_base__
.LC1:
	.word	__bss_start__
.LC2:
	.word	__bss_end__
/*  Workspace for Angel calls.  */
	.data
/*  Data returned by monitor SWI.  */
.global	__stack_base__
.global __heap_limit__
.global __heap_base__
__heap_base__:	.word	0
__heap_limit__:	.word	0
__stack_base__:	.word	0
StackLimit:	.word	0
