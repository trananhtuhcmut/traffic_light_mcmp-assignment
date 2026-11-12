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
#include "i2c.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "scheduler.h"
#include "traffic_light.h"
#include "button.h"
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

/* USER CODE BEGIN PV */
uint8_t found[8] = {0};   // Lưu địa chỉ I2C tìm được (Mục 3.4)
int n = 0;                // Số lượng thiết bị I2C tìm thấy
uint8_t btn_raw[5] = {0}; // Lưu mức logic (0 hoặc 1) của 5 nút nhấn (Mục 3.3)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* === Traffic Light Test ===
 * Road 1: cycles RED -> GREEN -> AMBER -> OFF (2s each)
 * Road 2: cycles GREEN -> AMBER -> RED -> OFF (2s each, offset)
 * This verifies all 4 states on both roads.
 */
static const TrafficLightState test_seq_road1[] = {TL_RED, TL_GREEN, TL_AMBER, TL_OFF};
static const TrafficLightState test_seq_road2[] = {TL_GREEN, TL_AMBER, TL_RED, TL_OFF};
static uint8_t test_index = 0;

void traffic_test_cycle(void)
{
  traffic_set(1, test_seq_road1[test_index]);
  traffic_set(2, test_seq_road2[test_index]);
  test_index = (test_index + 1) % 4;
}

/* === Button Test ===
 * Btn 0 (D7): short press -> toggle LED PA5 (Flashing)
 * Btn 1 (D8): short press -> toggle LED PC9 (Built_IN_LED)
 * Btn 2 (D9): short press -> cycle Road 1 state
 * Btn 3 (D10): short press -> cycle Road 2 state
 * B1 (on-board): long press -> turn OFF both roads
 */
void button_test_task(void)
{
  if (is_button_pressed(0))
  {
    HAL_GPIO_TogglePin(Flashing_GPIO_Port, Flashing_Pin);
  }
  if (is_button_pressed(1))
  {
    HAL_GPIO_TogglePin(Built_IN_LED_GPIO_Port, Built_IN_LED_Pin);
  }
  if (is_button_pressed(2))
  {
    TrafficLightState s = (traffic_get(1) + 1) % 4;
    traffic_set(1, s);
  }
  if (is_button_pressed(3))
  {
    TrafficLightState s = (traffic_get(2) + 1) % 4;
    traffic_set(2, s);
  }
  if (is_button_long_pressed(4))
  {
    traffic_set(1, TL_OFF);
    traffic_set(2, TL_OFF);
  }
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
  // 1. Kiểm tra 2 LED Built-in (PA5 và PC9): Chớp tắt 3 lần khi khởi động
  for (int i = 0; i < 6; i++)
  {
    HAL_GPIO_TogglePin(Flashing_GPIO_Port, Flashing_Pin);
    HAL_GPIO_TogglePin(Built_IN_LED_GPIO_Port, Built_IN_LED_Pin);
    HAL_Delay(250);
  }
  HAL_GPIO_WritePin(Flashing_GPIO_Port, Flashing_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(Built_IN_LED_GPIO_Port, Built_IN_LED_Pin, GPIO_PIN_RESET);

  // 2. Quét thiết bị I2C (Mục 3.4): Tìm địa chỉ của DHT20 và LCD
  for (uint8_t addr = 1; addr < 128; addr++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK)
    {
      if (n < 8)
      {
        found[n++] = addr;
      }
    }
  }
  // �?ẶT BREAKPOINT TẠI DÒNG DƯỚI �?ÂY �?Ể XEM MẢNG found[] VÀ BIẾN n
  HAL_Delay(100);

  // 3. Scheduler + Module Init
  HAL_TIM_Base_Start_IT(&htim2);
  SCH_Init();
  traffic_light_init();
  button_init();

  // Task 1: Button debounce reading every 10ms (period = 1 tick)
  SCH_Add_Task(button_reading, 0, 1);

  // Task 2: Button test - check press flags every 10ms
  SCH_Add_Task(button_test_task, 0, 1);

  // Task 3: Traffic light auto-cycle every 2s (200 ticks)
  SCH_Add_Task(traffic_test_cycle, 0, 200);
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
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
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

#ifdef  USE_FULL_ASSERT
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
