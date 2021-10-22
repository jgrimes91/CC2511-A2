/**************************************************************
 * main.c
 * rev 1.0 06-Oct-2021 jessg
 * code
 * ***********************************************************/

#include "pico/stdlib.h"
#include <stdbool.h>
#include <stdio.h>
#include "hardware/pwm.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "terminal.h"

#define UART_ID uart0
#define UART_RX 0
#define UART_TX 1
#define X_DIR 3
#define X_STEP 4
#define Y_DIR 9
#define Y_STEP 10
#define Z_DIR 10
#define Z_STEP 11
#define SPINDLE 15
#define MODE0 20
#define MODE1 19
#define MODE2 18

uint8_t xdir, ydir, zdir, spindle = 0;
uint8_t spindle_up, spindle_down;
uint8_t x_length, y_length, z_length;
uint8_t spindle_speed;

// Allocating to buffer
volatile char buffer[100];
volatile unsigned int idx = 0;

// RX interupt handler
volatile bool input_ready = false;

void set_x_pins(bool step, bool dir)
{
  // Should help control the motors in x direction
  gpio_put(X_STEP, step);
  gpio_put(X_DIR, dir);
}

void set_y_pins(bool step, bool dir)
{
  // Should help control the motors in y direction
  gpio_put(Y_STEP, step);
  gpio_put(Y_DIR, dir);
}

void set_z_pins(bool step, bool dir)
{
  // Should help control the motors in z direction
  gpio_put(Z_STEP, step);
  gpio_put(Z_DIR, dir);
}

void on_uart_rx()
{
  // UART interrupt
  while (uart_is_readable(UART_ID))
  {
    uint8_t ch = uart_getc(UART_ID);
    if (ch != 255)
    {
      switch (ch)
      {
      case '\r':
      case '\n':
        buffer[idx] = 0;
        input_ready = true;
        break;
      case 0x7f:
        // Backspace
        buffer[idx] = '\0';
        idx--;
        send_char(0x7f);
        break;
      default:
        buffer[idx] = ch;
        idx++;
        send_char(ch);
        break;
      }
    }
  }
}

void send_char(char ch)
{
  // Checking if character is writeable
  if (uart_is_writable(UART_ID))
  {
    uart_putc(UART_ID, ch);
  }
}

void calibration(){
  // Calibrate to find/determine the boundaries of the working space - calculate from the corners
  // First calibrate in the x direction using a&d
  // Second calibrate in the y direction using w&s
  // Third z direction for the spindle

}

int main(void)
{
  // Initialise components and variables
  stdio_init_all();

  // Initialise directions
  gpio_init(X_DIR);
  gpio_init(X_STEP);
  gpio_init(Y_DIR);
  gpio_init(Y_STEP);
  gpio_init(Z_DIR);
  gpio_init(Z_STEP);
  gpio_set_dir(X_DIR, true);
  gpio_set_dir(X_STEP, true);
  gpio_set_dir(Y_DIR, true);
  gpio_set_dir(Y_STEP, true);
  gpio_set_dir(Z_DIR, true);
  gpio_set_dir(Z_STEP, true);

  // Initialise spindle
  gpio_init(SPINDLE);
  gpio_set_dir(SPINDLE, true);

  // Set up spindle PWM
  gpio_set_function(SPINDLE, GPIO_FUNC_UART);
  uint spindle_slice = pwm_gpio_to_slice_num(SPINDLE);
  pwm_set_enabled(spindle_slice, true);

  // Booleans to control directions for step motors
  bool step, dir = false;

  // Set up RX/TX pins
  gpio_set_function(UART_RX, GPIO_FUNC_UART);
  gpio_set_function(UART_TX, GPIO_FUNC_UART);

  // Turning off FIFO's - we want to read character by character
  uart_set_fifo_enabled(UART_ID, false);

  // Set up a RX interrupt
  // Setting up handler first for the correct interrupt for the UART we are using
  int UART_IQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

  // Settng up and enabling the interrupt handlers
  irq_set_exclusive_handler(UART0_IRQ, on_uart_rx);
  irq_set_enabled(UART_IQ, true);

  // Enabling the URAt to send interrupts - RX only
  uart_set_irq_enables(UART_ID, true, false);

  while (true)
  {
    set_x_pins(step, dir);
    set_y_pins(step, dir);
    set_z_pins(step, dir);

    while (!input_ready)
    {
      __asm("wfi"); // Wait in low power mode
    }

    if (input_ready == true)
    {
      // Manual mode -- TODO maybe serperate 
      // X direction
      if (sscanf(buffer, "a %hhu", &xdir) == 1)
      {
        step = ! step;
        idx = 0;
      }
      else if (sscanf(buffer, "d %hhu", &xdir) == 1)
      {
        dir = ! dir;
        idx = 0;
      }
      // Y direction
      else if (sscanf(buffer, "w %hhu", &ydir) == 1)
      {
        step = ! step;
        idx = 0;
      }
      else if (sscanf(buffer, "s %hhu", &ydir) == 1)
      {
        dir = ! dir;
        idx = 0;
      }
      // Z direction
      else if (sscanf(buffer, "z %hhu", &zdir) == 1)
      {
        step = ! step;
        idx = 0;
      }
      else if (sscanf(buffer, "x %hhu", &zdir) == 1)
      { 
        dir = ! dir;
        idx = 0;
      }
      // Spindle
      else if (sscanf(buffer, "on %hhu", &spindle_speed) == 1){
        pwm_set_gpio_level(SPINDLE, &spindle_speed);
        idx = 0;
      }
      else if (0 == strcmp(buffer, "off ")){
         pwm_set_gpio_level(SPINDLE, 0);
      }
    }
    input_ready = false;
  }
}
