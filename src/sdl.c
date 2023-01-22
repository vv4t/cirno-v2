#include <stdio.h>

#include "interpret.h"
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <math.h>

static SDL_Window   *sdl_window = NULL;
static SDL_Renderer *sdl_renderer = NULL;
static bool         sdl_is_quit = false;

static int  prev_time = 0;
static int  lag_time = 0;

static void sdl_poll();

static bool draw_line_f(expr_t *ret_value, scope_t *scope_args);
static bool cos_f(expr_t *ret_value, scope_t *scope_args);
static bool sin_f(expr_t *ret_value, scope_t *scope_args);

static expr_t arg_list[] = { };
static int    num_arg = sizeof(arg_list) / sizeof(expr_t);

bool sdl_init()
{
  if (!SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return false;
  }
  
  sdl_window = SDL_CreateWindow(
    "cirno",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    640,
    480,
    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  
  sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
  prev_time = SDL_GetTicks();
  
  int_bind("draw_line", draw_line_f);
  int_bind("cos", cos_f);
  int_bind("sin", sin_f);
  
  return true;
}

bool sdl_frame()
{
  sdl_poll();
  
  int now_time = SDL_GetTicks();
  int delta_time = now_time - prev_time;
  prev_time = now_time;
  lag_time += delta_time;
  
  while (lag_time > 0) {
    lag_time -= 15;
    
    SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, 255);
    SDL_RenderClear(sdl_renderer);
    
    SDL_SetRenderDrawColor(sdl_renderer, 255, 255, 255, 255);
    int_call("update", arg_list, num_arg);
    
    SDL_RenderPresent(sdl_renderer);
  }
  
  return !sdl_is_quit;
}

void sdl_poll()
{
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      sdl_is_quit = true;
      break;
    }
  }
}

void sdl_quit()
{
  SDL_DestroyRenderer(sdl_renderer);
  SDL_DestroyWindow(sdl_window);
  SDL_Quit();
}

bool draw_line_f(expr_t *ret_value, scope_t *scope_args)
{
  expr_t x0;
  expr_t y0;
  expr_t x1;
  expr_t y1;
  
  int_arg_load(scope_args, &x0, "x0");
  int_arg_load(scope_args, &y0, "y0");
  int_arg_load(scope_args, &x1, "x1");
  int_arg_load(scope_args, &y1, "y1");
  
  SDL_RenderDrawLine(sdl_renderer, x0.i32, y0.i32, x1.i32, y1.i32);
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
