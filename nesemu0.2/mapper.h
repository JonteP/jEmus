#ifndef MAPPER_H_
#define MAPPER_H_
#include <stdint.h>

void init_mapper(void), (*write_mapper_register)(uint_fast16_t, uint_fast8_t), vrc_irq(void),
     mmc3_irq(void);
float vrc6_sound(void);
float (*expansion_sound)(void);
extern uint_fast8_t mapperInt, expSound, wramBit, wramBitVal, prgBank[8], chrBank[8];

#endif /* MAPPER_H_ */
