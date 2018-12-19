#include "mode.h"

#include "stm32f4xx_hal.h"
#include "spi.h"
#include "tim.h"

#include <stdio.h>

#include "run.h"
#include "motion.h"
#include "targetGenerator.h"

#include "timer.h"

#include "maze.h"
#include "agent.h"

#include "buzzer.h"
#include "led.h"
#include "flash.h"
#include "function.h"
#include "logger.h"

#include "mazeRun.h"

// ゴール座標の設定
static uint8_t goal_x = 1;
static uint8_t goal_y = 0;
static uint8_t maze_goal_size = 1;

void modeSelect( int8_t mode )
{

  mode_init();
  log_init();

  switch( mode ){
    case 0:
      mode0();
      break;

    case 1:
      mode1();
      break;

    case 2:
      mode2();
      break;

    case 3:
      mode3();
      break;
    
    case 4:
      mode4();
      break;

    case 5:
      mode5();
      break;
    
    case 6:
      mode6();
      break;

    case 7:
      mode7();
      break;

    case 8:
      mode8();
      break;

    default:
      break;
  }
  adcEnd();
  HAL_Delay( 300 );
  translation_ideal.accel = 0.0;
	translation_ideal.velocity = 0.0;
  setControlFlag( 0 );
  mode_counter = 0;
}

void mode_init( void )
{
  printf("\r\n");
  failSafe_flag = 0;
  mode_counter |= 0x80;
	translation_ideal.accel = 0.0;
	translation_ideal.velocity = 0.0;
	translation_ideal.distance = 0.0;
	rotation_ideal.accel = 0.0;
	rotation_ideal.distance = 0.0;
	rotation_ideal.velocity = 0.0;
	rotation_trape_param.back_rightturn_flag = 0;
	rotation_deviation.cumulative = 0.0;
  // to do search param と fast paramで分けれるようにする
  setSlaromOffset( &slarom500, 21.0f, 20.0f, 21.0f, 21.5f, 7200.0f, 630.0f );

  setPIDGain( &translation_gain, 1.5f, 30.0f, 0.0f );  
  setPIDGain( &rotation_gain, 0.45f, 50.0f, 0.0f ); 

  // sensor 値設定
  setSensorConstant( &sen_front, 730, 380 );
  setSensorConstant( &sen_l, 540, 400 );
  setSensorConstant( &sen_r, 630, 510 );

  certainLedOut( LED_FRONT );
  waitMotion( 100 );
  certainLedOut( LED_OFF );
  waitMotion( 100 );
  certainLedOut( LED_FRONT );
  waitMotion( 100 );
  certainLedOut( LED_OFF );
  fullColorLedOut( LED_OFF );
  waitMotion( 100 );
}

void startAction( void )
{
  while( getPushsw() == 0 );
  buzzerSetMonophonic( A_SCALE, 200 );
  HAL_Delay( 300 );
  fullColorLedOut( LED_MAGENTA );
  adcStart();
  while( sen_front.now < 600 );
  fullColorLedOut( LED_YELLOW );
  buzzerSetMonophonic( C_H_SCALE, 300 );
  adcEnd();
  waitMotion( 500 );
  MPU6500_z_axis_offset_calc_start();
  while( MPU6500_calc_check() ==  0 );
  fullColorLedOut( LED_OFF );
  adcStart();
  waitMotion( 100 );
  setLogFlag( 1 );
  setControlFlag( 1 );
}

void writeFlashData( t_walldata *wall )
{
  wall->save = 1;

  for ( int i = 0; i <= MAZE_HALF_MAX_SIZE; i++ ){
    wall->column_known[i] = 0xffffffff;
    wall->row_known[i] = 0xffffffff;
  }

  writeFlash( start_address, (uint8_t*)wall, sizeof( t_walldata ) );
}

void loadWallData( t_walldata *wall )
{
  loadFlash( start_address, (uint8_t *)wall, sizeof( t_walldata ) );
}

// 足立法探索
void mode0( void )
{
  setNormalRunParam( &run_param, 4000.0f, 500.0f );       // 加速度、探索速度指定
  setNormalRunParam( &rotation_param, 5400.0f, 450.0f );  // 角加速度、角速度指定
  wall_Init( &wall_data, MAZE_CLASSIC_SIZE );
  wallBIt_Init( &wall_bit, MAZE_CLASSIC_SIZE );
  setMazeGoalSize( maze_goal_size );
  positionReset( &mypos );

  startAction();

  adachiSearchRun( goal_x, goal_y, &run_param, &rotation_param, &wall_data, &wall_bit, &mypos, MAZE_CLASSIC_SIZE );
  adcEnd();
  setControlFlag( 0 );
  writeFlashData( &wall_bit );
  setVirtualGoal( MAZE_CLASSIC_SIZE, &wall_data );
  adcStart();
  setControlFlag( 1 );
  adachiSearchRun( 0, 0, &run_param, &rotation_param, &wall_data, &wall_bit, &mypos, MAZE_CLASSIC_SIZE );
  adcEnd();
  setControlFlag( 0 );
  writeFlashData( &wall_bit );

}

// 足立法最短(斜めなし)
void mode1( void )
{
  setNormalRunParam( &run_param, 8000.0f, 500.0f );       // 加速度、速度指定
  setNormalRunParam( &rotation_param, 5400.0f, 450.0f );  // 角加速度、角速度指定

  loadWallData( &wall_data );
  agentSetShortRoute( goal_x, goal_y, &wall_data, MAZE_CLASSIC_SIZE, 0, 0 );
  
  positionReset( &mypos );
  startAction();

  adachiFastRun( &run_param, &rotation_param );

  setControlFlag( 0 );
}

// 足立法最短( 斜めあり )
void mode2( void )
{
  setNormalRunParam( &run_param, 8000.0f, 700.0f );       // 加速度、速度指定
  setNormalRunParam( &rotation_param, 6300.0f, 450.0f );  // 角加速度、角速度指定

  loadWallData( &wall_data );
  
  if ( agentDijkstraRoute( goal_x, goal_y, &wall_data, MAZE_CLASSIC_SIZE, 0, 0 ) == 0 ){
    return;
  }

  positionReset( &mypos );
  startAction();

  adachiFastRunDiagonal( &run_param, &rotation_param );

}


// 各種センサー値確認,最短パスチェック
void mode3( void )
{
  loadWallData( &wall_data );
  HAL_Delay( 300 );
  adcStart();
  cnt_motion = 0;
  while( 1 ){
    printf( "fl : %4d, l: %4d, r: %4d, fr: %4d, front : %4d\r",sen_fl.now, sen_l.now, sen_r.now, sen_fr.now, sen_front.now );
    if ( getPushsw() == 1 && cnt_motion >= 2000 ) break;
  }
  adcEnd();
  if ( wall_data.save == 1 ){
    agentSetShortRoute( goal_x, goal_y, &wall_data, MAZE_CLASSIC_SIZE, 1, 0 );
    agentDijkstraRoute( goal_x, goal_y, &wall_data, MAZE_CLASSIC_SIZE, 0, 1 );
    agentDijkstraRoute( goal_x, goal_y, &wall_data, MAZE_CLASSIC_SIZE, 1, 1 );
  }
  
}

// fun check
void mode4( void )
{
  funControl( FUN_ON );
  waitMotion( 5000 );
  funControl( FUN_OFF );
}

// 直進、超進地チェック
void mode5( void )
{
  startAction();
  adjFront( 4000, 500.0f );
  straightOneBlock( 500.0f );
  straightOneBlock( 500.0f );
  straightOneBlock( 500.0f );
  straightOneBlock( 500.0f );
  straightHalfBlockStop( 4000.0f, 500.0f );
  setLogFlag( 0 );
  setControlFlag( 0 );
  while( getPushsw() == 0 );
  showLog();
}

// 回転方向のチェック
void mode6( void )
{
  startAction();
  setRotation( 180.0f, 3600.0f, 360.0f, 0.0f );
  waitRotation();
  waitMotion( 300 );
  setRotation( 180.0f, 3600.0f, 360.0f, 0.0f );
  waitRotation();
  setLogFlag( 0 );
  setControlFlag( 0 );
  while( getPushsw() == 0 );
  showLog();
}

// スラロームチェック
void mode7( void )
{

  buzzerSetMonophonic( NORMAL, 200 );
  HAL_Delay(300); 
  startAction();
  adjFront( 4000.0, 500.0f );
  slaromLeft( 500.0f );
  straightHalfBlockStop( 4000.0f, 500.0f );
}

// 直進、回転組み合わせチェック 超進地旋回
void mode8( void )
{
  buzzerSetMonophonic( NORMAL, 200 );
  HAL_Delay(300); 
  startAction();
  translation_ideal.velocity = 0.0f;
  rotation_ideal.velocity = 0.0f;
  while( 1 );
}
