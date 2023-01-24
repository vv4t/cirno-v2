#include "lib.h"

#include "data.h"
#include "int_main.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>

int getch()
{
  struct termios oldt, newt;
  int ch;
  
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  ch = getchar();
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  
  return ch;
}

bool clear_f(expr_t *ret_value, scope_t *scope_args)
{
  system("clear");
}

bool getch_f(expr_t *ret_value, scope_t *scope_args)
{
  expr_i32(ret_value, getch());
}

bool cos_f(expr_t *ret_value, scope_t *scope_args)
{
  expr_t theta;
  int_arg_load(scope_args, &theta, "theta");
  
  expr_f32(ret_value, cos(theta.f32));
}

bool sin_f(expr_t *ret_value, scope_t *scope_args)
{
  expr_t theta;
  int_arg_load(scope_args, &theta, "theta");
  
  expr_f32(ret_value, sin(theta.f32));
}

void lib_load_stdlib()
{
  int_bind("getch", getch_f);
  int_bind("clear", clear_f);
}

void lib_load_math()
{
  int_bind("cos", cos_f);
  int_bind("sin", sin_f);
}
