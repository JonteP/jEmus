#ifndef MAPPER_H_
#define MAPPER_H_
#include <stdint.h>

void init_mapper(void), write_mapper_register(uint16_t, uint8_t), vrc_irq(void),
     mmc3_irq(void);
extern uint8_t mapperInt;

#endif /* MAPPER_H_ */
