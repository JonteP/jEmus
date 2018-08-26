#ifndef MAPPER_H_
#define MAPPER_H_
#include <stdint.h>

void init_mapper(void), write_mapper_register(uint16_t, uint8_t), vrc_irq(void);
extern uint8_t mmc3IrqLatch, mmc3IrqReload, mmc3IrqCounter, mmc3IrqEnable, mmc3Int;

#endif /* MAPPER_H_ */
