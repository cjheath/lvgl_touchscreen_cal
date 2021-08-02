/*
 * Touchscreen calibration for LVGL v8
 *
 * Copyright (c) 2021 Clifford Heath.
 * ALL USES ALLOWED. NO LIABILITY ACCEPTED. ACKNOWLEDGEMENT REQUIRED.
 * Inspired by tpcal, unknown author(s).
 */
#include "lvgl/lvgl.h"
#include "stdio.h"

#define TARGET_SIZE      20	// Size of the target circle

typedef enum {
    TS_CAL_STATE_START,
    TS_CAL_STATE_WAIT_TOP_LEFT,
    TS_CAL_STATE_WAIT_TOP_RIGHT,
    TS_CAL_STATE_WAIT_BOTTOM_RIGHT,
    TS_CAL_STATE_WAIT_BOTTOM_LEFT,
    TS_CAL_STATE_WAIT_LEAVE
} tp_cal_state_t;

static void		btn_click_action(lv_event_t* event);

static lv_point_t	calibration_points[4];	// top-left, top-right, bottom-right, bottom-left
static tp_cal_state_t	state;
static lv_obj_t*	prev_scr;	// Screen to return to
static lv_obj_t*	cal_screen;	// The calibration screen
static lv_obj_t*	big_btn;	// A button covering the whole screen, to gather clicks
static lv_obj_t*	instructions_label;	// A centred label for instructions
static lv_obj_t*	target;		// A small circle to act as the click target

// Hide the compiler warnings on the animation callback pointer casts:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
const lv_anim_exec_xcb_t    set_x_cb = (lv_anim_exec_xcb_t) lv_obj_set_x;
const lv_anim_exec_xcb_t    set_y_cb = (lv_anim_exec_xcb_t) lv_obj_set_y;
#pragma GCC diagnostic pop

// Clamp a coordinate value between in and max
static inline lv_coord_t
clamp(lv_coord_t val, lv_coord_t min, lv_coord_t max)
{
	return val < min ? min : (val > max ? max : val);
}

/*
 * Create a touch pad calibration screen
 */
void touchscreen_cal_create(void)
{
    // Save the previous screen to return to:
    prev_scr = lv_scr_act();

    // A new screen:
    cal_screen = lv_obj_create(NULL);
    lv_obj_remove_style(cal_screen, NULL, LV_PART_ANY | LV_STATE_ANY);
    lv_obj_set_size(cal_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_layout(cal_screen, 0);  // Disable layout of children. The first registered layout starts at 1
    lv_scr_load(cal_screen);

    // A big transparent button to receive clicks:
    big_btn = lv_btn_create(cal_screen);
    lv_obj_remove_style(big_btn, NULL, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(big_btn, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_opa(big_btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);  // Opacity zero
    lv_obj_add_event_cb(big_btn, btn_click_action, LV_EVENT_CLICKED, NULL);
    lv_obj_set_layout(big_btn, 0);  // Disable layout of children. The first registered layout starts at 1

    // A label to show instructions:
    instructions_label = lv_label_create(cal_screen);
    lv_obj_add_flag(instructions_label, LV_OBJ_FLAG_IGNORE_LAYOUT); // Don't get moved by parent!
    lv_obj_set_align(instructions_label, LV_ALIGN_CENTER);

    // A small circle object as the target:
    target = lv_obj_create(cal_screen);
    lv_obj_set_size(target, TARGET_SIZE, TARGET_SIZE);
    static lv_style_t   style_circ;
    lv_style_init(&style_circ);
    lv_style_set_radius(&style_circ, LV_RADIUS_CIRCLE);
    lv_obj_add_style(target, &style_circ, LV_PART_MAIN);
    lv_obj_clear_flag(target, LV_OBJ_FLAG_CLICKABLE);

    // Start off the fun with a non-event:
    state = TS_CAL_STATE_START;
    btn_click_action(0);
}

static void btn_click_action(lv_event_t* event)
{
    // Get the location of the current calibration target:
    lv_coord_t  current_x = lv_obj_get_x(target);
    lv_coord_t  current_y = lv_obj_get_y(target);
    lv_point_t  location;   // Location of the touch event
    printf("Target location is at %d, %d\n", current_x, current_y);

    if (event)
    {
        // I really want the untransformed hardware coordinates from the driver here.
        lv_indev_t* indev = lv_indev_get_act();
        lv_indev_get_point(indev, &location);
        printf("Touch at %d, %d\n", location.x, location.y);

        // Label the corner with the X and Y values:
        char	buf[64];
        snprintf(buf, 64, "x: %d\ny: %d", location.x, location.y);
        lv_obj_t*   label_coord = lv_label_create(cal_screen);
        lv_label_set_text(label_coord, buf);
        lv_obj_update_layout(label_coord);
	    // Position the coordinates label in the corner with a 5 pixel margin:
        lv_obj_set_pos(label_coord,
	        clamp(current_x, 5, LV_HOR_RES-lv_obj_get_width(label_coord)-5),
	        clamp(current_y, 5, LV_VER_RES-lv_obj_get_height(label_coord)-5));
    }

    // Set up the instructions and the animation to the new calibration target location:
    const char* instructions;
    lv_coord_t  anim_x = 0;
    lv_coord_t  anim_y = 0;
    switch (state)
    {
    default:    return;
    case TS_CAL_STATE_START:
        instructions = "Click the circle in\n"
                        "upper left-hand corner";
	current_x = LV_HOR_RES / 2;
	current_y = LV_VER_RES / 2;
        state = TS_CAL_STATE_WAIT_TOP_LEFT;
        break;

    case TS_CAL_STATE_WAIT_TOP_LEFT:
        calibration_points[0] = location;
        instructions = "Click the circle in\n"
                        "upper right-hand corner";
        anim_x = LV_HOR_RES - TARGET_SIZE;
        anim_y = 0;
        state = TS_CAL_STATE_WAIT_TOP_RIGHT;
        break;

    case TS_CAL_STATE_WAIT_TOP_RIGHT:
        calibration_points[1] = location;
        instructions = "Click the circle in\n"
                        "lower right-hand corner";
        anim_x = LV_HOR_RES - TARGET_SIZE;
        anim_y = LV_VER_RES - TARGET_SIZE;
        state = TS_CAL_STATE_WAIT_BOTTOM_RIGHT;
        break;

    case TS_CAL_STATE_WAIT_BOTTOM_RIGHT:
        calibration_points[2] = location;
        instructions = "Click the circle in\n"
                        "lower left-hand corner";
        anim_x = 0;
        anim_y = LV_VER_RES - TARGET_SIZE;
        state = TS_CAL_STATE_WAIT_BOTTOM_LEFT;
        break;

    case TS_CAL_STATE_WAIT_BOTTOM_LEFT:
        calibration_points[3] = location;
        anim_x = current_x;
        anim_y = current_y;
        instructions = "Click the screen\n"
                        "to leave calibration";
	// REVISIT: We need a button to restart the calibration here
        state = TS_CAL_STATE_WAIT_LEAVE;
        printf("Ready to leave calibration\n");
        break;

    case TS_CAL_STATE_WAIT_LEAVE:
        printf("Leaving calibration\n");
        lv_scr_load(prev_scr);
        // Delete the existing screen and all its contents in case it gets initiated again.
	lv_obj_del(cal_screen);
	cal_screen = 0;
	// REVISIT: Need to call a "screen calibration" callback to apply the new calibration
        return;
    }

    // Revise the instructions:
    lv_label_set_text(instructions_label, instructions);

    if (current_x != anim_x || current_y != anim_y)
    {
        printf("Animating from %d, %d to %d, %d\n", current_x, current_y, anim_x, anim_y);

        lv_anim_t   a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, target);
        lv_anim_set_values(&a, current_x, anim_x);
        lv_anim_set_exec_cb(&a, set_x_cb);
        lv_anim_set_delay(&a, 500);
        lv_anim_set_time(&a, 200);
        lv_anim_start(&a);

        lv_anim_set_values(&a, current_y, anim_y);
        lv_anim_set_exec_cb(&a, set_y_cb);
        lv_anim_set_time(&a, 200);
        lv_anim_start(&a);
        lv_obj_move_foreground(target);
    }
    else
        lv_obj_add_flag(target, LV_OBJ_FLAG_HIDDEN);  // Hide the target, it's not needed any more.
}
