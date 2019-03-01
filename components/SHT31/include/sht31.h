
#include <stdbool.h>
#include <esp_err.h>

#ifndef _COMPONENT_SHT31_H_
#define _COMPONENT_SHT31_H_

#define I2C_NUM 				I2C_NUM_0
#define I2C_SCL_IO    			(GPIO_NUM_23)    /*!< gpio number for I2C master clock */
#define I2C_SDA_IO    			(GPIO_NUM_22)    /*!< gpio number for I2C master data  */
#define I2C_FREQ_HZ    		    100000     /*!< I2C master clock frequency */
#define I2C_TX_BUF_DISABLE   	0   /*!< I2C master do not need buffer */
#define I2C_RX_BUF_DISABLE   	0   /*!< I2C master do not need buffer */

#define SHT31_ADDRESS 			0x44
#define SHT31_SOFTRESET			0x30A2
#define SHT31_ACK_CHECK_EN   	0x1     /*!< I2C master will check ack from slave*/
#define SHT31_ACK_CHECK_DIS  	0x0     /*!< I2C master will not check ack from slave */
#define SHT31_ACK_VAL    0x0         /*!< I2C ack value */
#define SHT31_NACK_VAL   0x1         /*!< I2C nack value */

extern void i2c_init(void) ;

extern bool sht31_readTempHum(double* Temperature,double* Humidity) ;
extern esp_err_t sht31_reset();
extern uint8_t sht31_crc8(const uint8_t *data, int len);

double Temperature;
double Humidity;
char Temperature_c[20];
char Humidity_c[20];


#endif /* _COMPONENT_SHT31_H_ */
