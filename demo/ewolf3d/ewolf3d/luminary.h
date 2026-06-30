#define SYSCTL 0x400fe000
#define HWREG(x) (*((volatile uint32_t *)(x)))
#define GPIOA 0x40004000
#define GPIOB 0x40005000
#define GPIOC 0x40006000
#define GPIOD 0x40007000
#define GPIOE 0x40024000
#define GPIOF 0x40025000
#define GPIOG 0x40026000
#define SSI0  0x40008000
#define ENET  0x40048000

#define NVIC ((volatile int *)0xe000e000)
