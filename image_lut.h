/*
 * image_lut.h
 *
 *  Created on: Jan 27, 2023
 *      Author: Gintaras
 */

#ifndef IMAGE_LUT_H_
#define IMAGE_LUT_H_

/* Set baud rate to special 1093750 */
#define ARDU_BAUD_RATE       		1093750

#define TH_IMG_POSLEFT				UINT16_C (137)
#define TH_IMG_POSTOP				UINT16_C (63)

#define RULER_POSLEFT				UINT16_C (89)
#define RULER_POSTOP				UINT16_C (80)

#define DUMMY_CMD					0
#define COLOUR_CMD					249
#define POSLEFT_CMD					251
#define POSTOP_CMD					252

#define BITS_UINT8					UINT8_C  (255)

#define BUFF_OVF_TOUT_MS			100

#include "cyhal.h"
#include "cybsp.h"

extern uint8_t iron_map[];
extern uint8_t ruler_map[];

#endif /* IMAGE_LUT_H_ */
