
#define DTCM_DATA  __attribute__((section(".dtcmram_bss")))
#define ITCM_FUNCTION __attribute__((long_call, section(".itcm_text")))

#define GP_SPEED GPIO_SPEED_FREQ_LOW
#define LOG_EEPROM_BYTES 1

// Set the read speed delay.
#define READ_DELAY_NS 1000
//#define READ_DELAY_NS 500

#define N64_ROM_BASE              0x10000000
#define CART_DOM2_ADDR1_START     0x05000000
#define CART_DOM2_ADDR1_END       0x05FFFFFF

// N64DD IPL ROM
#define CART_DOM1_ADDR1_START     0x06000000
#define CART_DOM1_ADDR1_END       0x07FFFFFF

// Cartridge SRAM
#define CART_DOM2_ADDR2_START     0x08000000
#define CART_DOM2_ADDR2_END       0x0FFFFFFF

// Port B
#define WRITE_LINE (1 << 1)
#define ALE_H (1 << 12)

// Port C
#define ALE_L (1 << 0)
#define READ_LINE (1 << 1)
#define S_DAT_LINE (1 << 4)

// Port D
#define RESET_LINE (1 << 11)

// Port G
#define CIC_D1 (1 << 9)
#define CIC_CLK (1 << 10)
#define N64_NMI (1 << 11)

#define ALE_H_IS_HIGH ((GPIOD->IDR & ALE_H) != 0)
#define ALE_H_IS_LOW ((GPIOD->IDR & ALE_H) == 0)
#define ALE_L_IS_HIGH ((GPIOC->IDR & ALE_L) != 0)
#define ALE_L_IS_LOW ((GPIOC->IDR & ALE_L) == 0)
#define READ_IS_HIGH ((GPIOC->IDR & READ_LINE) != 0)
#define READ_IS_LOW ((GPIOC->IDR & READ_LINE) == 0)
#define RESET_IS_HIGH ((GPIOD->IDR & RESET_LINE) != 0)
#define RESET_IS_LOW ((GPIOD->IDR & RESET_LINE) == 0)

int InitializeDmaChannels(void);
void InitializeTimersPI(void);
extern "C" ITCM_FUNCTION void EXTI15_10_IRQHandler(void);
extern "C" ITCM_FUNCTION void EXTI1_IRQHandler(void);
extern "C" ITCM_FUNCTION void EXTI0_IRQHandler(void);
void ITCM_FUNCTION RunEEPROMEmulator(void);

extern DTCM_DATA volatile bool Running;

extern BYTE EEPROMStore[2048]; // 16KiBit
extern BYTE EEPROMType;
extern unsigned char *ram;

extern DTCM_DATA volatile uint32_t DMACount;
extern DTCM_DATA volatile uint32_t IntCount;
extern DTCM_DATA volatile uint32_t ALE_H_Count;
extern DTCM_DATA volatile uint32_t ADInputAddress;
extern DTCM_DATA uint32_t EepLogIdx;

#define EEPROM_4K 0x80
#define EEPROM_16K 0xC0
#if 0
    constexpr Pin D25 = Pin(PORTA, 0); // AD0  // Could be used for DMA but upsets the conguity of the bits.
    constexpr Pin D24 = Pin(PORTA, 1); // AD1
    constexpr Pin D28 = Pin(PORTA, 2); // AD2  // Could be used for DMA but upsets the conguity of the bits.
    constexpr Pin D16 = Pin(PORTA, 3); // AD3
    constexpr Pin D23 = Pin(PORTA, 4); // AD4
    constexpr Pin D22 = Pin(PORTA, 5); // AD5
    constexpr Pin D19 = Pin(PORTA, 6); // AD6
    constexpr Pin D18 = Pin(PORTA, 7); // AD7

    constexpr Pin D17 = Pin(PORTB, 1); // Write.  TIM3_CH4 // This is a good candidate for S-DATA, because Tim3-CH4 can do direct capture mode of a signal.
    constexpr Pin D9  = Pin(PORTB, 4); // AD8 --- TIM3-CH1
    constexpr Pin D10 = Pin(PORTB, 5); // AD9 --- TIM3-CH2
    constexpr Pin D13 = Pin(PORTB, 6); // AD10 -- TIM4-CH1
    constexpr Pin D14 = Pin(PORTB, 7); // AD11 -- TIM4-CH2
    constexpr Pin D11 = Pin(PORTB, 8); // AD12 -- TIM4-CH3
    constexpr Pin D12 = Pin(PORTB, 9); // AD13 -- TIM4-CH4
    constexpr Pin D0  = Pin(PORTB, 12); // ALE_H  TIM1-BKIN
    constexpr Pin D29 = Pin(PORTB, 14); // AD14 - TIM1_CH2N
    constexpr Pin D30 = Pin(PORTB, 15); // AD15 - TIM1_CH3N

    constexpr Pin D15 = Pin(PORTC, 0); // ALE_L // Read (ADC3 IN10) 
    constexpr Pin D20 = Pin(PORTC, 1); // Read // Reset (ADC3 IN11)
    constexpr Pin D21 = Pin(PORTC, 4); // S-DATA 
    constexpr Pin D4  = Pin(PORTC, 8); // SD card D0
    constexpr Pin D3  = Pin(PORTC, 9); // SD card D1
    constexpr Pin D2  = Pin(PORTC, 10); // SD card D2
    constexpr Pin D1  = Pin(PORTC, 11); // SD card D3
    constexpr Pin D6  = Pin(PORTC, 12); // SD card CLK

    constexpr Pin D5  = Pin(PORTD, 2);  // SD card CMD  // Could be used for DMA, requires disabling the SD Card.
    constexpr Pin D26 = Pin(PORTD, 11); // Reset // LPTIM2_IN2 cannot be used for anything useful..

    constexpr Pin D27 = Pin(PORTG, 9);  // CIC_D1
    constexpr Pin D7  = Pin(PORTG, 10); // CIC_D2
    constexpr Pin D8  = Pin(PORTG, 11); // N64_NMI // HRTIM_EEV4 --
    // PORTC, 7 -- LED, NC -- Card Detect ?
    // PORTG, 14 NC -- S-CLK  // LPTIM1_ETR -- Could be used for DMA with chain.
    // 
#endif