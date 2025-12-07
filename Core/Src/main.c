/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (CMSIS-RTOS2 benchmark, renamed vars)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

/* Definitions for Task1_FunctionCall */
osThreadId_t Task1_FunctionCallHandle;
const osThreadAttr_t Task1_FunctionCall_attributes = {
  .name = "Task1_FunctionCall",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/* Definitions for Task2_SVC_Call */
osThreadId_t Task2_SVC_CallHandle;
const osThreadAttr_t Task2_SVC_Call_attributes = {
  .name = "Task2_SVC_Call",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/* Monitor task */
osThreadId_t MonitorHandle;
const osThreadAttr_t Monitor_attributes = {
  .name = "Monitor",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};

/* USER CODE BEGIN PV */
/* Renamed minimal benchmark globals (volatile so debugger reads live) */
volatile uint32_t rtos_call_count = 0;         /* RTOS (syscall) counter */
volatile uint32_t function_call_count = 0;     /* Function-call (cooperative) counter */
volatile uint8_t test_running = 0;              /* 0 = idle, 1 = test running */

/* DWT timing helpers */
static uint32_t cycles_per_us = 0;
static uint32_t cycles_per_sec = 0;

/* DWT end_cycle to stop workers cleanly */
volatile uint32_t end_cycle = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void StartTask_FunctionCall(void *argument);
void StartTask_SVC_Call(void *argument);
void MonitorTask(void *argument);

/* USER CODE BEGIN 0 */
/* DWT init — enable cycle counter and compute scaling */
static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; /* enable trace */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            /* enable cycle counter */
    DWT->CYCCNT = 0;

    /* compute cycles per microsecond and per second (rounded) */
    cycles_per_us  = (SystemCoreClock + 500000U) / 1000000U;
    cycles_per_sec = SystemCoreClock;
}

/* Precise 1 microsecond busy-wait using DWT->CYCCNT */
static inline void Burn_1us(void)
{
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles_per_us)
    {
        __NOP();
    }
}

/* Cooperative baseline: run two logical tasks sequentially for 1 second */
static void run_function_call_test(void)
{
    uint32_t start = DWT->CYCCNT;
    function_call_count = 0;

    /* run for exactly 1 second using DWT */
    while ((int32_t)(DWT->CYCCNT - start) < (int32_t)cycles_per_sec)
    {
        /* Logical Task A (FunctionCall) */
        Burn_1us();
        function_call_count++;   /* counted as A */

        /* Logical Task B (SVC_Call logical) */
        Burn_1us();
        function_call_count++;   /* counted as B */
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  /* Initialize DWT for accurate timing */
  DWT_Init();

  MX_GPIO_Init();

  /* --- Run cooperative baseline (no RTOS scheduler active) --- */
  test_running = 1;
  run_function_call_test();
  test_running = 0; /* cooperative finished; results in function_call_count */

  /* Init scheduler and create RTOS tasks */
  osKernelInitialize();

  Task1_FunctionCallHandle = osThreadNew(StartTask_FunctionCall, NULL, &Task1_FunctionCall_attributes);
  Task2_SVC_CallHandle    = osThreadNew(StartTask_SVC_Call, NULL, &Task2_SVC_Call_attributes);
  MonitorHandle           = osThreadNew(MonitorTask, NULL, &Monitor_attributes);

  BSP_LED_Init(LED_GREEN);
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Start scheduler */
  osKernelStart();

  /* Should never reach here */
  while (1) { }
}

/* USER CODE BEGIN 4 */

/* Worker Task 1: named FunctionCall (but this is the RTOS task doing syscall) */
void StartTask_FunctionCall(void *argument)
{
    (void) argument;

    for (;;)
    {
        if (test_running)
        {
            Burn_1us();
            rtos_call_count++;

            /* If end_cycle reached, exit cleanly */
            if ((int32_t)(DWT->CYCCNT - end_cycle) >= 0)
            {
                osThreadExit();
            }

            osThreadYield(); /* SVC/syscall via RTOS scheduler */
        }
        else
        {
            osDelay(1);
        }
    }
}

/* Worker Task 2: named SVC_Call (also performs syscall yielding) */
void StartTask_SVC_Call(void *argument)
{
    (void) argument;

    for (;;)
    {
        if (test_running)
        {
            Burn_1us();
            rtos_call_count++;

            if ((int32_t)(DWT->CYCCNT - end_cycle) >= 0)
            {
                osThreadExit();
            }

            osThreadYield();
        }
        else
        {
            osDelay(1);
        }
    }
}

/* MonitorTask: compute end_cycle and start the RTOS 1s run; workers self-exit at end_cycle */
void MonitorTask(void *argument)
{
    (void) argument;

    osDelay(10); /* let tasks be ready */

    uint32_t now = DWT->CYCCNT;
    end_cycle = now + cycles_per_sec; /* run for 1 second */

    rtos_call_count = 0;
    test_running = 1;

    /* Wait until end_cycle (coarse sleep) */
    while ((int32_t)(DWT->CYCCNT - end_cycle) < 0)
    {
        osDelay(10);
    }

    /* Ensure termination */
    test_running = 0;
    if (Task1_FunctionCallHandle != NULL) { osThreadTerminate(Task1_FunctionCallHandle); Task1_FunctionCallHandle = NULL; }
    if (Task2_SVC_CallHandle != NULL)    { osThreadTerminate(Task2_SVC_CallHandle);    Task2_SVC_CallHandle    = NULL; }

    /* Idle forever so debugger can inspect final counts */
    for (;;)
    {
        osDelay(1000);
    }
}
/* USER CODE END 4 */

/* Minimal SystemClock_Config and MX_GPIO_Init (keep or replace with CubeMX versions) */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) { Error_Handler(); }
  SystemCoreClockUpdate();
}

static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
}

/* HAL callback and error handler left as template */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6) { HAL_IncTick(); }
}
void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { (void)file; (void)line; }
#endif
