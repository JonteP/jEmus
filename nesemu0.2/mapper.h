/*
 * mapper.h
 *
 *  Created on: Jul 25, 2018
 *      Author: jonas
 */

#ifndef MAPPER_H_
#define MAPPER_H_
#include <stdint.h>

extern inline void mapper_mmc1(uint16_t, uint8_t), reset_mmc1(void),
				   mapper_mmc3(uint16_t, uint8_t), reset_mmc3(void),
				   mapper_vrc24(uint16_t, uint8_t), reset_vrc24(void),
				   reset_nrom(void), reset_default(void),
				   mapper_uxrom(uint8_t), reset_uxrom(void),
				   mapper_cnrom(uint8_t), reset_cnrom(void),
				   mapper_axrom(uint8_t), reset_axrom(void);

extern uint8_t mmc3IrqLatch, mmc3IrqReload, mmc3IrqCounter, mmc3IrqEnable, mmc3Int,
			   vrcIrqControl, vrcIrqInt, vrcIrqLatch, vrcIrqCounter;
extern uint16_t vrcIrqPrescale;

#endif /* MAPPER_H_ */
