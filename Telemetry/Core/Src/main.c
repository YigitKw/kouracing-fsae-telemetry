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
#include "cmsis_os.h"
#include "fatfs.h"
#include "lwip.h"
#include "usb_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lv_port_disp.h"
#include "lvgl.h"
#include "lwip/udp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// --- FAULT MONITORING EÃƒâ€¦Ã‚ÂÃƒâ€Ã‚Â°K DEÃƒâ€Ã‚ÂERLERÃƒâ€Ã‚Â° ---
#define MOTOR_TEMP_WARNING 75    // Ãƒâ€šÃ‚Â°C - uyarÃƒâ€Ã‚Â±
#define MOTOR_TEMP_CRITICAL 85   // Ãƒâ€šÃ‚Â°C - kritik
#define BATTERY_VOLT_LOW 340.0f  // V - dÃƒÆ’Ã‚Â¼Ãƒâ€¦Ã…Â¸ÃƒÆ’Ã‚Â¼k voltaj
#define BATTERY_VOLT_HIGH 380.0f // V - yÃƒÆ’Ã‚Â¼ksek voltaj
#define RPM_REDLINE 9500         // RPM - kÃƒâ€Ã‚Â±rmÃƒâ€Ã‚Â±zÃƒâ€Ã‚Â± bÃƒÆ’Ã‚Â¶lge

// --- RASPBERRY PI AÃƒâ€Ã‚Â AYARLARI ---
#define RPI_IP_1 192
#define RPI_IP_2 168
#define RPI_IP_3 1
#define RPI_IP_4 2
#define RPI_UDP_PORT 5000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc3;

CRC_HandleTypeDef hcrc;

DCMI_HandleTypeDef hdcmi;

DMA2D_HandleTypeDef hdma2d;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c3;

LTDC_HandleTypeDef hltdc;

QSPI_HandleTypeDef hqspi;

RTC_HandleTypeDef hrtc;

SAI_HandleTypeDef hsai_BlockA2;
SAI_HandleTypeDef hsai_BlockB2;

SD_HandleTypeDef hsd1;

SPDIFRX_HandleTypeDef hspdif;

SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim8;
TIM_HandleTypeDef htim12;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart6;

SDRAM_HandleTypeDef hsdram1;

osThreadId defaultTaskHandle;
/* USER CODE BEGIN PV */
typedef struct {
  uint16_t rpm;
  uint8_t speed;
  int8_t motor_temp;       // MTR
  float battery_voltage;
  float throttle;          // 0.0-1.0
  float brake;             // 0.0-1.0
  char *vehicle_state;
  char *inverter_status;
  char *battery_status;
  uint8_t fault;
  uint8_t traction_control;
  int8_t bat_temp;         // battery temp
  int8_t inv_temp;         // inverter temp
  int8_t oil_temp;         // oil temp
  int8_t wtr_temp;         // water/coolant temp
  int8_t tire_temp_left;   // rear left tire
  int8_t tire_temp_right;  // rear right tire
  uint16_t power_watts;    // instantaneous power
  uint8_t bat_percent;     // battery SOC %
  uint8_t regen_percent;   // regenerative braking %
  uint8_t tcs_percent;     // traction control %
  uint8_t ready;           // 0=NOT READY, 1=READY
  char *mode;              // ENDR, SKID, DRAG, CROSS
} TelemetryData;

TelemetryData myCar;
char json_buffer[1024];
volatile uint8_t new_data_received = 0;
static uint32_t lap_time_ms = 0;
volatile uint8_t current_screen = 0; // 0=dashboard, 1=screen2
static uint8_t btn_prev = 1;         // debounce: previous button state

// LVGL Objeleri
#define RPM_LED_COUNT 12
#define RPM_MAX_VALUE 7500
#define TC_LED_COUNT  9

// ========== AYIRICI BEYAZ DIKDORTGEN AYARLARI ==========
// Kalinlik (piksel)
#define DIV_THICKNESS     2
// Renk (beyaz)
#define DIV_COLOR         0xFFFFFF
// Opaklık (0-255, 255=tam opak)
#define DIV_OPACITY       LV_OPA_COVER

// --- Yatay cizgiler (soldan saga) ---
// Ust bilgi satiri altindaki yatay cizgi
#define DIV_H1_X          0
#define DIV_H1_Y          56
#define DIV_H1_W          480

// Alt TC LED satiri ustundeki yatay cizgi
#define DIV_H2_X          0
#define DIV_H2_Y          236
#define DIV_H2_W          480

// --- Dikey cizgiler (yukaridan asagi) ---
// Sol sicaklik kolonu sag kenari
#define DIV_V1_X          133
#define DIV_V1_Y          56
#define DIV_V1_H          180

// Sol fren bari sag kenari (fren+lastik | hiz merkez)
#define DIV_V2_X          163
#define DIV_V2_Y          56
#define DIV_V2_H          180

// Sag gaz bari sol kenari (hiz merkez | gaz+lastik)
#define DIV_V3_X          315
#define DIV_V3_Y          56
#define DIV_V3_H          180

// Sag MODE/FAULT kolonu sol kenari
#define DIV_V4_X          345
#define DIV_V4_Y          56
#define DIV_V4_H          180
lv_obj_t *rpm_leds[RPM_LED_COUNT];
lv_obj_t *rpm_label;
lv_obj_t *regen_label;
lv_obj_t *tcs_label;
lv_obj_t *speed_label;
lv_obj_t *bat_temp_lbl;
lv_obj_t *inv_temp_lbl;
lv_obj_t *mtr_temp_lbl;
lv_obj_t *oil_temp_lbl;
lv_obj_t *wtr_temp_lbl;
lv_obj_t *tire_left_lbl;
lv_obj_t *tire_right_lbl;
lv_obj_t *brake_bar;
lv_obj_t *brake_pct_lbl;
lv_obj_t *throttle_bar;
lv_obj_t *throttle_pct_lbl;
lv_obj_t *laptime_lbl;
lv_obj_t *bat_pct_lbl;
lv_obj_t *power_lbl;
lv_obj_t *mode_title_lbl;
lv_obj_t *mode_value_lbl;
lv_obj_t *fault_label;
lv_obj_t *ready_label;
lv_obj_t *tc_leds[TC_LED_COUNT];
lv_obj_t *scr1_cont; // screen 1 container
lv_obj_t *scr2_cont; // screen 2 container
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC3_Init(void);
static void MX_CRC_Init(void);
static void MX_DCMI_Init(void);
static void MX_DMA2D_Init(void);
static void MX_FMC_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C3_Init(void);
static void MX_LTDC_Init(void);
static void MX_QUADSPI_Init(void);
static void MX_RTC_Init(void);
static void MX_SAI2_Init(void);
static void MX_SDMMC1_SD_Init(void);
static void MX_SPDIFRX_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM5_Init(void);
static void MX_TIM8_Init(void);
static void MX_TIM12_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART6_UART_Init(void);
void StartDefaultTask(void const *argument);

/* USER CODE BEGIN PFP */
void Create_KOURacing_Dashboard(void);
void Create_Screen2(void);
void Show_Splash_Screen(void);
void Refresh_Dashboard_UI(void);
void Refresh_Screen2_UI(void);
void UDP_Server_Init(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void Update_Telemetry(void) {
  // Ana sensÃƒÆ’Ã‚Â¶rler
  myCar.rpm = 1500 + (rand() % 6000);        // 1500-7500
  myCar.speed = 60 + (rand() % 80);           // 60-140 km/h
  myCar.motor_temp = 55 + (rand() % 40);      // MTR 55-95
  myCar.battery_voltage = 345.0f + ((rand() % 400) / 10.0f);
  myCar.throttle = (rand() % 101) / 100.0f;   // 0-100%
  myCar.brake = (rand() % 101) / 100.0f;      // 0-100%

  // SÃƒâ€Ã‚Â±caklÃƒâ€Ã‚Â±klar
  myCar.bat_temp = 35 + (rand() % 25);         // BAT 35-60
  myCar.inv_temp = 40 + (rand() % 30);         // INV 40-70
  myCar.oil_temp = 70 + (rand() % 35);         // OIL 70-105
  myCar.wtr_temp = 55 + (rand() % 30);         // WTR 55-85
  myCar.tire_temp_left  = 45 + (rand() % 45);  // Arka sol lastik 45-90
  myCar.tire_temp_right = 45 + (rand() % 45);  // Arka sag lastik 45-90

  // GÃƒÆ’Ã‚Â¼ÃƒÆ’Ã‚Â§ & Batarya
  myCar.power_watts = 50 + (rand() % 250);     // 50-300W
  myCar.bat_percent = 30 + (rand() % 60);      // 30-90%
  myCar.regen_percent = rand() % 100;           // REG 0-100%
  myCar.tcs_percent = rand() % 60;              // TCS 0-60%

  // Mode: her ~5 saniyede deÃƒâ€Ã…Â¸iÃƒâ€¦Ã…Â¸ (10Hz'de 50 tick)
  {
    static uint8_t mode_cnt = 0;
    static uint8_t mode_idx = 0;
    const char *modes[] = {"ENDR", "SKID", "DRAG", "CROSS"};
    if (++mode_cnt >= 50) { mode_cnt = 0; mode_idx = (mode_idx + 1) % 4; }
    myCar.mode = (char *)modes[mode_idx];
  }

  // Ready durumu: fault yoksa READY
  myCar.fault = 0;
  myCar.vehicle_state = "Drive";
  myCar.inverter_status = "Active";
  myCar.battery_status = "Normal";

  if (myCar.motor_temp >= MOTOR_TEMP_CRITICAL) {
    myCar.fault = 1;
    myCar.vehicle_state = "Fault";
    myCar.inverter_status = "Overheat";
  } else if (myCar.motor_temp >= MOTOR_TEMP_WARNING) {
    myCar.inverter_status = "Warning";
  }
  if (myCar.battery_voltage < BATTERY_VOLT_LOW) {
    myCar.fault = 1;
    myCar.vehicle_state = "Fault";
    myCar.battery_status = "Low Voltage";
  } else if (myCar.battery_voltage > BATTERY_VOLT_HIGH) {
    myCar.fault = 1;
    myCar.vehicle_state = "Fault";
    myCar.battery_status = "High Voltage";
  }
  if (myCar.rpm > RPM_REDLINE) {
    myCar.inverter_status = "Redline";
  }

  myCar.ready = myCar.fault ? 0 : 1;

  // Traction Control
  if (myCar.throttle > 0.7f && myCar.speed < 60) {
    myCar.traction_control = 1;
  } else if (rand() % 10 == 0) {
    myCar.traction_control = 1;
  } else {
    myCar.traction_control = 0;
  }

  // Lap timer (10Hz = 100ms/tick)
  lap_time_ms += 100;
  if (lap_time_ms >= 600000) lap_time_ms = 0;
}

// --- UDP GÃƒÆ’Ã¢â‚¬â€œNDERÃƒâ€Ã‚Â°M (Persistent PCB ile optimize) ---
static struct udp_pcb *udp_send_pcb = NULL;
static ip_addr_t rpi_addr;

void Send_Telemetry_UDP(char *message) {
  // PCB'yi bir kez oluÃƒâ€¦Ã…Â¸tur, tekrar kullan
  if (udp_send_pcb == NULL) {
    udp_send_pcb = udp_new();
    IP4_ADDR(&rpi_addr, RPI_IP_1, RPI_IP_2, RPI_IP_3, RPI_IP_4);
  }

  if (udp_send_pcb != NULL) {
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, strlen(message), PBUF_POOL);
    if (p != NULL) {
      pbuf_take(p, message, strlen(message));
      udp_sendto(udp_send_pcb, p, &rpi_addr, RPI_UDP_PORT);
      pbuf_free(p);
    }
  }
}

// --- UDP ALIM (Raspberry Pi'den veri okuma) ---
static struct udp_pcb *udp_server_pcb;

// Basit JSON parse:
// {"rpm":8500,"speed":95,"temp":65,"volt":359.2,"throttle":0.75,"state":"Drive","fault":0}
static int parse_int_field(const char *json, const char *key) {
  char search[32];
  snprintf(search, sizeof(search), "\"%s\":", key);
  const char *p = strstr(json, search);
  if (p) {
    p += strlen(search);
    return atoi(p);
  }
  return 0;
}

static float parse_float_field(const char *json, const char *key) {
  char search[32];
  snprintf(search, sizeof(search), "\"%s\":", key);
  const char *p = strstr(json, search);
  if (p) {
    p += strlen(search);
    return (float)atof(p);
  }
  return 0.0f;
}

static void parse_string_field(const char *json, const char *key, char *out,
                               int maxlen) {
  char search[32];
  snprintf(search, sizeof(search), "\"%s\":\"", key);
  const char *p = strstr(json, search);
  if (p) {
    p += strlen(search);
    int i = 0;
    while (*p && *p != '"' && i < maxlen - 1) {
      out[i++] = *p++;
    }
    out[i] = '\0';
  }
}

static char vehicle_state_buf[16] = "Ready"; // State string buffer

void udp_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                          const ip_addr_t *addr, u16_t port) {
  if (p != NULL) {
    // Gelen veriyi null-terminated string'e ÃƒÆ’Ã‚Â§evir
    char buf[256];
    uint16_t len =
        (p->tot_len < sizeof(buf) - 1) ? p->tot_len : sizeof(buf) - 1;
    pbuf_copy_partial(p, buf, len, 0);
    buf[len] = '\0';

    // JSON parse et ve TelemetryData'ya yaz
    myCar.rpm = (uint16_t)parse_int_field(buf, "rpm");
    myCar.speed = (uint8_t)parse_int_field(buf, "speed");
    myCar.motor_temp = (int8_t)parse_int_field(buf, "temp");
    myCar.battery_voltage = parse_float_field(buf, "volt");
    myCar.throttle = parse_float_field(buf, "throttle");
    myCar.fault = (uint8_t)parse_int_field(buf, "fault");

    parse_string_field(buf, "state", vehicle_state_buf,
                       sizeof(vehicle_state_buf));
    myCar.vehicle_state = vehicle_state_buf;

    new_data_received = 1;

    pbuf_free(p);
  }
}

void UDP_Server_Init(void) {
  udp_server_pcb = udp_new();
  if (udp_server_pcb != NULL) {
    err_t err =
        udp_bind(udp_server_pcb, IP_ADDR_ANY, 5000); // Port 5000'den dinle
    if (err == ERR_OK) {
      udp_recv(udp_server_pcb, udp_receive_callback, NULL);
    }
  }
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */
  // MPU_Config();
  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  // MPU_Config();  // DEVRE DIÃƒâ€¦Ã‚ÂI: FMC register alanÃƒâ€Ã‚Â±nÃƒâ€Ã‚Â± (0xA0000000) engelliyor
  // ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ HardFault

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC3_Init();
  MX_CRC_Init();
  MX_DCMI_Init();
  MX_DMA2D_Init();
  MX_FMC_Init();
  MX_I2C1_Init();
  MX_I2C3_Init();
  MX_LTDC_Init();
  MX_QUADSPI_Init();
  MX_RTC_Init();
  MX_SAI2_Init();
  MX_SDMMC1_SD_Init();
  MX_SPDIFRX_Init();
  MX_SPI2_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM5_Init();
  MX_TIM8_Init();
  MX_TIM12_Init();
  MX_USART1_UART_Init();
  MX_USART6_UART_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 4096);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure LSE Drive Capability
   */
  HAL_PWR_EnableBkUpAccess();

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 400;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
   */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief Peripherals Common Clock Configuration
 * @retval None
 */
void PeriphCommonClock_Config(void) {
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
   */
  PeriphClkInitStruct.PeriphClockSelection =
      RCC_PERIPHCLK_LTDC | RCC_PERIPHCLK_SAI2 | RCC_PERIPHCLK_SDMMC1 |
      RCC_PERIPHCLK_CLK48;
  PeriphClkInitStruct.PLLSAI.PLLSAIN = 384;
  PeriphClkInitStruct.PLLSAI.PLLSAIR = 5;
  PeriphClkInitStruct.PLLSAI.PLLSAIQ = 2;
  PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV8;
  PeriphClkInitStruct.PLLSAIDivQ = 1;
  PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_8;
  PeriphClkInitStruct.Sai2ClockSelection = RCC_SAI2CLKSOURCE_PLLSAI;
  PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48SOURCE_PLLSAIP;
  PeriphClkInitStruct.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_CLK48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief ADC3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC3_Init(void) {

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data
   * Alignment and number of conversion)
   */
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc3.Init.Resolution = ADC_RESOLUTION_12B;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DMAContinuousRequests = DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc3) != HAL_OK) {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in
   * the sequencer and its sample time.
   */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */
}

/**
 * @brief CRC Initialization Function
 * @param None
 * @retval None
 */
static void MX_CRC_Init(void) {

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
  if (HAL_CRC_Init(&hcrc) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */
}

/**
 * @brief DCMI Initialization Function
 * @param None
 * @retval None
 */
static void MX_DCMI_Init(void) {

  /* USER CODE BEGIN DCMI_Init 0 */

  /* USER CODE END DCMI_Init 0 */

  /* USER CODE BEGIN DCMI_Init 1 */

  /* USER CODE END DCMI_Init 1 */
  hdcmi.Instance = DCMI;
  hdcmi.Init.SynchroMode = DCMI_SYNCHRO_HARDWARE;
  hdcmi.Init.PCKPolarity = DCMI_PCKPOLARITY_FALLING;
  hdcmi.Init.VSPolarity = DCMI_VSPOLARITY_LOW;
  hdcmi.Init.HSPolarity = DCMI_HSPOLARITY_LOW;
  hdcmi.Init.CaptureRate = DCMI_CR_ALL_FRAME;
  hdcmi.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;
  hdcmi.Init.JPEGMode = DCMI_JPEG_DISABLE;
  hdcmi.Init.ByteSelectMode = DCMI_BSM_ALL;
  hdcmi.Init.ByteSelectStart = DCMI_OEBS_ODD;
  hdcmi.Init.LineSelectMode = DCMI_LSM_ALL;
  hdcmi.Init.LineSelectStart = DCMI_OELS_ODD;
  if (HAL_DCMI_Init(&hdcmi) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN DCMI_Init 2 */

  /* USER CODE END DCMI_Init 2 */
}

/**
 * @brief DMA2D Initialization Function
 * @param None
 * @retval None
 */
static void MX_DMA2D_Init(void) {

  /* USER CODE BEGIN DMA2D_Init 0 */

  /* USER CODE END DMA2D_Init 0 */

  /* USER CODE BEGIN DMA2D_Init 1 */

  /* USER CODE END DMA2D_Init 1 */
  hdma2d.Instance = DMA2D;
  hdma2d.Init.Mode = DMA2D_M2M;
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_ARGB8888;
  hdma2d.Init.OutputOffset = 0;
  hdma2d.LayerCfg[1].InputOffset = 0;
  hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_ARGB8888;
  hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[1].InputAlpha = 0;
  if (HAL_DMA2D_Init(&hdma2d) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_DMA2D_ConfigLayer(&hdma2d, 1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN DMA2D_Init 2 */

  /* USER CODE END DMA2D_Init 2 */
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00C0EAFF;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Analogue filter
   */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Digital filter
   */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
}

/**
 * @brief I2C3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C3_Init(void) {

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x00C0EAFF;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Analogue filter
   */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Digital filter
   */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */
}

/**
 * @brief LTDC Initialization Function
 * @param None
 * @retval None
 */
static void MX_LTDC_Init(void) {

  /* USER CODE BEGIN LTDC_Init 0 */

  /* USER CODE END LTDC_Init 0 */

  LTDC_LayerCfgTypeDef pLayerCfg = {0};

  /* USER CODE BEGIN LTDC_Init 1 */

  /* USER CODE END LTDC_Init 1 */
  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hltdc.Init.HorizontalSync = 40;
  hltdc.Init.VerticalSync = 9;
  hltdc.Init.AccumulatedHBP = 53;
  hltdc.Init.AccumulatedVBP = 11;
  hltdc.Init.AccumulatedActiveW = 533;
  hltdc.Init.AccumulatedActiveH = 283;
  hltdc.Init.TotalWidth = 565;
  hltdc.Init.TotalHeigh = 285;
  hltdc.Init.Backcolor.Blue = 0;
  hltdc.Init.Backcolor.Green = 0;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK) {
    Error_Handler();
  }
  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = 480;
  pLayerCfg.WindowY0 = 0;
  pLayerCfg.WindowY1 = 272;
  pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
  pLayerCfg.Alpha = 255;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
  pLayerCfg.FBStartAdress = 0xC0000000;
  pLayerCfg.ImageWidth = 480;
  pLayerCfg.ImageHeight = 272;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN LTDC_Init 2 */

  /* USER CODE END LTDC_Init 2 */
}

/**
 * @brief QUADSPI Initialization Function
 * @param None
 * @retval None
 */
static void MX_QUADSPI_Init(void) {

  /* USER CODE BEGIN QUADSPI_Init 0 */

  /* USER CODE END QUADSPI_Init 0 */

  /* USER CODE BEGIN QUADSPI_Init 1 */

  /* USER CODE END QUADSPI_Init 1 */
  /* QUADSPI parameter configuration*/
  hqspi.Instance = QUADSPI;
  hqspi.Init.ClockPrescaler = 1;
  hqspi.Init.FifoThreshold = 4;
  hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
  hqspi.Init.FlashSize = 24;
  hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_6_CYCLE;
  hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
  hqspi.Init.FlashID = QSPI_FLASH_ID_1;
  hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
  if (HAL_QSPI_Init(&hqspi) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN QUADSPI_Init 2 */

  /* USER CODE END QUADSPI_Init 2 */
}

/**
 * @brief RTC Initialization Function
 * @param None
 * @retval None
 */
static void MX_RTC_Init(void) {

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};
  RTC_AlarmTypeDef sAlarm = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
   */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK) {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
   */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK) {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;
  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK) {
    Error_Handler();
  }

  /** Enable the Alarm A
   */
  sAlarm.AlarmTime.Hours = 0x0;
  sAlarm.AlarmTime.Minutes = 0x0;
  sAlarm.AlarmTime.Seconds = 0x0;
  sAlarm.AlarmTime.SubSeconds = 0x0;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
  sAlarm.AlarmDateWeekDay = 0x1;
  sAlarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm(&hrtc, &sAlarm, RTC_FORMAT_BCD) != HAL_OK) {
    Error_Handler();
  }

  /** Enable the Alarm B
   */
  sAlarm.Alarm = RTC_ALARM_B;
  if (HAL_RTC_SetAlarm(&hrtc, &sAlarm, RTC_FORMAT_BCD) != HAL_OK) {
    Error_Handler();
  }

  /** Enable the TimeStamp
   */
  if (HAL_RTCEx_SetTimeStamp(&hrtc, RTC_TIMESTAMPEDGE_RISING,
                             RTC_TIMESTAMPPIN_POS1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */
}

/**
 * @brief SAI2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SAI2_Init(void) {

  /* USER CODE BEGIN SAI2_Init 0 */

  /* USER CODE END SAI2_Init 0 */

  /* USER CODE BEGIN SAI2_Init 1 */

  /* USER CODE END SAI2_Init 1 */
  hsai_BlockA2.Instance = SAI2_Block_A;
  hsai_BlockA2.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockA2.Init.AudioMode = SAI_MODEMASTER_TX;
  hsai_BlockA2.Init.DataSize = SAI_DATASIZE_8;
  hsai_BlockA2.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockA2.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
  hsai_BlockA2.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockA2.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockA2.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
  hsai_BlockA2.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockA2.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_192K;
  hsai_BlockA2.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA2.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA2.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA2.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockA2.FrameInit.FrameLength = 8;
  hsai_BlockA2.FrameInit.ActiveFrameLength = 1;
  hsai_BlockA2.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
  hsai_BlockA2.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
  hsai_BlockA2.FrameInit.FSOffset = SAI_FS_FIRSTBIT;
  hsai_BlockA2.SlotInit.FirstBitOffset = 0;
  hsai_BlockA2.SlotInit.SlotSize = SAI_SLOTSIZE_DATASIZE;
  hsai_BlockA2.SlotInit.SlotNumber = 1;
  hsai_BlockA2.SlotInit.SlotActive = 0x00000000;
  if (HAL_SAI_Init(&hsai_BlockA2) != HAL_OK) {
    Error_Handler();
  }
  hsai_BlockB2.Instance = SAI2_Block_B;
  hsai_BlockB2.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockB2.Init.AudioMode = SAI_MODESLAVE_RX;
  hsai_BlockB2.Init.DataSize = SAI_DATASIZE_8;
  hsai_BlockB2.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockB2.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
  hsai_BlockB2.Init.Synchro = SAI_SYNCHRONOUS;
  hsai_BlockB2.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockB2.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockB2.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockB2.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockB2.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockB2.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockB2.FrameInit.FrameLength = 8;
  hsai_BlockB2.FrameInit.ActiveFrameLength = 1;
  hsai_BlockB2.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
  hsai_BlockB2.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
  hsai_BlockB2.FrameInit.FSOffset = SAI_FS_FIRSTBIT;
  hsai_BlockB2.SlotInit.FirstBitOffset = 0;
  hsai_BlockB2.SlotInit.SlotSize = SAI_SLOTSIZE_DATASIZE;
  hsai_BlockB2.SlotInit.SlotNumber = 1;
  hsai_BlockB2.SlotInit.SlotActive = 0x00000000;
  if (HAL_SAI_Init(&hsai_BlockB2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN SAI2_Init 2 */

  /* USER CODE END SAI2_Init 2 */
}

/**
 * @brief SDMMC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SDMMC1_SD_Init(void) {

  /* USER CODE BEGIN SDMMC1_Init 0 */

  /* USER CODE END SDMMC1_Init 0 */

  /* USER CODE BEGIN SDMMC1_Init 1 */

  /* USER CODE END SDMMC1_Init 1 */
  hsd1.Instance = SDMMC1;
  hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
  hsd1.Init.ClockBypass = SDMMC_CLOCK_BYPASS_DISABLE;
  hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  hsd1.Init.BusWide = SDMMC_BUS_WIDE_1B;
  hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd1.Init.ClockDiv = 0;
  /* USER CODE BEGIN SDMMC1_Init 2 */

  /* USER CODE END SDMMC1_Init 2 */
}

/**
 * @brief SPDIFRX Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPDIFRX_Init(void) {

  /* USER CODE BEGIN SPDIFRX_Init 0 */

  /* USER CODE END SPDIFRX_Init 0 */

  /* USER CODE BEGIN SPDIFRX_Init 1 */

  /* USER CODE END SPDIFRX_Init 1 */
  hspdif.Instance = SPDIFRX;
  hspdif.Init.InputSelection = SPDIFRX_INPUT_IN0;
  hspdif.Init.Retries = SPDIFRX_MAXRETRIES_NONE;
  hspdif.Init.WaitForActivity = SPDIFRX_WAITFORACTIVITY_OFF;
  hspdif.Init.ChannelSelection = SPDIFRX_CHANNEL_A;
  hspdif.Init.DataFormat = SPDIFRX_DATAFORMAT_LSB;
  hspdif.Init.StereoMode = SPDIFRX_STEREOMODE_DISABLE;
  hspdif.Init.PreambleTypeMask = SPDIFRX_PREAMBLETYPEMASK_OFF;
  hspdif.Init.ChannelStatusMask = SPDIFRX_CHANNELSTATUS_OFF;
  hspdif.Init.ValidityBitMask = SPDIFRX_VALIDITYMASK_OFF;
  hspdif.Init.ParityErrorMask = SPDIFRX_PARITYERRORMASK_OFF;
  if (HAL_SPDIFRX_Init(&hspdif) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN SPDIFRX_Init 2 */

  /* USER CODE END SPDIFRX_Init 2 */
}

/**
 * @brief SPI2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI2_Init(void) {

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_4BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */
}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void) {

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK) {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);
}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);
}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void) {

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);
}

/**
 * @brief TIM5 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM5_Init(void) {

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 4294967295;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim5) != HAL_OK) {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim5) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim5, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

  /* USER CODE END TIM5_Init 2 */
  HAL_TIM_MspPostInit(&htim5);
}

/**
 * @brief TIM8 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM8_Init(void) {

  /* USER CODE BEGIN TIM8_Init 0 */

  /* USER CODE END TIM8_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM8_Init 1 */

  /* USER CODE END TIM8_Init 1 */
  htim8.Instance = TIM8;
  htim8.Init.Prescaler = 0;
  htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim8.Init.Period = 65535;
  htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim8.Init.RepetitionCounter = 0;
  htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim8) != HAL_OK) {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim8, &sClockSourceConfig) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM8_Init 2 */

  /* USER CODE END TIM8_Init 2 */
}

/**
 * @brief TIM12 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM12_Init(void) {

  /* USER CODE BEGIN TIM12_Init 0 */

  /* USER CODE END TIM12_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM12_Init 1 */

  /* USER CODE END TIM12_Init 1 */
  htim12.Instance = TIM12;
  htim12.Init.Prescaler = 0;
  htim12.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim12.Init.Period = 65535;
  htim12.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim12.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim12) != HAL_OK) {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim12, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM12_Init 2 */

  /* USER CODE END TIM12_Init 2 */
  HAL_TIM_MspPostInit(&htim12);
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

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
  if (HAL_UART_Init(&huart1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */
}

/**
 * @brief USART6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART6_UART_Init(void) {

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  huart6.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart6.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart6) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */
}

/* FMC initialization function */
static void MX_FMC_Init(void) {

  /* USER CODE BEGIN FMC_Init 0 */
  __HAL_RCC_FMC_CLK_ENABLE(); // <-- SADECE BURAYA YAZIYORUZ
  /* USER CODE END FMC_Init 0 */

  FMC_SDRAM_TimingTypeDef SdramTiming = {0};

  /* USER CODE BEGIN FMC_Init 1 */

  /* USER CODE END FMC_Init 1 */

  /** Perform the SDRAM1 memory initialization sequence
   */
  hsdram1.Instance = FMC_SDRAM_DEVICE;
  /* hsdram1.Init */
  hsdram1.Init.SDBank = FMC_SDRAM_BANK1;
  hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_8;
  hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
  hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
  hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_3;
  hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
  hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;
  hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_0;
  /* SdramTiming */
  SdramTiming.LoadToActiveDelay = 2;
  SdramTiming.ExitSelfRefreshDelay = 7;
  SdramTiming.SelfRefreshTime = 4;
  SdramTiming.RowCycleDelay = 7;
  SdramTiming.WriteRecoveryTime = 3;
  SdramTiming.RPDelay = 2;
  SdramTiming.RCDDelay = 2;

  if (HAL_SDRAM_Init(&hsdram1, &SdramTiming) != HAL_OK) {
    Error_Handler();
  }

  /* USER CODE BEGIN FMC_Init 2 */
  // --- SDRAM Initialization Sequence ---
  FMC_SDRAM_CommandTypeDef Command;

  // 1. Clock Configuration Enable
  Command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
  Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber = 1;
  Command.ModeRegisterDefinition = 0;
  HAL_SDRAM_SendCommand(&hsdram1, &Command, 0xFFF);
  HAL_Delay(1);

  // 2. PALL (Precharge All)
  Command.CommandMode = FMC_SDRAM_CMD_PALL;
  Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber = 1;
  Command.ModeRegisterDefinition = 0;
  HAL_SDRAM_SendCommand(&hsdram1, &Command, 0xFFF);

  // 3. Auto-Refresh (8 cycles)
  Command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber = 8;
  Command.ModeRegisterDefinition = 0;
  HAL_SDRAM_SendCommand(&hsdram1, &Command, 0xFFF);

// 4. Load Mode Register: CAS Latency=3, Burst Length=1, Sequential
#define SDRAM_MODEREG_BURST_LENGTH_1 ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL ((uint16_t)0x0000)
#define SDRAM_MODEREG_CAS_LATENCY_3 ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE ((uint16_t)0x0200)

  Command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
  Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber = 1;
  Command.ModeRegisterDefinition =
      SDRAM_MODEREG_BURST_LENGTH_1 | SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
      SDRAM_MODEREG_CAS_LATENCY_3 | SDRAM_MODEREG_OPERATING_MODE_STANDARD |
      SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;
  HAL_SDRAM_SendCommand(&hsdram1, &Command, 0xFFF);

  // 5. Set Refresh Rate (HCLK=200MHz, SDCLK=100MHz)
  HAL_SDRAM_ProgramRefreshRate(&hsdram1, 0x0603);
  /* USER CODE END FMC_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOJ_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOK_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin,
                    GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOI, ARDUINO_D7_Pin | ARDUINO_D8_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_Port, LCD_BL_CTRL_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_DISP_GPIO_Port, LCD_DISP_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DCMI_PWR_EN_GPIO_Port, DCMI_PWR_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, ARDUINO_D4_Pin | ARDUINO_D2_Pin | EXT_RST_Pin,
                    GPIO_PIN_RESET);

  /*Configure GPIO pin : OTG_HS_OverCurrent_Pin */
  GPIO_InitStruct.Pin = OTG_HS_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_HS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ULPI_D7_Pin ULPI_D6_Pin ULPI_D5_Pin ULPI_D3_Pin
                           ULPI_D2_Pin ULPI_D1_Pin ULPI_D4_Pin */
  GPIO_InitStruct.Pin = ULPI_D7_Pin | ULPI_D6_Pin | ULPI_D5_Pin | ULPI_D3_Pin |
                        ULPI_D2_Pin | ULPI_D1_Pin | ULPI_D4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_VBUS_Pin */
  GPIO_InitStruct.Pin = OTG_FS_VBUS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_VBUS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : Audio_INT_Pin */
  GPIO_InitStruct.Pin = Audio_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(Audio_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ARDUINO_D7_Pin ARDUINO_D8_Pin LCD_DISP_Pin */
  GPIO_InitStruct.Pin = ARDUINO_D7_Pin | ARDUINO_D8_Pin | LCD_DISP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

  /*Configure GPIO pin : uSD_Detect_Pin */
  GPIO_InitStruct.Pin = uSD_Detect_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(uSD_Detect_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_BL_CTRL_Pin */
  GPIO_InitStruct.Pin = LCD_BL_CTRL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_BL_CTRL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_OverCurrent_Pin */
  GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : TP3_Pin NC2_Pin */
  GPIO_InitStruct.Pin = TP3_Pin | NC2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pin : DCMI_PWR_EN_Pin */
  GPIO_InitStruct.Pin = DCMI_PWR_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DCMI_PWR_EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_INT_Pin */
  GPIO_InitStruct.Pin = LCD_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(LCD_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ULPI_NXT_Pin */
  GPIO_InitStruct.Pin = ULPI_NXT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(ULPI_NXT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ARDUINO_D4_Pin ARDUINO_D2_Pin EXT_RST_Pin */
  GPIO_InitStruct.Pin = ARDUINO_D4_Pin | ARDUINO_D2_Pin | EXT_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : ULPI_STP_Pin ULPI_DIR_Pin */
  GPIO_InitStruct.Pin = ULPI_STP_Pin | ULPI_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : RMII_RXER_Pin */
  GPIO_InitStruct.Pin = RMII_RXER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(RMII_RXER_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ULPI_CLK_Pin ULPI_D0_Pin */
  GPIO_InitStruct.Pin = ULPI_CLK_Pin | ULPI_D0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  // B1 Mavi buton (PI11) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ekran geÃƒÆ’Ã‚Â§iÃƒâ€¦Ã…Â¸i
  GPIO_InitStruct.Pin = GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// ================================================================
// SPLASH SCREEN
// ================================================================
void Show_Splash_Screen(void) {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x045D4B), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "KOU RACING");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

  lv_obj_t *sub = lv_label_create(scr);
  lv_label_set_text(sub, "TELEMETRY SYSTEM");
  lv_obj_set_style_text_color(sub, lv_color_hex(0x00d2ff), 0);
  lv_obj_align(sub, LV_ALIGN_CENTER, 0, 25);

  for (int i = 0; i < 30; i++) {
    lv_timer_handler();
    osDelay(100);
  }
  lv_obj_del(title);
  lv_obj_del(sub);
}

// ================================================================
// SCREEN 1 - ANA DASHBOARD
// ================================================================
void Create_KOURacing_Dashboard(void) {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  scr1_cont = lv_obj_create(scr);
  lv_obj_remove_style_all(scr1_cont);
  lv_obj_set_size(scr1_cont, 480, 272);
  lv_obj_set_pos(scr1_cont, 0, 0);

  // ========== RPM LED'ler (ust satir) ==========
  {
    int led_w = 28, led_h = 18, led_gap = 6;
    int total_w = RPM_LED_COUNT * led_w + (RPM_LED_COUNT - 1) * led_gap;
    int sx = (480 - total_w) / 2;
    for (int i = 0; i < RPM_LED_COUNT; i++) {
      rpm_leds[i] = lv_obj_create(scr1_cont);
      lv_obj_remove_style_all(rpm_leds[i]);
      lv_obj_set_size(rpm_leds[i], led_w, led_h);
      lv_obj_set_style_radius(rpm_leds[i], led_h / 2, 0);
      lv_color_t c;
      if (i < 4)      c = lv_color_hex(0x00FF88);
      else if (i < 8) c = lv_color_hex(0xFFD700);
      else            c = lv_color_hex(0xE94560);
      lv_obj_set_style_bg_color(rpm_leds[i], c, 0);
      lv_obj_set_style_bg_opa(rpm_leds[i], LV_OPA_COVER, 0);
      lv_obj_set_pos(rpm_leds[i], sx + i * (led_w + led_gap), 3);
      lv_obj_add_flag(rpm_leds[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  // ========== Bilgi satiri: REG | RPM | TCS ==========
  regen_label = lv_label_create(scr1_cont);
  lv_label_set_text(regen_label, "REG %0");
  lv_obj_set_style_text_color(regen_label, lv_color_hex(0x00FF88), 0);
  lv_obj_set_style_text_font(regen_label, &lv_font_montserrat_28, 0);
  lv_obj_set_pos(regen_label, 15, 26);

  rpm_label = lv_label_create(scr1_cont);
  lv_label_set_text(rpm_label, "0");
  lv_obj_set_style_text_color(rpm_label, lv_color_hex(0xe94560), 0);
  lv_obj_set_style_text_font(rpm_label, &lv_font_montserrat_28, 0);
  lv_obj_align(rpm_label, LV_ALIGN_TOP_MID, 0, 26);

  tcs_label = lv_label_create(scr1_cont);
  lv_label_set_text(tcs_label, "TCS %0");
  lv_obj_set_style_text_color(tcs_label, lv_color_hex(0xe94560), 0);
  lv_obj_set_style_text_font(tcs_label, &lv_font_montserrat_28, 0);
  lv_obj_set_pos(tcs_label, 339, 26);

  // ========== Sol kolon: Sicakliklar ==========
  bat_temp_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(bat_temp_lbl, "BAT  0\xc2\xb0" "C");
  lv_obj_set_style_text_color(bat_temp_lbl, lv_color_hex(0xffd700), 0);
  lv_obj_set_style_text_font(bat_temp_lbl, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(bat_temp_lbl, 5, 84);

  inv_temp_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(inv_temp_lbl, "INV  0\xc2\xb0" "C");
  lv_obj_set_style_text_color(inv_temp_lbl, lv_color_hex(0xffd700), 0);
  lv_obj_set_style_text_font(inv_temp_lbl, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(inv_temp_lbl, 5, 110);

  mtr_temp_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(mtr_temp_lbl, "MTR  0\xc2\xb0" "C");
  lv_obj_set_style_text_color(mtr_temp_lbl, lv_color_hex(0xff6b35), 0);
  lv_obj_set_style_text_font(mtr_temp_lbl, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(mtr_temp_lbl, 5, 136);

  oil_temp_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(oil_temp_lbl, "OIL  0\xc2\xb0" "C");
  lv_obj_set_style_text_color(oil_temp_lbl, lv_color_hex(0xff6b35), 0);
  lv_obj_set_style_text_font(oil_temp_lbl, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(oil_temp_lbl, 5, 162);

  wtr_temp_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(wtr_temp_lbl, "WTR  0\xc2\xb0" "C");
  lv_obj_set_style_text_color(wtr_temp_lbl, lv_color_hex(0x00d2ff), 0);
  lv_obj_set_style_text_font(wtr_temp_lbl, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(wtr_temp_lbl, 5, 188);

  // ========== Sol lastik sicakligi ==========
  tire_left_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(tire_left_lbl, "0\n\xc2\xb0" "C");
  lv_obj_set_style_text_color(tire_left_lbl, lv_color_white(), 0);
  lv_obj_set_style_text_align(tire_left_lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(tire_left_lbl, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(tire_left_lbl, 168, 60);

  // ========== Fren bari (kirmizi) ==========
  brake_bar = lv_bar_create(scr1_cont);
  lv_obj_set_size(brake_bar, 18, 120);
  lv_obj_set_pos(brake_bar, 140, 100);
  lv_bar_set_range(brake_bar, 0, 100);
  lv_obj_set_style_bg_color(brake_bar, lv_color_hex(0x333355), LV_PART_MAIN);
  lv_obj_set_style_bg_color(brake_bar, lv_color_hex(0xff0000), LV_PART_INDICATOR);

  brake_pct_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(brake_pct_lbl, "0");
  lv_obj_set_style_text_color(brake_pct_lbl, lv_color_hex(0xff0000), 0);
  lv_obj_set_style_text_font(brake_pct_lbl, &lv_font_montserrat_16, 0);
  lv_obj_set_pos(brake_pct_lbl, 129, 220);

  // ========== Hiz (merkez, buyuk font) ==========
  speed_label = lv_label_create(scr1_cont);
  lv_label_set_text(speed_label, "0");
  lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(speed_label, lv_color_white(), 0);
  lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, -50);

  lv_obj_t *kmh = lv_label_create(scr1_cont);
  lv_label_set_text(kmh, "KM/H");
  lv_obj_set_style_text_color(kmh, lv_color_hex(0x888888), 0);
  lv_obj_set_style_text_font(kmh, &lv_font_montserrat_20, 0);
  lv_obj_align(kmh, LV_ALIGN_CENTER, 0, -15);

  // Lap timer
  laptime_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(laptime_lbl, "00.00.000");
  lv_obj_set_style_text_color(laptime_lbl, lv_color_white(), 0);
  lv_obj_set_style_text_font(laptime_lbl, &lv_font_montserrat_28, 0);
  lv_obj_align(laptime_lbl, LV_ALIGN_CENTER, 0, 10);

  // BAT %
  bat_pct_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(bat_pct_lbl, "BAT %0");
  lv_obj_set_style_text_color(bat_pct_lbl, lv_color_hex(0xffd700), 0);
  lv_obj_set_style_text_font(bat_pct_lbl, &lv_font_montserrat_28, 0);
  lv_obj_align(bat_pct_lbl, LV_ALIGN_CENTER, 0, 40);

  // Power (W)
  power_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(power_lbl, "0W");
  lv_obj_set_style_text_color(power_lbl, lv_color_white(), 0);
  lv_obj_set_style_text_font(power_lbl, &lv_font_montserrat_28, 0);
  lv_obj_align(power_lbl, LV_ALIGN_CENTER, 0, 70);

  // ========== Sag lastik sicakligi ==========
  tire_right_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(tire_right_lbl, "0\n\xc2\xb0" "C");
  lv_obj_set_style_text_color(tire_right_lbl, lv_color_white(), 0);
  lv_obj_set_style_text_align(tire_right_lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(tire_right_lbl, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(tire_right_lbl, 280, 60);

  // ========== Gaz bari (yesil) ==========
  throttle_bar = lv_bar_create(scr1_cont);
  lv_obj_set_size(throttle_bar, 18, 120);
  lv_obj_set_pos(throttle_bar, 320, 100);
  lv_bar_set_range(throttle_bar, 0, 100);
  lv_obj_set_style_bg_color(throttle_bar, lv_color_hex(0x333355), LV_PART_MAIN);
  lv_obj_set_style_bg_color(throttle_bar, lv_color_hex(0x00FF88), LV_PART_INDICATOR);

  throttle_pct_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(throttle_pct_lbl, "0");
  lv_obj_set_style_text_color(throttle_pct_lbl, lv_color_hex(0x00FF88), 0);
  lv_obj_set_style_text_font(throttle_pct_lbl, &lv_font_montserrat_16, 0);
  lv_obj_set_pos(throttle_pct_lbl, 310, 220);

  // ========== Sag kolon: MODE + FAULT ==========
  mode_title_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(mode_title_lbl, "MODE");
  lv_obj_set_style_text_color(mode_title_lbl, lv_color_hex(0x888888), 0);
  lv_obj_set_style_text_font(mode_title_lbl, &lv_font_montserrat_16, 0);
  lv_obj_set_pos(mode_title_lbl, 400, 60);

  mode_value_lbl = lv_label_create(scr1_cont);
  lv_label_set_text(mode_value_lbl, "ENDR");
  lv_obj_set_style_text_color(mode_value_lbl, lv_color_white(), 0);
  lv_obj_set_style_text_font(mode_value_lbl, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(mode_value_lbl, 395, 73);

  fault_label = lv_label_create(scr1_cont);
  lv_label_set_text(fault_label, "FAULT");
  lv_obj_set_style_text_color(fault_label, lv_color_hex(0x888888), 0);
  lv_obj_set_style_text_font(fault_label, &lv_font_montserrat_16, 0);
  lv_obj_set_pos(fault_label, 400, 96);

  // ========== Alt satir: Mor TC LED'ler + READY ==========
  {
    int d = 24, gap = 4; // Boyutu 30->24, boslugu 5->4 yaptik ki ortaya yer acilsin
    // Sol 5 mor LED
    for (int i = 0; i < 5; i++) {
      tc_leds[i] = lv_obj_create(scr1_cont);
      lv_obj_remove_style_all(tc_leds[i]);
      lv_obj_set_size(tc_leds[i], d, d);
      lv_obj_set_style_radius(tc_leds[i], d / 2, 0);
      lv_obj_set_style_bg_color(tc_leds[i], lv_color_hex(0x9B30FF), 0);
      lv_obj_set_style_bg_opa(tc_leds[i], LV_OPA_COVER, 0);
      lv_obj_set_pos(tc_leds[i], 15 + i * (d + gap), 244);
      lv_obj_add_flag(tc_leds[i], LV_OBJ_FLAG_HIDDEN);
    }
    // Sag 4 mor LED
    for (int i = 0; i < 4; i++) {
      tc_leds[5 + i] = lv_obj_create(scr1_cont);
      lv_obj_remove_style_all(tc_leds[5 + i]);
      lv_obj_set_size(tc_leds[5 + i], d, d);
      lv_obj_set_style_radius(tc_leds[5 + i], d / 2, 0);
      lv_obj_set_style_bg_color(tc_leds[5 + i], lv_color_hex(0x9B30FF), 0);
      lv_obj_set_style_bg_opa(tc_leds[5 + i], LV_OPA_COVER, 0);
      lv_obj_set_pos(tc_leds[5 + i], 357 + i * (d + gap), 244);
      lv_obj_add_flag(tc_leds[5 + i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  // READY / NOT READY
  ready_label = lv_label_create(scr1_cont);
  lv_label_set_text(ready_label, "NOT READY");
  lv_obj_set_style_text_color(ready_label, lv_color_hex(0xff0000), 0);
  lv_obj_set_style_text_font(ready_label, &lv_font_montserrat_28, 0);
  lv_obj_align(ready_label, LV_ALIGN_BOTTOM_MID, 0, -2);

  // ========== BEYAZ DIKDORTGEN AYIRICILAR ==========
  // Yardimci makro: beyaz ayirici dikdortgen olustur
  #define CREATE_DIVIDER(parent, x, y, w, h) \
    do { \
      lv_obj_t *div = lv_obj_create(parent); \
      lv_obj_remove_style_all(div); \
      lv_obj_set_size(div, (w), (h)); \
      lv_obj_set_pos(div, (x), (y)); \
      lv_obj_set_style_bg_color(div, lv_color_hex(DIV_COLOR), 0); \
      lv_obj_set_style_bg_opa(div, DIV_OPACITY, 0); \
      lv_obj_set_style_radius(div, 0, 0); \
      lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE); \
    } while(0)

  // Yatay cizgi 1: Ust bilgi satiri alti
  CREATE_DIVIDER(scr1_cont, DIV_H1_X, DIV_H1_Y, DIV_H1_W, DIV_THICKNESS);

  // Yatay cizgi 2: Alt TC LED satiri ustu
  CREATE_DIVIDER(scr1_cont, DIV_H2_X, DIV_H2_Y, DIV_H2_W, DIV_THICKNESS);

  // Dikey cizgi 1: Sol sicaklik kolonu sag kenari
  CREATE_DIVIDER(scr1_cont, DIV_V1_X, DIV_V1_Y, DIV_THICKNESS, DIV_V1_H);

  // Dikey cizgi 2: Fren bari | Hiz merkez
  CREATE_DIVIDER(scr1_cont, DIV_V2_X, DIV_V2_Y, DIV_THICKNESS, DIV_V2_H);

  // Dikey cizgi 3: Hiz merkez | Gaz bari
  CREATE_DIVIDER(scr1_cont, DIV_V3_X, DIV_V3_Y, DIV_THICKNESS, DIV_V3_H);

  // Dikey cizgi 4: MODE/FAULT kolonu sol kenari
  CREATE_DIVIDER(scr1_cont, DIV_V4_X, DIV_V4_Y, DIV_THICKNESS, DIV_V4_H);

  #undef CREATE_DIVIDER
}

// ================================================================
// SCREEN 2 - DETAYLI BILGI (mavi butonla gecis)
// ================================================================
void Create_Screen2(void) {
  lv_obj_t *scr = lv_screen_active();
  scr2_cont = lv_obj_create(scr);
  lv_obj_remove_style_all(scr2_cont);
  lv_obj_set_size(scr2_cont, 480, 272);
  lv_obj_set_pos(scr2_cont, 0, 0);
  lv_obj_add_flag(scr2_cont, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *t = lv_label_create(scr2_cont);
  lv_label_set_text(t, "-- DETAILED TELEMETRY --");
  lv_obj_set_style_text_color(t, lv_color_hex(0x00d2ff), 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 5);

  const char *left_labels[] = {
    "RPM:", "SPEED:", "THROTTLE:", "BRAKE:",
    "BAT TEMP:", "INV TEMP:", "MTR TEMP:",
    "OIL TEMP:", "WTR TEMP:", "MODE:"
  };
  for (int i = 0; i < 10; i++) {
    lv_obj_t *l = lv_label_create(scr2_cont);
    lv_label_set_text(l, left_labels[i]);
    lv_obj_set_style_text_color(l, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(l, 15, 25 + i * 22);
  }
  const char *right_labels[] = {
    "BATTERY V:", "BAT SOC:", "POWER:",
    "TIRE L:", "TIRE R:", "REG:",
    "TCS:", "FAULT:", "READY:", "LAP:"
  };
  for (int i = 0; i < 10; i++) {
    lv_obj_t *l = lv_label_create(scr2_cont);
    lv_label_set_text(l, right_labels[i]);
    lv_obj_set_style_text_color(l, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(l, 255, 25 + i * 22);
  }
}

// ================================================================
// REFRESH SCREEN 1
// ================================================================
void Refresh_Dashboard_UI(void) {
  // RPM LED'ler
  {
    int n = (myCar.rpm * RPM_LED_COUNT) / RPM_MAX_VALUE;
    if (n > RPM_LED_COUNT) n = RPM_LED_COUNT;
    for (int i = 0; i < RPM_LED_COUNT; i++) {
      if (i < n) lv_obj_clear_flag(rpm_leds[i], LV_OBJ_FLAG_HIDDEN);
      else       lv_obj_add_flag(rpm_leds[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  // REG / RPM / TCS
  lv_label_set_text_fmt(regen_label, "REG %%%d", myCar.regen_percent);
  lv_label_set_text_fmt(rpm_label, "%d RPM", myCar.rpm);
  lv_label_set_text_fmt(tcs_label, "TCS %%%d", myCar.tcs_percent);

  // Sicakliklar
  lv_label_set_text_fmt(bat_temp_lbl, "BAT %d\xc2\xb0" "C", myCar.bat_temp);
  lv_label_set_text_fmt(inv_temp_lbl, "INV %d\xc2\xb0" "C", myCar.inv_temp);
  lv_label_set_text_fmt(mtr_temp_lbl, "MTR %d\xc2\xb0" "C", myCar.motor_temp);
  lv_label_set_text_fmt(oil_temp_lbl, "OIL %d\xc2\xb0" "C", myCar.oil_temp);
  lv_label_set_text_fmt(wtr_temp_lbl, "WTR %d\xc2\xb0" "C", myCar.wtr_temp);

  // Lastik sicakliklari
  lv_label_set_text_fmt(tire_left_lbl, "%d\n\xc2\xb0" "C", myCar.tire_temp_left);
  lv_label_set_text_fmt(tire_right_lbl, "%d\n\xc2\xb0" "C", myCar.tire_temp_right);

  // Fren bari (kirmizi)
  lv_bar_set_value(brake_bar, (int32_t)(myCar.brake * 100), LV_ANIM_ON);
  lv_label_set_text_fmt(brake_pct_lbl, "%%%d", (int)(myCar.brake * 100));

  // Gaz bari (yesil)
  lv_bar_set_value(throttle_bar, (int32_t)(myCar.throttle * 100), LV_ANIM_ON);
  lv_label_set_text_fmt(throttle_pct_lbl, "%%%d", (int)(myCar.throttle * 100));

  // Hiz
  lv_label_set_text_fmt(speed_label, "%d", myCar.speed);

  // Lap timer (mm.ss.mmm)
  {
    int ms  = lap_time_ms % 1000;
    int sec = (lap_time_ms / 1000) % 60;
    int min = (lap_time_ms / 60000) % 10;
    lv_label_set_text_fmt(laptime_lbl, "0%d.%02d.%03d", min, sec, ms);
  }

  // Batarya % ve Guc
  lv_label_set_text_fmt(bat_pct_lbl, "BAT %%%d", myCar.bat_percent);
  lv_label_set_text_fmt(power_lbl, "%dW", myCar.power_watts);

  // Mode
  lv_label_set_text_fmt(mode_value_lbl, "%s", myCar.mode);

  // Mor TC LED'ler (alt kisim)
  for (int i = 0; i < TC_LED_COUNT; i++) {
    if (myCar.traction_control)
      lv_obj_clear_flag(tc_leds[i], LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(tc_leds[i], LV_OBJ_FLAG_HIDDEN);
  }

  // Ready durumu
  if (myCar.ready) {
    lv_label_set_text(ready_label, "READY");
    lv_obj_set_style_text_color(ready_label, lv_color_hex(0x00FF88), 0);
  } else {
    lv_label_set_text(ready_label, "NOT READY");
    lv_obj_set_style_text_color(ready_label, lv_color_hex(0xff0000), 0);
  }
}

// ================================================================
// REFRESH SCREEN 2 - placeholder
// ================================================================
void Refresh_Screen2_UI(void) {
  // Ileride dinamik degerler eklenecek
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const *argument) {
  /* init code for USB_HOST */
  MX_USB_HOST_Init();

  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN 5 */

  // --- 1. EKRAN VE ARAYUZ ---
  lv_init();
  lv_port_disp_init();
  Show_Splash_Screen();  // 3 saniye acilis ekrani
  Create_KOURacing_Dashboard();
  Create_Screen2();

  // --- 2. UDP SUNUCU BASLAT ---
  UDP_Server_Init();

  // --- 3. RASTGELE SAYI TOHUMLAMA ---
  srand(HAL_GetTick());  // Splash screen sonrasi ~3000ms tick = her resette farkli seed

  // Varsayilan degerler
  myCar.vehicle_state = "Ready";
  myCar.inverter_status = "Ready";
  myCar.battery_status = "Normal";
  myCar.mode = "ENDR";
  myCar.fault = 0;
  myCar.ready = 0;

  /* Infinite loop */
  for (;;) {
    // LVGL ekran guncelleme
    lv_timer_handler();

    // Mavi buton (PI11) ile ekran gecisi - debounce
    {
      uint8_t btn_now = HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_11);
      if (btn_now == GPIO_PIN_SET && btn_prev == GPIO_PIN_RESET) {
        // Buton basildi (rising edge)
        current_screen = !current_screen;
        if (current_screen) {
          lv_obj_add_flag(scr1_cont, LV_OBJ_FLAG_HIDDEN);
          lv_obj_clear_flag(scr2_cont, LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_obj_clear_flag(scr1_cont, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(scr2_cont, LV_OBJ_FLAG_HIDDEN);
        }
      }
      btn_prev = btn_now;
    }

    // Telemetri verisi uret
    Update_Telemetry();

    // Aktif ekrani guncelle
    if (current_screen == 0) {
      Refresh_Dashboard_UI();
    } else {
      Refresh_Screen2_UI();
    }

    // JSON olustur ve UDP ile gonder (Raspberry Pi'ye)
    sprintf(json_buffer,
            "{\"rpm\":%d,\"speed\":%d,\"motor_temp\":%d,"
            "\"battery_voltage\":%.1f,\"throttle\":%.2f,\"brake\":%.2f,"
            "\"bat_temp\":%d,\"inv_temp\":%d,\"oil_temp\":%d,\"wtr_temp\":%d,"
            "\"tire_left\":%d,\"tire_right\":%d,"
            "\"power_watts\":%d,\"bat_percent\":%d,"
            "\"regen_percent\":%d,\"tcs_percent\":%d,"
            "\"mode\":\"%s\",\"ready\":%s,"
            "\"fault\":%s,\"traction_control\":%s}",
            myCar.rpm, myCar.speed, myCar.motor_temp,
            myCar.battery_voltage, myCar.throttle, myCar.brake,
            myCar.bat_temp, myCar.inv_temp, myCar.oil_temp, myCar.wtr_temp,
            myCar.tire_temp_left, myCar.tire_temp_right,
            myCar.power_watts, myCar.bat_percent,
            myCar.regen_percent, myCar.tcs_percent,
            myCar.mode, myCar.ready ? "true" : "false",
            myCar.fault ? "true" : "false",
            myCar.traction_control ? "true" : "false");

    Send_Telemetry_UDP(json_buffer);

    // RTOS uyumlu bekleme (100ms = 10 Hz)
    osDelay(100);
  }
  /* USER CODE END 5 */
}

/* MPU Configuration */
void MPU_Config(void) {
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
   */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM6 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */
  lv_tick_inc(1); // LVGL tick her 1ms'de bir (TIM6 = 1kHz)
  /* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  // LCD Backlight'Ãƒâ€Ã‚Â± yakÃƒâ€Ã‚Â±p sÃƒÆ’Ã‚Â¶ndÃƒÆ’Ã‚Â¼rerek hata olduÃƒâ€Ã…Â¸unu gÃƒÆ’Ã‚Â¶ster
  while (1) {
    HAL_GPIO_TogglePin(LCD_BL_CTRL_GPIO_Port, LCD_BL_CTRL_Pin);
    for (volatile uint32_t i = 0; i < 2000000; i++)
      ; // Basit gecikme
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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
