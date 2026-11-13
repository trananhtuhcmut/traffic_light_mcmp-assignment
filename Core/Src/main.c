/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "global.h"
#include "scheduler.h"
#include "button.h"
#include "traffic_light.h"
#include "lcd.h"
#include "dht20.h"
#include "env_display.h"
#include "fsm_auto.h"
#include "fsm_manual.h"
#include "fsm_setting.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
System_Mode curr_mode = MODE_AUTO;

Traffic_Light_Config_t curr_light_config = {
    .R1_dur = DEFAULT_RED, .R2_dur = DEFAULT_RED, .Y1_dur = DEFAULT_AMBER, .Y2_dur = DEFAULT_AMBER, .G1_dur = DEFAULT_GREEN, .G2_dur = DEFAULT_GREEN};

System_Event_t event = {0};
Display_Data_t curr_disp_data = {0};

static Traffic_Light_State road_1_state = TL_OFF;
static Traffic_Light_State road_2_state = TL_OFF;

static uint8_t tick_250ms_cnt = 0;
static uint8_t tick_1s_cnt = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
void system_fsm_task(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void system_fsm_task(void)
{
  /* 1. Tao co su kien thoi gian TICK_250MS (25 ticks) va TICK_1S (100 ticks) */
  if (++tick_250ms_cnt >= (250U / SCHEDULER_TICK_MS))
  {
    tick_250ms_cnt = 0;
    event.TICK_250MS = true;
  }

  if (++tick_1s_cnt >= TICK_PER_SEC)
  {
    tick_1s_cnt = 0;
    event.TICK_1S = true;
  }

  /* 2. Doc su kien tu cac nut nhan */
  if (is_button_pressed(0))
    event.MODE_CHANGE = true;
  if (is_button_pressed(1))
    event.SWITCH_LIGHT = true;
  if (is_button_long_pressed(1))
    event.CONFIRM = true;
  if (is_button_pressed(2) || is_button_long_pressed(2))
    event.INCREASE = true;
  if (is_button_pressed(3) || is_button_long_pressed(3))
    event.DECREASE = true;
#if (NUM_BUTTONS >= 5)
  if (is_button_pressed(4))
    event.CONFIRM = true; /* Nut B1 (PC13) */
#endif

  /* 3. Chuyen Mode khi co su kien MODE_CHANGE */
  if (event.MODE_CHANGE)
  {
    switch (curr_mode)
    {
    case MODE_AUTO:
      fsm_auto_exit();
      curr_mode = MODE_MANUAL;
      fsm_manual_enter();
      break;

    case MODE_MANUAL:
      fsm_manual_exit();
      curr_mode = MODE_SETTING;
      fsm_setting_enter();
      break;

    case MODE_SETTING:
      fsm_setting_exit();
      curr_mode = MODE_AUTO;
      fsm_auto_enter();
      break;

    default:
      curr_mode = MODE_AUTO;
      fsm_auto_enter();
      break;
    }
    // Give the newly entered mode a full 1s / 250ms period
    tick_1s_cnt = 0;
    tick_250ms_cnt = 0;
  }

  /* 4. Chay FSM cua Mode hien tai */
  switch (curr_mode)
  {
  case MODE_AUTO:
    fsm_auto_run();
    break;
  case MODE_MANUAL:
    fsm_manual_run();
    break;
  case MODE_SETTING:
    fsm_setting_run();
    break;
  default:
    break;
  }

  /* 5. Kiem tra an toan & xuat trang thai den ra GPIO qua traffic_light.c */
  if (road_1_state == TL_GREEN && road_2_state == TL_GREEN)
  {
    road_1_state = TL_RED;
    road_2_state = TL_RED;
  }

  if (road_1_state != traffic_get(1))
  {
    traffic_set(1, road_1_state);
  }
  if (road_2_state != traffic_get(2))
  {
    traffic_set(2, road_2_state);
  }

  /* 6. Xoa toan bo co su kien one-shot sau moi chu ky */
  event = (System_Event_t){0};
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  SCH_Init();
  button_init();
  traffic_light_init();
  lcd_init(&hi2c1, 0x21);
  dht20_init(&hi2c1);

  fsm_auto_init(&curr_light_config, &road_1_state, &road_2_state, &curr_disp_data);
  fsm_manual_init(&curr_light_config, &road_1_state, &road_2_state, &curr_disp_data);
  fsm_setting_init(&curr_light_config, &road_1_state, &road_2_state, &curr_disp_data);

  curr_mode = MODE_AUTO;
  fsm_auto_enter();

  SCH_Add_Task(button_reading, 0, 1);
  SCH_Add_Task(system_fsm_task, 0, 1);
  SCH_Add_Task(display_task, 5, 10);
  SCH_Add_Task(lcd_flush_task, 6, 5);
  SCH_Add_Task(env_sample_task, 10, 200);

  HAL_TIM_Base_Start_IT(&htim2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    SCH_Dispatch_Tasks();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void)
{
  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void)
{
  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED1_1_GPIO_Port, LED1_1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED2_1_Pin | LED2_2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED1_2_GPIO_Port, LED1_2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : Button1_2_Pin Button2_1_Pin Button1_1_Pin Button2_2_Pin */
  GPIO_InitStruct.Pin = Button1_2_Pin | Button2_1_Pin | Button1_1_Pin | Button2_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LED1_1_Pin */
  GPIO_InitStruct.Pin = LED1_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED1_1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED2_1_Pin LED2_2_Pin */
  GPIO_InitStruct.Pin = LED2_1_Pin | LED2_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LED1_2_Pin */
  GPIO_InitStruct.Pin = LED1_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED1_2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
