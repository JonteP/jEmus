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
				   reset_nrom(void),
				   mapper_uxrom(uint8_t), reset_uxrom(void),
				   mapper_cnrom(uint8_t), reset_cnrom(void),
				   mapper_axrom(uint8_t), reset_axrom(void);


#endif /* MAPPER_H_ */
