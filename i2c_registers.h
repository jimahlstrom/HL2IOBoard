// These are the registers in the HL2 IO board Pico

#define REG_TX_FREQ_BYTE4	0
#define REG_TX_FREQ_BYTE3	1
#define REG_TX_FREQ_BYTE2	2
#define REG_TX_FREQ_BYTE1	3
#define REG_TX_FREQ_BYTE0	4
#define REG_CONTROL		5
#define REG_INPUT_PINS		6
#define REG_ANTENNA_TUNER	7
#define REG_FAULT		8

#define REG_FIRMWARE_MAJOR	9
#define REG_FIRMWARE_MINOR	10

#define REG_RF_INPUTS		11
#define REG_FAN_SPEED		12

#define REG_FCODE_RX1		13
#define REG_FCODE_RX2		14
#define REG_FCODE_RX3		15
#define REG_FCODE_RX4		16
#define REG_FCODE_RX5		17
#define REG_FCODE_RX6		18
#define REG_FCODE_RX7		19
#define REG_FCODE_RX8		20
#define REG_FCODE_RX9		21
#define REG_FCODE_RX10		22
#define REG_FCODE_RX11		23
#define REG_FCODE_RX12		24

#define REG_ADC0_MSB		25
#define REG_ADC0_LSB		26
#define REG_ADC1_MSB		27
#define REG_ADC1_LSB		28
#define REG_ADC2_MSB		29
#define REG_ADC2_LSB		30
#define REG_ANTENNA		31

#define REG_STATUS		167
#define REG_IN_PINS		168
#define REG_OUT_PINS		169

#define GPIO_DIRECT_BASE	170	// map registers to GPIO pins for direct read and write
