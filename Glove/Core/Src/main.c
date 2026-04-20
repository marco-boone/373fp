/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
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
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart1_tx;

/* USER CODE BEGIN PV */
typedef enum
{
  FINGERS_UP = 0,
  FINGERS_DOWN,
  FINGERS_HORIZONTAL,
  FINGERS_UNKNOWN
} FingerDirection_t;

typedef enum
{
  HAND_BACK_UP = 0,   /* +Z points up (out of back of hand is up) */
  HAND_PALM_UP,       /* -Z points up */
  HAND_EDGE_UP,       /* neither back nor palm mostly up */
  HAND_FACE_UNKNOWN
} HandFace_t;

typedef struct
{
  FingerDirection_t fingers;
  HandFace_t face;
} HandPose_t;



static HandPose_t s_hand_pose;
#define SAD_W_A 0x32
#define SAD_R_A 0x33

void myprintf(const char *fmt, ...);

#define ADC_MODE_THRESH 100
#define ADC_FLEX_THRESH 200
static uint16_t adc1_dma_initial_buf_average[4];
static uint16_t adc1_dma_buf[16];
static uint16_t adc1_dma_buf_average[4];
static uint8_t  glove_commands[7];
static uint8_t  sensor = 0;
static uint8_t  s_mode = 0; 

/* 1..10 value representing left/right roll around X axis (X along fingers). */
static volatile uint8_t s_roll_x_1to10 = 5u;


/* Accelerometer register map (matches your reference code) */
#define ACCEL_REG_CTRL1    0x20
#define ACCEL_REG_OUT_X_L  0x28
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void mode_switch(void)
{
	
	if((adc1_dma_buf_average[0] - adc1_dma_initial_buf_average[0] > ADC_MODE_THRESH) & (!sensor))
	{
		sensor = 1;//doesn't allow one tap to go trhough multiple with the debounce as well
		s_mode = (s_mode + 1) % 4; //next mode
		HAL_Delay(100);//debounce
	}
	else
	{
		sensor = 0;
	}
}

void myprintf(const char *fmt, ...)
{
  char buffer[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  HAL_UART_Transmit(&huart2, (uint8_t *)buffer, strlen(buffer), 0xFFFF);
}

void update_glove_commands(void)
{
 
  glove_commands[0] = s_mode; 
  glove_commands[1] = adc1_dma_buf_average[1] - adc1_dma_initial_buf_average[1] > ADC_FLEX_THRESH ? 1 : 0;
  glove_commands[2] = adc1_dma_buf_average[2] - adc1_dma_initial_buf_average[2] > ADC_FLEX_THRESH ? 1 : 0;
  glove_commands[3] = adc1_dma_buf_average[3] - adc1_dma_initial_buf_average[3] > ADC_FLEX_THRESH ? 1 : 0;
  glove_commands[4] = s_hand_pose.fingers == FINGERS_UP ? 1 : 0;
  glove_commands[5] = s_hand_pose.face == HAND_PALM_UP ? 1 : 0;
  glove_commands[6] = s_roll_x_1to10; /* 1..10 */
}

void average_adc_values(void)
{
  adc1_dma_buf_average[0] = (adc1_dma_buf[0] + adc1_dma_buf[4] + adc1_dma_buf[8] + adc1_dma_buf[12]) / 4;
  adc1_dma_buf_average[1] = (adc1_dma_buf[1] + adc1_dma_buf[5] + adc1_dma_buf[9] + adc1_dma_buf[13]) / 4;
  adc1_dma_buf_average[2] = (adc1_dma_buf[2] + adc1_dma_buf[6] + adc1_dma_buf[10] + adc1_dma_buf[14]) / 4;
  adc1_dma_buf_average[3] = (adc1_dma_buf[3] + adc1_dma_buf[7] + adc1_dma_buf[11] + adc1_dma_buf[15]) / 4;
}

void average_initial_adc_values(void)
{
  adc1_dma_initial_buf_average[0] = adc1_dma_buf_average[0];
  adc1_dma_initial_buf_average[1] = adc1_dma_buf_average[1];
  adc1_dma_initial_buf_average[2] = adc1_dma_buf_average[2];
  adc1_dma_initial_buf_average[3] = adc1_dma_buf_average[3];
}

HAL_StatusTypeDef Glove_UART2_SendCommands_DMA(void)
{
  /* Don't start a new TX while DMA is still running */
  if (HAL_UART_GetState(&huart1) != HAL_UART_STATE_READY)
  {
    return HAL_BUSY;
  }

  return HAL_UART_Transmit_DMA(&huart1, glove_commands, 7);
}

static HAL_StatusTypeDef Accel_Init(void)
{
  /* Example init from your reference: write 0x97 to CTRL_REG1 (0x20) */
  uint8_t reg_and_val[2] = { ACCEL_REG_CTRL1, 0x97 };
  return HAL_I2C_Master_Transmit(&hi2c1, SAD_W_A, reg_and_val, 2, 1000);
}

static HAL_StatusTypeDef Accel_ReadXYZ(int16_t *x, int16_t *y, int16_t *z)
{
  /* Read 6 bytes starting at OUT_X_L.
   * Many ST accel parts support auto-increment with bit7=1. */
  uint8_t start_reg = (uint8_t)(ACCEL_REG_OUT_X_L | 0x80);
  uint8_t buf[6] = {0};

  HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(&hi2c1, SAD_W_A, &start_reg, 1, 1000);
  if (ret != HAL_OK) return ret;

  ret = HAL_I2C_Master_Receive(&hi2c1, SAD_R_A, buf, 6, 1000);
  if (ret != HAL_OK) return ret;

  /* Little-endian: XL,XH,YL,YH,ZL,ZH */
  *x = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
  *y = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
  *z = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);
  return HAL_OK;
}


static HandPose_t HandPose_FromAccelXYZ(int16_t ax, int16_t ay, int16_t az);
static const char *FingerDirection_ToString(FingerDirection_t d);
static const char *HandFace_ToString(HandFace_t f);

static void update_hand_pose(void)
{
  int16_t x = 0, y = 0, z = 0;
  if (Accel_ReadXYZ(&x, &y, &z) != HAL_OK)
  {
    myprintf("ACCEL I2C read error\r\n");
    return;
  }

  s_hand_pose = HandPose_FromAccelXYZ(x, y, z);

  /* Roll around X axis (left/right tilt) from gravity vector:
   * roll_x = atan2(Y, Z) -> approx [-pi, +pi]. Clamp to +/- 90deg and map to 1..10.
   * If direction is inverted for your mounting, swap signs of y/z or invert the mapping. */
  {
    const float k_max = 1.57079632679f; /* pi/2 */
    /* Use -Z so "normal" (back-of-hand up -> z<0, y~0) maps near roll=0.
     * Then wrap to [-pi/2, +pi/2] so we don't jump to +/-pi when Z flips sign. */
    float roll = atan2f((float)y, (float)(-z)); /* [-pi, +pi] */
    if (roll > k_max) roll -= 3.14159265359f;
    if (roll < -k_max) roll += 3.14159265359f;
    if (roll > k_max) roll = k_max;
    if (roll < -k_max) roll = -k_max;
    float t = (roll + k_max) / (2.0f * k_max); /* 0..1 */
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    uint32_t v = (uint32_t)lrintf(1.0f + t * 9.0f); /* 1..10 */
    if (v < 1u) v = 1u;
    if (v > 10u) v = 10u;
    s_roll_x_1to10 = (uint8_t)v;
  }
}

static const char *FingerDirection_ToString(FingerDirection_t d)
{
  switch (d)
  {
    case FINGERS_UP: return "FINGERS_UP";
    case FINGERS_DOWN: return "FINGERS_DOWN";
    case FINGERS_HORIZONTAL: return "FINGERS_HORIZONTAL";
    default: return "FINGERS_UNKNOWN";
  }
}

static const char *HandFace_ToString(HandFace_t f)
{
  switch (f)
  {
    case HAND_BACK_UP: return "BACK_OF_HAND_UP";
    case HAND_PALM_UP: return "PALM_UP";
    case HAND_EDGE_UP: return "EDGE_UP";
    default: return "FACE_UNKNOWN";
  }
}

/*
 * Translate accelerometer XYZ into a coarse "hand position".
 *
 * Coordinate assumptions (per your description):
 * - +X points toward the fingers (fully extended direction).
 * - +Z points out of the back of the hand.
 * - +Y is across the back of the hand.
 *
 * Uses ONLY gravity (accelerometer), so it cannot determine yaw/heading:
 * it can say up/down vs horizontal, and whether palm/back/edge is up.
 */
static HandPose_t HandPose_FromAccelXYZ(int16_t ax, int16_t ay, int16_t az)
{
  HandPose_t p = { .fingers = FINGERS_UNKNOWN, .face = HAND_FACE_UNKNOWN };

  int32_t x = ax, y = ay, z = az;
  int32_t axa = (x >= 0) ? x : -x;
  int32_t aya = (y >= 0) ? y : -y;
  int32_t aza = (z >= 0) ? z : -z;

  /* Decide which axis is most aligned with gravity. */
  int32_t maxa = axa;
  char max_axis = 'x';
  if (aya > maxa) { maxa = aya; max_axis = 'y'; }
  if (aza > maxa) { maxa = aza; max_axis = 'z'; }

  /*
   * Finger direction:
   * If gravity points mostly along +X, then +X is "down" -> fingers down.
   * If gravity points mostly along -X, then +X is "up"   -> fingers up.
   * Otherwise fingers are roughly horizontal.
   */
  if (max_axis == 'x')
  {
    p.fingers = (x < 0) ? FINGERS_UP : FINGERS_DOWN;
  }
  else
  {
    p.fingers = FINGERS_HORIZONTAL;
  }

  /*
   * Which face is up:
   * If gravity points mostly along +Z, +Z is down -> palm is up.
   * If gravity points mostly along -Z, +Z is up   -> back of hand is up.
   * Otherwise it's on an edge (thumb/pinky side or finger/wrist edge).
   */
  if (max_axis == 'z')
  {
    p.face = (z < 0) ? HAND_BACK_UP : HAND_PALM_UP;
  }
  else
  {
    p.face = HAND_EDGE_UP;
  }

  return p;
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
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  myprintf("The Last Dance G\r\n");

  if (Accel_Init() != HAL_OK)
  {
    myprintf("ACCEL init error\r\n");
  }

  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1_dma_buf,
                        (uint32_t)(sizeof(adc1_dma_buf) / sizeof(adc1_dma_buf[0]))) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_Delay(500);
  average_adc_values();
  average_initial_adc_values();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    mode_switch();
    average_adc_values();
    update_glove_commands();
    update_hand_pose();
    myprintf("button initial: %d, button avg: %d, RAW button: %d\r\n", adc1_dma_initial_buf_average[0], adc1_dma_buf_average[0], adc1_dma_buf_average[0] - adc1_dma_initial_buf_average[0]);
    myprintf("GLOVE_COMMANDS: %d, %d, %d, %d, %d, %d, %d\r\n",
             glove_commands[0], glove_commands[1], glove_commands[2], glove_commands[3],
             glove_commands[4], glove_commands[5], glove_commands[6]);
    myprintf("HAND_POSE: %s %s\r\n", FingerDirection_ToString(s_hand_pose.fingers), HandFace_ToString(s_hand_pose.face));
    myprintf("ROLL_X_1to10: %u\r\n", (unsigned)s_roll_x_1to10);
    Glove_UART2_SendCommands_DMA();
    HAL_Delay(500);

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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.NbrOfConversion = 4;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_9;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_12;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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
  hi2c1.Init.Timing = 0x00B07CB4;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LD3_Pin */
  GPIO_InitStruct.Pin = LD3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
