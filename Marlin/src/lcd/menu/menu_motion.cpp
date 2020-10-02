/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

//
// Motion Menu
//

#include "../../inc/MarlinConfigPre.h"

#if HAS_LCD_MENU

#include "menu.h"
#include "menu_addon.h"

#include "../../module/motion.h"

#include "../../gcode/queue.h"
GCodeQueue queue_Kalle;

#if ENABLED(DELTA)
  #include "../../module/delta.h"
#endif

#if ENABLED(PREVENT_COLD_EXTRUSION)
  #include "../../module/temperature.h"
#endif

#if HAS_LEVELING
  #include "../../module/planner.h"
  #include "../../feature/bedlevel/bedlevel.h"
#endif

extern millis_t manual_move_start_time;
extern int8_t manual_move_axis;
#if ENABLED(MANUAL_E_MOVES_RELATIVE)
  float manual_move_e_origin = 0;
#endif
#if IS_KINEMATIC
  extern float manual_move_offset;
#endif

//
// Tell ui.update() to start a move to current_position" after a short delay.
//
inline void manual_move_to_current(AxisEnum axis
  #if E_MANUAL > 1
    , const int8_t eindex=-1
  #endif
) {
  #if E_MANUAL > 1
    if (axis == E_AXIS) ui.manual_move_e_index = eindex >= 0 ? eindex : active_extruder;
  #endif
  manual_move_start_time = millis() + (move_menu_scale < 0.99f ? 0UL : 250UL); // delay for bigger moves
  manual_move_axis = (int8_t)axis;
}

//Kalle below are a couple of functions and menus to create the custom "Set Coordinates"-menu function that lets you set the coordinates befor executing the move!
  
  float X_Coordinate_Move=0;
  float Y_Coordinate_Move=0;
  float Z_Coordinate_Move=0;
 // Function to update the three variables that keeps track of your chosen coordinates (before you execute the actual move) increment/decrement 0.1, 1 or 10 at a time
  static void SetCoordinateWithMultiplier(const char* namn, AxisEnum axis) {
    if (ui.use_click()) return ui.goto_previous_screen_no_defer();
    //ENCODER_DIRECTION_NORMAL();
    
    if (ui.encoderPosition) {
      //refresh_cmd_timeout();
      if(ui.encoderPosition%ENCODER_STEPS_PER_MENU_ITEM==0){ 
        float Addition = float((int32_t)ui.encoderPosition)/ENCODER_STEPS_PER_MENU_ITEM;
        if(axis == X_AXIS){
          X_Coordinate_Move += Addition; 
          //CurrentPos=X_Coordinate_Move;
        }else if(axis == Y_AXIS){
          Y_Coordinate_Move += Addition;
          //CurrentPos=Y_Coordinate_Move;
        }else{
          Z_Coordinate_Move += Addition;
          //CurrentPos=Z_Coordinate_Move;
        }
        
        ui.encoderPosition = 0;
      }
      ui.refresh(LCDVIEW_REDRAW_NOW);
    }
    //if (lcdDrawUpdate){
     if(axis == X_AXIS){
        //X_Coordinate_Move = 23.22;
        MenuEditItemBase::draw_edit_screen(namn, move_menu_scale >= 0.1f ? ftostr41sign(X_Coordinate_Move) : ftostr43sign(X_Coordinate_Move));
      }else if(axis == Y_AXIS){
        MenuEditItemBase::draw_edit_screen(namn, move_menu_scale >= 0.1f ? ftostr41sign(Y_Coordinate_Move) : ftostr43sign(Y_Coordinate_Move));
      }else{
        MenuEditItemBase::draw_edit_screen(namn, move_menu_scale >= 0.1f ? ftostr41sign(Z_Coordinate_Move) : ftostr43sign(Z_Coordinate_Move));
      }
      
   // }
  }


//  
// "Motion" > "Move Axis" submenu
//

static void _lcd_move_xyz(PGM_P const name, const AxisEnum axis) {
  if (ui.use_click()) return ui.goto_previous_screen_no_defer();
  if (ui.encoderPosition && !ui.processing_manual_move) {

    // Start with no limits to movement
    float min = current_position[axis] - 1000,
          max = current_position[axis] + 1000;

    // Limit to software endstops, if enabled
    #if HAS_SOFTWARE_ENDSTOPS
      if (soft_endstops_enabled) switch (axis) {
        case X_AXIS:
          #if ENABLED(MIN_SOFTWARE_ENDSTOP_X)
            min = soft_endstop.min.x;
          #endif
          #if ENABLED(MAX_SOFTWARE_ENDSTOP_X)
            max = soft_endstop.max.x;
          #endif
          break;
        case Y_AXIS:
          #if ENABLED(MIN_SOFTWARE_ENDSTOP_Y)
            min = soft_endstop.min.y;
          #endif
          #if ENABLED(MAX_SOFTWARE_ENDSTOP_Y)
            max = soft_endstop.max.y;
          #endif
          break;
        case Z_AXIS:
          #if ENABLED(MIN_SOFTWARE_ENDSTOP_Z)
            min = soft_endstop.min.z;
          #endif
          #if ENABLED(MAX_SOFTWARE_ENDSTOP_Z)
            max = soft_endstop.max.z;
          #endif
        default: break;
      }
    #endif // HAS_SOFTWARE_ENDSTOPS

    // Delta limits XY based on the current offset from center
    // This assumes the center is 0,0
    #if ENABLED(DELTA)
      if (axis != Z_AXIS) {
        max = SQRT(sq((float)(DELTA_PRINTABLE_RADIUS)) - sq(current_position[Y_AXIS - axis])); // (Y_AXIS - axis) == the other axis
        min = -max;
      }
    #endif

    // Get the new position
    const float diff = float(int32_t(ui.encoderPosition)) * move_menu_scale;
    #if IS_KINEMATIC
      manual_move_offset += diff;
      if (int32_t(ui.encoderPosition) < 0)
        NOLESS(manual_move_offset, min - current_position[axis]);
      else
        NOMORE(manual_move_offset, max - current_position[axis]);
    #else
      current_position[axis] += diff;
      if (int32_t(ui.encoderPosition) < 0)
        NOLESS(current_position[axis], min);
      else
        NOMORE(current_position[axis], max);
    #endif

    manual_move_to_current(axis);
    ui.refresh(LCDVIEW_REDRAW_NOW);
  }
  ui.encoderPosition = 0;
  if (ui.should_draw()) {
    const float pos = NATIVE_TO_LOGICAL(ui.processing_manual_move ? destination[axis] : current_position[axis]
      #if IS_KINEMATIC
        + manual_move_offset
      #endif
    , axis);
    MenuEditItemBase::draw_edit_screen(name, move_menu_scale >= 0.1f ? ftostr41sign(pos) : ftostr43sign(pos));
  }
}
void lcd_move_x() { _lcd_move_xyz(GET_TEXT(MSG_MOVE_X), X_AXIS); }
void lcd_move_y() { _lcd_move_xyz(GET_TEXT(MSG_MOVE_Y), Y_AXIS); }
void lcd_move_z() { _lcd_move_xyz(GET_TEXT(MSG_MOVE_Z), Z_AXIS); }

#if E_MANUAL

  static void lcd_move_e(
    #if E_MANUAL > 1
      const int8_t eindex=-1
    #endif
  ) {
    if (ui.use_click()) return ui.goto_previous_screen_no_defer();
    if (ui.encoderPosition) {
      if (!ui.processing_manual_move) {
        const float diff = float(int32_t(ui.encoderPosition)) * move_menu_scale;
        #if IS_KINEMATIC
          manual_move_offset += diff;
        #else
          current_position.e += diff;
        #endif
        manual_move_to_current(E_AXIS
          #if E_MANUAL > 1
            , eindex
          #endif
        );
        ui.refresh(LCDVIEW_REDRAW_NOW);
      }
      ui.encoderPosition = 0;
    }
    if (ui.should_draw()) {
      #if E_MANUAL > 1
        MenuItemBase::init(eindex);
      #endif
      MenuEditItemBase::draw_edit_screen(
        GET_TEXT(
          #if E_MANUAL > 1
            MSG_MOVE_EN
          #else
            MSG_MOVE_E
          #endif
        ),
        ftostr41sign(current_position.e
          #if IS_KINEMATIC
            + manual_move_offset
          #endif
          #if ENABLED(MANUAL_E_MOVES_RELATIVE)
            - manual_move_e_origin
          #endif
        )
      );
    } // should_draw
  }

#endif // E_MANUAL

//Kalle below are a couple of functions and menus to create the custom "Set Coordinates"-menu function that lets you set the coordinates befor executing the move!

  void MoveToCoordinates(){
    
    char buffer_X[15];
    char buffer_Y[15];
    char buffer_Z[15];
    char cmd[30]; 
    dtostrf(X_Coordinate_Move, 1, 2, buffer_X); //1=min width of string, 2=decimals
    dtostrf(Y_Coordinate_Move, 1, 2, buffer_Y);
    dtostrf(Z_Coordinate_Move, 1, 2, buffer_Z);
    
    if(Z_Coordinate_Move>0){
      //Start with Z to move it (up) first...
      snprintf_P(cmd, 30, PSTR("G1 F200 Z%s"), buffer_Z);

      queue_Kalle.enqueue_one_now(cmd);//execute 1st command
    
      snprintf_P(cmd,30,PSTR("G1 F2000 X%s Y%s"), buffer_X, buffer_Y);
    
    }else{        
      sprintf(cmd,"G1 F2000 X%s Y%s", buffer_X, buffer_Y);
      queue_Kalle.enqueue_one_now(cmd); //execute 1st command
      
      sprintf(cmd, "G1 F200 Z%s",buffer_Z);
    }
    //execute 2nd command
    queue_Kalle.enqueue_one_now(cmd);
    
    //ui.goto_previous_screen_no_defer();
    ui.return_to_status();
  }
//Help function to pass variables (not possible like this in Menu-item)
  static void lcd_pos_x() { SetCoordinateWithMultiplier(PSTR("X"), X_AXIS); }
  static void lcd_pos_y() { SetCoordinateWithMultiplier(PSTR("Y"), Y_AXIS); }
  static void lcd_pos_z() { SetCoordinateWithMultiplier(PSTR("Z"), Z_AXIS); }

//Function to owerwrite text on screen (used to add current choosen coordinates next to menu item X, Y and Z) like "X  2.23"
  static void PrintOnLCDKalle(int RowIndex, int ColIndex, const char* text){
    
    uint8_t char_width = 6; 
    uint8_t char_height = 12; 

    uint8_t xStart = ColIndex*char_width+1;
    uint8_t yStart = RowIndex*char_height; //RowIndex*rowHeight + kHalfChar;
  
    u8g.setPrintPos(xStart, yStart);
    u8g.print(text);
  }
//Custom menu to set coordinates before you move! 
  void SetCoodinatesAndMove() {
    START_MENU();
    BACK_ITEM(MSG_MOVE_AXIS);
    SUBMENU(MSG_Kalle_X, lcd_pos_x);
    SUBMENU(MSG_Kalle_Y, lcd_pos_y);
    SUBMENU(MSG_Kalle_Z, lcd_pos_z);

    //MENU_ITEM(function, "Execute move", MoveToCoordinates);
    SUBMENU(MSG_Kalle_Execute, MoveToCoordinates);
    END_MENU();
  }

//
// "Motion" > "Move Xmm" > "Move XYZ" submenu
//

#ifndef SHORT_MANUAL_Z_MOVE
  #define SHORT_MANUAL_Z_MOVE 0.025
#endif

screenFunc_t _manual_move_func_ptr;

void _goto_manual_move(const float scale) {
  ui.defer_status_screen();
  move_menu_scale = scale;
  ui.goto_screen(_manual_move_func_ptr);
}

void _menu_move_distance(const AxisEnum axis, const screenFunc_t func, const int8_t eindex=-1) {
  _manual_move_func_ptr = func;
  START_MENU();
  if (LCD_HEIGHT >= 4) {
    switch (axis) {
      case X_AXIS: STATIC_ITEM(MSG_MOVE_X, SS_CENTER|SS_INVERT); break;
      case Y_AXIS: STATIC_ITEM(MSG_MOVE_Y, SS_CENTER|SS_INVERT); break;
      case Z_AXIS: STATIC_ITEM(MSG_MOVE_Z, SS_CENTER|SS_INVERT); break;
      default:
        #if ENABLED(MANUAL_E_MOVES_RELATIVE)
          manual_move_e_origin = current_position.e;
        #endif
        STATIC_ITEM(MSG_MOVE_E, SS_CENTER|SS_INVERT);
        break;
    }
  }
  #if ENABLED(PREVENT_COLD_EXTRUSION)
    if (axis == E_AXIS && thermalManager.tooColdToExtrude(eindex >= 0 ? eindex : active_extruder))
      BACK_ITEM(MSG_HOTEND_TOO_COLD);
    else
  #endif
  {
    BACK_ITEM(MSG_MOVE_AXIS);
    SUBMENU(MSG_MOVE_10MM, []{ _goto_manual_move(10);    });
    SUBMENU(MSG_MOVE_1MM,  []{ _goto_manual_move( 1);    });
    SUBMENU(MSG_MOVE_01MM, []{ _goto_manual_move( 0.1f); });
    if (axis == Z_AXIS && (SHORT_MANUAL_Z_MOVE) > 0.0f && (SHORT_MANUAL_Z_MOVE) < 0.1f) {
      extern const char NUL_STR[];
      SUBMENU_P(NUL_STR, []{ _goto_manual_move(float(SHORT_MANUAL_Z_MOVE)); });
      MENU_ITEM_ADDON_START(0
        #if HAS_CHARACTER_LCD
          + 1
        #endif
      );
        char tmp[20], numstr[10];
        // Determine digits needed right of decimal
        const uint8_t digs = !UNEAR_ZERO((SHORT_MANUAL_Z_MOVE) * 1000 - int((SHORT_MANUAL_Z_MOVE) * 1000)) ? 4 :
                             !UNEAR_ZERO((SHORT_MANUAL_Z_MOVE) *  100 - int((SHORT_MANUAL_Z_MOVE) *  100)) ? 3 : 2;
        sprintf_P(tmp, GET_TEXT(MSG_MOVE_Z_DIST), dtostrf(SHORT_MANUAL_Z_MOVE, 1, digs, numstr));
        lcd_put_u8str(tmp);
      MENU_ITEM_ADDON_END();
    }
  }
  END_MENU();
}

void menu_move() {
  START_MENU();
  BACK_ITEM(MSG_MOTION);

  #if HAS_SOFTWARE_ENDSTOPS && ENABLED(SOFT_ENDSTOPS_MENU_ITEM)
    EDIT_ITEM(bool, MSG_LCD_SOFT_ENDSTOPS, &soft_endstops_enabled);
  #endif

  if (
    #if IS_KINEMATIC || ENABLED(NO_MOTION_BEFORE_HOMING)
      all_axes_homed()
    #else
      true
    #endif
  ) {
    if (
      #if ENABLED(DELTA)
        current_position.z <= delta_clip_start_height
      #else
        true
      #endif
    ) {
      SUBMENU(MSG_MOVE_X, []{ _menu_move_distance(X_AXIS, lcd_move_x); });
      SUBMENU(MSG_MOVE_Y, []{ _menu_move_distance(Y_AXIS, lcd_move_y); });
    }
    #if ENABLED(DELTA)
      else
        ACTION_ITEM(MSG_FREE_XY, []{ line_to_z(delta_clip_start_height); ui.synchronize(); });
    #endif

    SUBMENU(MSG_MOVE_Z, []{ _menu_move_distance(Z_AXIS, lcd_move_z); });
  }
  else
    GCODES_ITEM(MSG_AUTO_HOME, G28_STR);

  #if ANY(SWITCHING_EXTRUDER, SWITCHING_NOZZLE, MAGNETIC_SWITCHING_TOOLHEAD)

    #if EXTRUDERS >= 4
      switch (active_extruder) {
        case 0: GCODES_ITEM_N(1, MSG_SELECT_E, PSTR("T1")); break;
        case 1: GCODES_ITEM_N(0, MSG_SELECT_E, PSTR("T0")); break;
        case 2: GCODES_ITEM_N(3, MSG_SELECT_E, PSTR("T3")); break;
        case 3: GCODES_ITEM_N(2, MSG_SELECT_E, PSTR("T2")); break;
        #if EXTRUDERS == 6
          case 4: GCODES_ITEM_N(5, MSG_SELECT_E, PSTR("T5")); break;
          case 5: GCODES_ITEM_N(4, MSG_SELECT_E, PSTR("T4")); break;
        #endif
      }
    #elif EXTRUDERS == 3
      if (active_extruder < 2) {
        if (active_extruder)
          GCODES_ITEM_N(0, MSG_SELECT_E, PSTR("T0"));
        else
          GCODES_ITEM_N(1, MSG_SELECT_E, PSTR("T1"));
      }
    #else
      if (active_extruder)
        GCODES_ITEM_N(0, MSG_SELECT_E, PSTR("T0"));
      else
        GCODES_ITEM_N(1, MSG_SELECT_E, PSTR("T1"));
    #endif

  #elif ENABLED(DUAL_X_CARRIAGE)

    if (active_extruder)
      GCODES_ITEM_N(0, MSG_SELECT_E, PSTR("T0"));
    else
      GCODES_ITEM_N(1, MSG_SELECT_E, PSTR("T1"));

  #endif

  #if E_MANUAL

    // The current extruder
    SUBMENU(MSG_MOVE_E, []{ _menu_move_distance(E_AXIS, []{ lcd_move_e(); }, -1); });

    #define SUBMENU_MOVE_E(N) SUBMENU_N(N, MSG_MOVE_EN, []{ _menu_move_distance(E_AXIS, []{ lcd_move_e(MenuItemBase::itemIndex); }, MenuItemBase::itemIndex); });

    #if EITHER(SWITCHING_EXTRUDER, SWITCHING_NOZZLE)

      // ...and the non-switching
      #if E_MANUAL == 5
        SUBMENU_MOVE_E(4);
      #elif E_MANUAL == 3
        SUBMENU_MOVE_E(2);
      #endif

    #elif E_MANUAL > 1

      // Independent extruders with one E-stepper per hotend
      LOOP_L_N(n, E_MANUAL) SUBMENU_MOVE_E(n);

    #endif

  #endif // E_MANUAL

  END_MENU();
}

#if ENABLED(AUTO_BED_LEVELING_UBL)
  void _lcd_ubl_level_bed();
#elif ENABLED(LCD_BED_LEVELING)
  void menu_bed_leveling();
#endif

void disableStepperMenu(){
  START_MENU();
  BACK_ITEM(MSG_MAIN);
  GCODES_ITEM(MSG_DISABLE_STEPPERS, PSTR("M18"));
  GCODES_ITEM(MSG_DISABLE_STEPPER_XY, PSTR("M18 X Y"));
  GCODES_ITEM(MSG_DISABLE_STEPPER_Z, PSTR("M18 Z"));
  END_MENU();
}
void goToHhomeAll(){
  queue_Kalle.enqueue_one_now("G1 X0 Y0 F2000");
  queue_Kalle.enqueue_one_now("G1 Z0 F300");
  ui.return_to_status();
}

void gotoHomeMenu(){
  START_MENU();
  BACK_ITEM(MSG_MAIN);
  SUBMENU(MSG_Kalle_All,goToHhomeAll);
  
  GCODES_ITEM(MSG_Kalle_XY, PSTR("G1 X0 Y0 F2000"));
  GCODES_ITEM(MSG_Kalle_Z, PSTR("G1 Z0 F300"));
  GCODES_ITEM(MSG_Kalle_X, PSTR("G1 X0 F2000"));
  GCODES_ITEM(MSG_Kalle_Y, PSTR("G1 X0 F2000"));
  END_MENU();
}
void setHomeHere(){
  queue_Kalle.enqueue_one_now("G92 X0 Y0 Z0");
  axis_known_position = 0xFF;
  ui.return_to_status();
}
void setZHomeHere(){
  queue_Kalle.enqueue_one_now("G92 Z0");
  bitSet(axis_known_position,Z_AXIS);
  ui.return_to_status();
}
void homingMenu(){
  START_MENU();
  BACK_ITEM(MSG_MAIN);
  //
  // Auto Home
  //
  SUBMENU(MSG_Kalle_MakeThisHome,setHomeHere);
  SUBMENU(MSG_Kalle_MakeThisHomeZ,setZHomeHere);
  //GCODES_ITEM(MSG_Kalle_MakeThisHome, PSTR("G92 X0 Y0 Z0"));

  #if ENABLED(INDIVIDUAL_AXIS_HOMING_MENU)
    
    GCODES_ITEM(MSG_AUTO_HOME_XY, PSTR("G28 X Y"));
    GCODES_ITEM(MSG_AUTO_HOME_X, PSTR("G28X"));
    GCODES_ITEM(MSG_AUTO_HOME_Y, PSTR("G28Y"));
    GCODES_ITEM(MSG_AUTO_HOME_Z, PSTR("G28Z"));
  #endif

  GCODES_ITEM(MSG_AUTO_HOME, G28_STR);

  END_MENU();
}
void menu_motion() {
  START_MENU();

  //
  // ^ Main
  //
  BACK_ITEM(MSG_MAIN);

  //
  // Move Axis
  //
  #if ENABLED(DELTA)
    if (all_axes_homed())
  #endif
      SUBMENU(MSG_MOVE_AXIS, menu_move);

  SUBMENU(MSG_Kalle_SetCoordinates, SetCoodinatesAndMove);

  
  //
  // Auto Z-Align
  //
  #if ENABLED(Z_STEPPER_AUTO_ALIGN)
    GCODES_ITEM(MSG_AUTO_Z_ALIGN, PSTR("G34"));
  #endif

  //
  // Level Bed
  //
  #if ENABLED(AUTO_BED_LEVELING_UBL)

    SUBMENU(MSG_UBL_LEVEL_BED, _lcd_ubl_level_bed);

  #elif ENABLED(LCD_BED_LEVELING)

    if (!g29_in_progress) SUBMENU(MSG_BED_LEVELING, menu_bed_leveling);

  #elif HAS_LEVELING && DISABLED(SLIM_LCD_MENUS)

    #if DISABLED(PROBE_MANUALLY)
      GCODES_ITEM(MSG_LEVEL_BED, PSTR("G28\nG29"));
    #endif
    if (all_axes_homed() && leveling_is_valid()) {
      bool show_state = planner.leveling_active;
      EDIT_ITEM(bool, MSG_BED_LEVELING, &show_state, _lcd_toggle_bed_leveling);
    }
    #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
      editable.decimal = planner.z_fade_height;
      EDIT_ITEM_FAST(float3, MSG_Z_FADE_HEIGHT, &editable.decimal, 0, 100, []{ set_z_fade_height(editable.decimal); });
    #endif

  #endif

  #if ENABLED(LEVEL_BED_CORNERS) && DISABLED(LCD_BED_LEVELING)
    ACTION_ITEM(MSG_LEVEL_CORNERS, _lcd_level_bed_corners);
  #endif

  #if ENABLED(Z_MIN_PROBE_REPEATABILITY_TEST)
    GCODES_ITEM(MSG_M48_TEST, PSTR("G28\nM48 P10"));
  #endif

  //
  // Disable Steppers
  //
  GCODES_ITEM(MSG_DISABLE_STEPPERS, PSTR("M84"));

  END_MENU();
}

#endif // HAS_LCD_MENU
