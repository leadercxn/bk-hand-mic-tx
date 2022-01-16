#include "board_config.h"

#include "bk953x_handler.h"
#include "lcd_display_handler.h"
#include "button_handler.h"
#include "battery_handler.h"

#define SCHED_MAX_EVENT_DATA_SIZE   8
#define SCHED_QUEUE_SIZE            20

static app_scheduler_t  m_app_scheduler;

static gpio_object_t   m_power_on_gpio = 
{
    .gpio_port_periph_clk = POWER_ON_PORT_PERIPH_CLK,
    .p_gpio_port = POWER_ON_PORT,
    .gpio_dir = GPIO_DIR_OUTPUR,
    .gpio_pin = POWER_ON_PIN,
};

/**
 * 系统开机flag
 */
static bool m_sys_power_on = false;
static uint64_t m_per_battery_sample_ticks = 0;

static char * mp_button[BUTTON_EVENT_MAX] = {
    "BUTTON_EVENT_PUSH",
    "BUTTON_EVENT_LONG_PUSH",
    "BUTTON_EVENT_RELEASE",
};


/**
 * @brief 按键回调
 */
static void button_handler(button_event_e event)
{
    static uint64_t pre_ticks = 0;
    uint64_t now_ticks = mid_timer_ticks_get();

    static button_event_e old_event = BUTTON_EVENT_MAX;

    /**
     * 两个时间得间隔200ms
     */
    if(old_event != event)
    {
        old_event = event;
    }
    else if(event == BUTTON_EVENT_LONG_PUSH)
    {
        if(now_ticks - pre_ticks > 2000)
        {
            pre_ticks = now_ticks;
        }
        else
        {
          return;
        }
    }
    else if(now_ticks - pre_ticks > 200)
    {
        pre_ticks = now_ticks;
    }
    else
    {
      return;
    }
  
    trace_debug("button_handler %s\n\r",mp_button[event]);
    switch(event)
    {
        case BUTTON_EVENT_PUSH:
          if(m_sys_power_on)
          {
            lcd_black_light_enable(true);
            trace_debug("turn on lcd led\n\r");
          }

          break;

        case BUTTON_EVENT_LONG_PUSH:
          if(m_sys_power_on)
          {
              m_sys_power_on = false;
              trace_debug("system power off\n\r");

              bk953x_task_stage_set(BK_STATE_IDLE);
          }
          else
          {
              m_sys_power_on = true;
              trace_debug("system power on\n\r");

              bk953x_task_stage_set(BK_STAGE_INIT);
          }

          gpio_output_set(&m_power_on_gpio, m_sys_power_on);
          lcd_black_light_enable(m_sys_power_on);
          lcd_off_status_set(m_sys_power_on);
          break;

        default:
          break;
    }
}


static void app_evt_schedule(void * p_event_data)
{
    trace_debug("app_evt_schedule\n\r");
}

/**
 * @brief host-task 主任务，负责各个task之间的交互
 */
static void host_loop_task(void)
{
    uint8_t ir_rx_data_len = 0;
    uint8_t ir_rx_data[16] = {0};

    /**
     * 关机状态下不执行
     */
    if(!m_sys_power_on)
    {
        return;
    }

    /**
     * 一分钟采样一次电池电压并更新LCD
     */
    if(((mid_timer_ticks_get() - m_per_battery_sample_ticks) > 60000) || (m_per_battery_sample_ticks == 0))
    {
        m_per_battery_sample_ticks = mid_timer_ticks_get();

        lcd_battery_level_set( battery_mv_level_get() );
    }

    /**
     * 红外解码
     */
    ir_rx_data_len = ir_rx_decode_result_get(ir_rx_data);
    if(ir_rx_data_len)
    {
        trace_debug("host_loop_task ir rx data:");
        trace_dump_d(ir_rx_data, ir_rx_data_len);
    }
}

int main(void)
{
  trace_init();

  /*greeting*/
  trace_info("\n\r\n\r\n\r\n\r");
  trace_info("       *** Welcome to the Hand-Mic Project ***\n\r");
  trace_info("\n\r");


  TIMER_INIT();

  /**
   * delay 函数的初始化
   */
  mid_system_tick_init();

  APP_SCHED_INIT(&m_app_scheduler, SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
//  app_sched_event_put(&m_app_scheduler, NULL, 0, app_evt_schedule);

  lcd_display_init();

  gpio_config(&m_power_on_gpio);
  button_hw_init(button_handler);

  /**
   * 资源申请
   */
  bk9531_init();

  ir_rx_init();

  /* adc 相关 */
  adc_init();

  trace_info("Start loop\n\r");
  while(1)
  {
    app_sched_execute(&m_app_scheduler);

    bk953x_loop_task();
    button_loop_task();
    lcd_display_loop_task();
    host_loop_task();
  }
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif


