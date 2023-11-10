/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the RDK3 D6T32L01A Demo
*              Application for ModusToolbox.
*
* Related Document: See README.md
*
*
*  Created on: 2023-11-10
*  Company: Rutronik Elektronische Bauelemente GmbH
*  Address: Jonavos g. 30, Kaunas 44262, Lithuania
*  Author: Gintaras
*
*******************************************************************************
* (c) 2019-2021, Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
* This software, including source code, documentation and related materials
* ("Software"), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries ("Cypress") and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software ("EULA").
*
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress's integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*
* Rutronik Elektronische Bauelemente GmbH Disclaimer: The evaluation board
* including the software is for testing purposes only and,
* because it has limited functions and limited resilience, is not suitable
* for permanent use under real conditions. If the evaluation board is
* nevertheless used under real conditions, this is done at one’s responsibility;
* any liability of Rutronik is insofar excluded
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "d6t32l01a.h"
#include "image_lut.h"
#include "arm_math.h"

#define USE_KITPROG3_OUTPUT
//#define USE_DISPLAY_OUTPUT

#ifdef USE_DISPLAY_OUTPUT
/*Display Function prototypes*/
void ResetDisplay(void);
cy_rslt_t ardu_uart_init(void);
void DrawStaticDisplay(void);
void DrawThermalImage(void);
void DrawTemperatures(float max_t, float min_t);
#endif

/*I2C Device Global Variables*/
cyhal_i2c_t I2C_scb3;
cyhal_i2c_cfg_t i2c_scb3_cfg =
{
		.is_slave = false,
	    .address = 0,
	    .frequencyhal_hz = 1000000UL,
};

/*D6T32L01A Global Variables*/
uint8_t rbuf[N_READ];
float ptat;
float pix_data[N_PIXEL];

#ifdef USE_DISPLAY_OUTPUT
/*Arduino UART object and configuration*/
cyhal_uart_t ardu_uart;
/*Thermal image*/
uint8_t thermal_image[N_PIXEL] = {0};
uint8_t thermal_cache[N_PIXEL] = {0};
float max_temp;
uint32_t max_temp_index;
float min_temp;
uint32_t min_temp_index;
#endif

int main(void)
{
    cy_rslt_t result;
    uint32_t d6t_rslt;

#ifdef USE_DISPLAY_OUTPUT
	float scale_unit = 0;
	float thermal_diff = 0;
	int32_t iron_map_index = 0;
#endif

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
    	CY_ASSERT(0);
    }

    __enable_irq();

    /*Initialize LEDs*/
    result = cyhal_gpio_init( LED1, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}
    result = cyhal_gpio_init( LED2, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}
    result = cyhal_gpio_init( LED3, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}

    /*Enable debug output via KitProg UART*/
    result = cy_retarget_io_init( KITPROG_TX, KITPROG_RX, CY_RETARGET_IO_BAUDRATE);
    if (result != CY_RSLT_SUCCESS)
    {
    	CY_ASSERT(0);
    }

    /*Initialize I2C Master*/
    result = cyhal_i2c_init(&I2C_scb3, ARDU_SDA, ARDU_SCL, NULL);
    if (result != CY_RSLT_SUCCESS)
    {
    	CY_ASSERT(0);
    }
    result = cyhal_i2c_configure(&I2C_scb3, &i2c_scb3_cfg);
    if (result != CY_RSLT_SUCCESS)
    {
    	CY_ASSERT(0);
    }

#ifdef USE_DISPLAY_OUTPUT
    /*Initialize Charger Control Pin*/
    result = cyhal_gpio_init( CHR_DIS, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, true);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}

    /*Initialize Display RESET pin*/
    result = cyhal_gpio_init(ARDU_IO8, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}

	/*Reset the Display*/
	ResetDisplay();

    /*Initialize The Arduino UART*/
    result = ardu_uart_init();
    if (result != CY_RSLT_SUCCESS)
    {
    	printf("Could not initialize Arduino UART.\r\n");
    	CY_ASSERT(0);
    }

	/*Setup the environment*/
	DrawStaticDisplay();
#endif

    printf("\x1b[2J\x1b[;H");
    printf("D6T32L01 Test.\r\n");

    /*Initialize the D6T32L01A */
    d6t_rslt = d6t32_init();
    if(d6t_rslt != D6T_OK)
    {
    	printf("D6T32L01 Initialization Failure.\r\n");
    	CyDelay(100);
    	CY_ASSERT(0);
    }

    for (;;)
    {
    	/*Read the values from the sensor*/
    	d6t_rslt =  D6T_getvalue(rbuf, &ptat, pix_data);

    	/*Check if the temperature values were read without any errors*/
        if(d6t_rslt == D6T_OK)
        {
#ifdef USE_KITPROG3_OUTPUT
            /* Output results */
    		printf("PTAT: %4.1f [degC],\r\nTemperatures: 32x32 [degC],\r\n", ptat);

    		uint32_t j = 0;
    		for (uint32_t i = 0; i < N_PIXEL; i++)
    		{
    		    printf("%4.1f, ", pix_data[i]);
    		    j++;
    		    if(j > 31)
    		    {
    		    	printf("\r\n");
    		    	j=0;
    		    }
    		}
#endif
#ifdef USE_DISPLAY_OUTPUT
    		cyhal_gpio_write(LED1, CYBSP_LED_STATE_ON);

    		/*Calculate & Convert Color Scale*/
    		arm_max_f32(pix_data, N_PIXEL, &max_temp, &max_temp_index);
    		arm_min_f32(pix_data, N_PIXEL, &min_temp, &min_temp_index);
    		scale_unit = (max_temp - min_temp)/BITS_UINT8;
	    	for(uint32_t x = 0; x < N_PIXEL; x++)
	    	{
	    		thermal_diff = pix_data[x] - min_temp;
	    		iron_map_index = thermal_diff/scale_unit - 1;
	    		if(iron_map_index < 0)
	    		{
	    			iron_map_index = 0;
	    		}
	    		else if(iron_map_index > 254)
	    		{
	    			iron_map_index = 254;
	    		}
	    		thermal_image[x] = iron_map[iron_map_index];
	    	}

	    	/*Draw a Thermal Image*/
	    	DrawThermalImage();

		    /*Draw Min/Max Temperatures*/
		    DrawTemperatures(max_temp, min_temp);

		    cyhal_gpio_write(LED1, CYBSP_LED_STATE_OFF);
#endif
        }

        /*According to datasheet the data is ready within 200ms*/
    	CyDelay(200);
    }
}

#ifdef USE_DISPLAY_OUTPUT
cy_rslt_t ardu_uart_init(void)
{
	cy_rslt_t result;
	uint32_t actualbaud;

    /* Initialize the UART configuration structure */
    const cyhal_uart_cfg_t uart_config =
    {
        .data_bits = 8,
        .stop_bits = 1,
        .parity = CYHAL_UART_PARITY_NONE,
        .rx_buffer = NULL,
        .rx_buffer_size = 0
    };

    /* Initialize the UART Block */
    result = cyhal_uart_init(&ardu_uart, ARDU_TX, ARDU_RX, NC, NC, NULL, &uart_config);
	if (result != CY_RSLT_SUCCESS)
	{return result;}

	result = cyhal_uart_set_baud(&ardu_uart, ARDU_BAUD_RATE, &actualbaud);
	if (result != CY_RSLT_SUCCESS)
	{return result;}

	/*Connect internal pull-up resistor*/
	cyhal_gpio_configure(ARDU_RX, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP);

	return result;
}

/*Reset Display Function*/
void ResetDisplay(void)
{
	cyhal_gpio_write(ARDU_IO8, false);
	CyDelay(500);
	cyhal_gpio_write(ARDU_IO8, true);
	CyDelay(3000);
}

void DrawStaticDisplay(void)
{
	int x=0, y=0;
	uint32_t position = 0;

	/*RULER*/
	cyhal_uart_putc(&ardu_uart, POSLEFT_CMD);
	cyhal_uart_putc(&ardu_uart, RULER_POSLEFT & 0xFF);
	cyhal_uart_putc(&ardu_uart, (RULER_POSLEFT >> 8) & 0xFF);
	cyhal_uart_putc(&ardu_uart, DUMMY_CMD);
	cyhal_uart_putc(&ardu_uart, POSTOP_CMD);
	cyhal_uart_putc(&ardu_uart, RULER_POSTOP & 0xFF);
	cyhal_uart_putc(&ardu_uart, (RULER_POSTOP >> 8) & 0xFF);
	cyhal_uart_putc(&ardu_uart, DUMMY_CMD);

	/*Draw the ruler image*/
	position = 0;
	for(y = 0; y < 20; y++)
	{
		for(x = 0; x < 4; x++)
		{
	    	cyhal_uart_putc(&ardu_uart, x);
	    	cyhal_uart_putc(&ardu_uart, y);
	    	cyhal_uart_putc(&ardu_uart, 0x20);
	    	cyhal_uart_putc(&ardu_uart, ruler_map[position]);
	    	position++;
		}
	}
}

void DrawThermalImage(void)
{
	cy_rslt_t result;
	int x=0, y=0;
	uint32_t position = 0;
	uint8_t byte;

	/*POSLEFT*/
	cyhal_uart_putc(&ardu_uart, POSLEFT_CMD);
	cyhal_uart_putc(&ardu_uart, TH_IMG_POSLEFT & 0xFF);
	cyhal_uart_putc(&ardu_uart, (TH_IMG_POSLEFT >> 8) & 0xFF);
	cyhal_uart_putc(&ardu_uart, DUMMY_CMD);

	/*POSTOP*/
	cyhal_uart_putc(&ardu_uart, POSTOP_CMD);
	cyhal_uart_putc(&ardu_uart, TH_IMG_POSTOP & 0xFF);
	cyhal_uart_putc(&ardu_uart, (TH_IMG_POSTOP >> 8) & 0xFF);
	cyhal_uart_putc(&ardu_uart, DUMMY_CMD);

	/*Draw the thermal image*/
	position = 0;
	for(y = 0; y < 32; y++)
	{
		for(x = 0; x < 32; x++)
		{
	    	/*Send if data changes*/
			if(thermal_image[position] != thermal_cache[position])
			{
		    	cyhal_uart_putc(&ardu_uart, x);
		    	cyhal_uart_putc(&ardu_uart, y);
		    	cyhal_uart_putc(&ardu_uart, 0x20);
		    	cyhal_uart_putc(&ardu_uart, thermal_image[position]);
		    	thermal_cache[position] = thermal_image[position];
			}
	    	position++;

	    	/*Check if a display data buffer is now overflowing*/
	    	result = cyhal_uart_readable(&ardu_uart);
	        if (result > 0)
	        {
	        	cyhal_uart_getc(&ardu_uart, &byte,0xFFFFFFFF);
	        	if(byte == 0xFF)
	        	{
	        		/*Wait for ready signal with a timeout*/
	        		for(uint8_t j = 0; j < BUFF_OVF_TOUT_MS; j++)
	        		{
	        			CyDelay(1);
	        			result = cyhal_uart_readable(&ardu_uart);
	        			if (result > 0)
	        			{
	        				cyhal_uart_getc(&ardu_uart, &byte,0xFFFFFFFF);
	        			}
	        			if(byte == 0xFE)
	        			{
	        				break;
	        			}
	        		}
	        	}
	        }
		}
	}
}

void DrawTemperatures(float max_t, float min_t)
{
	char temp[6] = {0};
	uint8_t pos;

	/*Set the position above the ruler*/
	cyhal_uart_putc(&ardu_uart, POSLEFT_CMD);
	cyhal_uart_putc(&ardu_uart, RULER_POSLEFT & 0xFF);
	cyhal_uart_putc(&ardu_uart, (RULER_POSLEFT >> 8) & 0xFF);
	cyhal_uart_putc(&ardu_uart, DUMMY_CMD);
	cyhal_uart_putc(&ardu_uart, POSTOP_CMD);
	cyhal_uart_putc(&ardu_uart, (RULER_POSTOP-10) & 0xFF);
	cyhal_uart_putc(&ardu_uart, ((RULER_POSTOP-10) >> 8) & 0xFF);
	cyhal_uart_putc(&ardu_uart, DUMMY_CMD);

	/*Convert maximum temperature to string*/
	sprintf(temp, "%d", (int)max_t);

	/*Clean old data*/
	for(pos = 0; pos < sizeof(temp); pos++)
	{
		cyhal_uart_putc(&ardu_uart, pos);
		cyhal_uart_putc(&ardu_uart, 0);
		cyhal_uart_putc(&ardu_uart, 0x20);
		cyhal_uart_putc(&ardu_uart, 0x00);
	}

	/*Draw the maximum temperature*/
	for(pos = 0; pos < strlen(temp); pos++)
	{
		cyhal_uart_putc(&ardu_uart, pos);
		cyhal_uart_putc(&ardu_uart, 0);
		cyhal_uart_putc(&ardu_uart, temp[pos]);
		cyhal_uart_putc(&ardu_uart, 0x00);
	}

	/*Draw the degrees symbol*/
	cyhal_uart_putc(&ardu_uart, pos);
	cyhal_uart_putc(&ardu_uart, 0);
	cyhal_uart_putc(&ardu_uart, 0xA1);
	cyhal_uart_putc(&ardu_uart, 0x00);
	pos++;

	/*Draw the Celsius symbol*/
	cyhal_uart_putc(&ardu_uart, pos);
	cyhal_uart_putc(&ardu_uart, 0);
	cyhal_uart_putc(&ardu_uart, 'C');
	cyhal_uart_putc(&ardu_uart, 0x00);

	/*Set the position below the ruler*/
	cyhal_uart_putc(&ardu_uart, POSLEFT_CMD);
	cyhal_uart_putc(&ardu_uart, RULER_POSLEFT & 0xFF);
	cyhal_uart_putc(&ardu_uart, (RULER_POSLEFT >> 8) & 0xFF);
	cyhal_uart_putc(&ardu_uart, DUMMY_CMD);
	cyhal_uart_putc(&ardu_uart, POSTOP_CMD);
	cyhal_uart_putc(&ardu_uart, (RULER_POSTOP+160) & 0xFF);
	cyhal_uart_putc(&ardu_uart, ((RULER_POSTOP+160) >> 8) & 0xFF);
	cyhal_uart_putc(&ardu_uart, DUMMY_CMD);

	/*Convert maximum temperature to string*/
	memset(temp, 0x00, sizeof(temp));
	sprintf(temp, "%d", (int)min_t);

	/*Clean old data*/
	for(pos = 0; pos < sizeof(temp); pos++)
	{
		cyhal_uart_putc(&ardu_uart, pos);
		cyhal_uart_putc(&ardu_uart, 0);
		cyhal_uart_putc(&ardu_uart, 0x20);
		cyhal_uart_putc(&ardu_uart, 0x00);
	}

	/*Draw the mainimum temperature*/
	for(pos = 0; pos < strlen(temp); pos++)
	{
		cyhal_uart_putc(&ardu_uart, pos);
		cyhal_uart_putc(&ardu_uart, 0);
		cyhal_uart_putc(&ardu_uart, temp[pos]);
		cyhal_uart_putc(&ardu_uart, 0x00);
	}

	/*Draw the degrees symbol*/
	cyhal_uart_putc(&ardu_uart, pos);
	cyhal_uart_putc(&ardu_uart, 0);
	cyhal_uart_putc(&ardu_uart, 0xA1);
	cyhal_uart_putc(&ardu_uart, 0x00);
	pos++;

	/*Draw the Celsius symbol*/
	cyhal_uart_putc(&ardu_uart, pos);
	cyhal_uart_putc(&ardu_uart, 0);
	cyhal_uart_putc(&ardu_uart, 'C');
	cyhal_uart_putc(&ardu_uart, 0x00);
}
#endif

/* [] END OF FILE */
