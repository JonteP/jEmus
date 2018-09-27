#ifndef MAPPER_H_
#define MAPPER_H_
#include <stdint.h>

typedef enum {
	CHR_RAM,
	CHR_ROM
} chrtype_t;
void init_mapper(void), (*write_mapper_register)(uint_fast16_t, uint_fast8_t), vrc_irq(void),
     mmc3_irq(void), ss88006_irq();
void prg_bank_switch(), chr_bank_switch();
float vrc6_sound(void);
float (*expansion_sound)(void);
extern uint_fast8_t mapperInt, expSound, wramBit, wramBitVal, prgBank[8], chrBank[8];
extern chrtype_t chrSource[0x8];

#endif /* MAPPER_H_ */
