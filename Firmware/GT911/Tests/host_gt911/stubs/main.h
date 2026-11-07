#pragma once
#include <cstdint>
struct GPIO_TypeDef {int port;};
enum GPIO_PinState { GPIO_PIN_RESET=0,GPIO_PIN_SET=1 };
struct GPIO_InitTypeDef {uint32_t Pin;uint32_t Mode;uint32_t Pull;uint32_t Speed;};
#define GPIO_MODE_INPUT 0u
#define GPIO_MODE_OUTPUT_PP 1u
#define GPIO_NOPULL 0u
#define GPIO_SPEED_FREQ_LOW 0u
void HAL_GPIO_WritePin(GPIO_TypeDef*,uint16_t,GPIO_PinState);
void HAL_GPIO_Init(GPIO_TypeDef*,GPIO_InitTypeDef*);
