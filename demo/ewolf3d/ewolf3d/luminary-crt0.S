	.text
	.thumb
	.syntax unified
	.global _start
	.type _start, %function
_start:
	ldr	sp, =_stack
	ldr	r0, =__data_load
	ldr	r1, =__data_start
	ldr	r2, =_edata
	/* Copy .data to ram.  */
1:
	ldr	r3, [r0], #4
	str	r3, [r1], #4
	cmp	r1, r2
	bne	1b
	/* Clear .bss */
	ldr	r2, =__bss_end__
	mov	r3, #0
1:
	str	r3, [r1], #4
	cmp	r1, r2
	bne	1b
	
	mov	r0, #0		/*  no arguments  */
	mov	r1, #0		/*  no argv either */

	bl	main

	b .
.pool
.global isr_handler
.type isr_handler, %function
isr_handler:
	b .

.section ".isr", "a", %progbits
.global __isr_vectors
__isr_vectors:
/* System vectors.  */
.word _stack
.word _start
.word isr_handler /* 2*/
.word isr_handler /* 3*/
.word isr_handler /* 4*/
.word isr_handler /* 5*/
.word isr_handler /* 6*/
.word isr_handler /* 7*/
.word isr_handler /* 8*/
.word isr_handler /* 9*/
.word isr_handler /*10*/
.word isr_handler /*11*/
.word isr_handler /*12*/
.word isr_handler /*13*/
.word isr_handler /*14*/
.word isr_handler /*15*/
/* External interrupts.  */
.word isr_handler /* 0*/
.word isr_handler /* 1*/
.word isr_handler /* 2*/
.word isr_handler /* 3*/
.word isr_handler /* 4*/
.word isr_handler /* 5*/
.word isr_handler /* 6*/
.word ssi_isr /* 7*/
.word isr_handler /* 8*/
.word isr_handler /* 9*/
.word isr_handler /*10*/
.word isr_handler /*11*/
.word isr_handler /*12*/
.word isr_handler /*13*/
.word isr_handler /*14*/
.word isr_handler /*15*/
.word isr_handler /*16*/
.word isr_handler /*17*/
.word isr_handler /*18*/
.word timer_isr /*19*/
.word isr_handler /*20*/
.word isr_handler /*21*/
.word isr_handler /*22*/
.word isr_handler /*23*/
.word isr_handler /*24*/
.word isr_handler /*25*/
.word isr_handler /*26*/
.word isr_handler /*27*/
.word isr_handler /*28*/
.word isr_handler /*29*/
.word isr_handler /*30*/
.word isr_handler /*31*/
.word isr_handler /*32*/
.word isr_handler /*33*/
.word isr_handler /*34*/
.word isr_handler /*35*/
.word isr_handler /*36*/
.word isr_handler /*37*/
.word isr_handler /*38*/
.word isr_handler /*39*/
.word isr_handler /*40*/
.word isr_handler /*41*/
.word isr_handler /*42*/
/*.word ethernet_isr *//*42*/
.word isr_handler /*43*/
.word isr_handler /*44*/
.word isr_handler /*45*/
.word isr_handler /*46*/
.word isr_handler /*47*/

.bss
_stack_end:
.space 1024
_stack:

