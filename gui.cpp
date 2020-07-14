/*
Copyright (c) 2019 lewis he
This is just a demonstration. Most of the functions are not implemented.
The main implementation is low-power standby.
The off-screen standby (not deep sleep) current is about 4mA.
Select standard motherboard and standard backplane for testing.
Created by Lewis he on October 10, 2019.
*/

// Please select the model you want to use in config.h
#include "config.h"
#include <Arduino.h>
#include <time.h>
#include "gui.h"
#include <WiFi.h>
#include "string.h"
#include <Ticker.h>
#include "FS.h"
#include "SD.h"

#include "StatusBar.h"
#include "MessageBox.h"
#include "MenuBar.h"

#define RTC_TIME_ZONE   "CST-8"


LV_FONT_DECLARE(Geometr);
LV_FONT_DECLARE(Ubuntu);
LV_IMG_DECLARE(bg);
LV_IMG_DECLARE(bg1);
LV_IMG_DECLARE(bg2);
LV_IMG_DECLARE(bg3);
LV_IMG_DECLARE(WALLPAPER_1_IMG);
LV_IMG_DECLARE(WALLPAPER_2_IMG);
LV_IMG_DECLARE(WALLPAPER_3_IMG);
LV_IMG_DECLARE(step);


LV_IMG_DECLARE(wifi);
LV_IMG_DECLARE(light);
LV_IMG_DECLARE(bluetooth);
LV_IMG_DECLARE(sd);
LV_IMG_DECLARE(setting);
LV_IMG_DECLARE(on);
LV_IMG_DECLARE(off);
LV_IMG_DECLARE(level1);
LV_IMG_DECLARE(level2);
LV_IMG_DECLARE(level3);
LV_IMG_DECLARE(iexit);
LV_IMG_DECLARE(modules);
LV_IMG_DECLARE(CAMERA_PNG);

extern EventGroupHandle_t g_event_group;
extern QueueHandle_t g_event_queue_handle;

static lv_style_t settingStyle;
static lv_obj_t *mainBar = nullptr;
static lv_obj_t *timeLabel = nullptr;
static lv_obj_t *menuBtn = nullptr;

static uint8_t globalIndex = 0;

static void lv_update_task(struct _lv_task_t *);
static void lv_battery_task(struct _lv_task_t *);
static void updateTime();
static void view_event_handler(lv_obj_t *obj, lv_event_t event);

static void wifi_event_cb();
static void sd_event_cb();
static void setting_event_cb();
static void light_event_cb();
static void modules_event_cb();
static void camera_event_cb();
static void wifi_destory();


MenuBar::lv_menu_config_t _cfg[7] = {
    {.name = "WiFi",  .img = (void *) &wifi, .event_cb = wifi_event_cb},
    {.name = "Bluetooth",  .img = (void *) &bluetooth, /*.event_cb = bluetooth_event_cb*/},
    {.name = "SD Card",  .img = (void *) &sd,  /*.event_cb =sd_event_cb*/},
    {.name = "Light",  .img = (void *) &light, /*.event_cb = light_event_cb*/},
    {.name = "Setting",  .img = (void *) &setting, /*.event_cb = setting_event_cb */},
    {.name = "Modules",  .img = (void *) &modules, /*.event_cb = modules_event_cb */},
    {.name = "Camera",  .img = (void *) &CAMERA_PNG, /*.event_cb = camera_event_cb*/ }
};

MenuBar menuBar;
StatusBar statusBar;

static void event_handler(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_SHORT_CLICKED) {  //!  Event callback Is in here
        if (obj == menuBtn) {
            lv_obj_set_hidden(mainBar, true);
            if (menuBar.self() == nullptr) {
                menuBar.createMenu(_cfg, sizeof(_cfg) / sizeof(_cfg[0]), view_event_handler);
                lv_obj_align(menuBar.self(), statusBar.self(), LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

            } else {
                menuBar.hidden(false);
            }
        }
    }
}


void setupGui()
{
    lv_style_init(&settingStyle);
    lv_style_set_radius(&settingStyle, LV_OBJ_PART_MAIN, 0);
    lv_style_set_bg_color(&settingStyle, LV_OBJ_PART_MAIN, LV_COLOR_GRAY);
    lv_style_set_bg_opa(&settingStyle, LV_OBJ_PART_MAIN, LV_OPA_0);
    lv_style_set_border_width(&settingStyle, LV_OBJ_PART_MAIN, 0);
    lv_style_set_text_color(&settingStyle, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);
    lv_style_set_image_recolor(&settingStyle, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);


    //Create wallpaper
    void *images[] = {(void *) &bg, (void *) &bg1, (void *) &bg2, (void *) &bg3 };
    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *img_bin = lv_img_create(scr, NULL);  /*Create an image object*/
    srand((int)time(0));
    int r = rand() % 4;
    lv_img_set_src(img_bin, images[r]);
    lv_obj_align(img_bin, NULL, LV_ALIGN_CENTER, 0, 0);

    //! Create & setup the status bar
    statusBar.createIcons(scr);
    updateBatteryLevel();
    lv_icon_battery_t icon = LV_ICON_CALCULATION;

    TTGOClass *ttgo = TTGOClass::getWatch();

    if (ttgo->power->isChargeing()) {
        icon = LV_ICON_CHARGE;
    }
    updateBatteryIcon(icon);

    //! main
    static lv_style_t mainStyle;
    lv_style_init(&mainStyle);
    lv_style_set_radius(&mainStyle, LV_OBJ_PART_MAIN, 0);
    lv_style_set_bg_color(&mainStyle, LV_OBJ_PART_MAIN, LV_COLOR_GRAY);
    lv_style_set_bg_opa(&mainStyle, LV_OBJ_PART_MAIN, LV_OPA_0);
    lv_style_set_border_width(&mainStyle, LV_OBJ_PART_MAIN, 0);
    lv_style_set_text_color(&mainStyle, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);
    lv_style_set_image_recolor(&mainStyle, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);


    mainBar = lv_cont_create(scr, NULL);
    lv_obj_set_size(mainBar,  LV_HOR_RES, LV_VER_RES - statusBar.height());
    lv_obj_add_style(mainBar, LV_OBJ_PART_MAIN, &mainStyle);
    lv_obj_align(mainBar, statusBar.self(), LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    //! Time
    static lv_style_t timeStyle;
    lv_style_copy(&timeStyle, &mainStyle);
    lv_style_set_text_font(&timeStyle, LV_STATE_DEFAULT, &Ubuntu);

    timeLabel = lv_label_create(mainBar, NULL);
    lv_obj_add_style(timeLabel, LV_OBJ_PART_MAIN, &timeStyle);
    updateTime();

    //! menu
    static lv_style_t style_pr;

    lv_style_init(&style_pr);
    lv_style_set_image_recolor(&style_pr, LV_OBJ_PART_MAIN, LV_COLOR_BLACK);
    lv_style_set_text_color(&style_pr, LV_OBJ_PART_MAIN, lv_color_hex3(0xaaa));

    menuBtn = lv_imgbtn_create(mainBar, NULL);

    /*lv_imgbtn_set_src(menuBtn, LV_BTN_STATE_ACTIVE, &menu);
    lv_imgbtn_set_src(menuBtn, LV_BTN_STATE_RELEASED, &menu);
    lv_imgbtn_set_src(menuBtn, LV_BTN_STATE_PRESSED, &menu);
    lv_imgbtn_set_src(menuBtn, LV_BTN_STATE_CHECKED_RELEASED, &menu);
    lv_imgbtn_set_src(menuBtn, LV_BTN_STATE_CHECKED_PRESSED, &menu);*/
    lv_obj_add_style(menuBtn, LV_OBJ_PART_MAIN, &style_pr);


    lv_obj_align(menuBtn, mainBar, LV_ALIGN_OUT_BOTTOM_MID, 0, -70);
    lv_obj_set_event_cb(menuBtn, event_handler);

    lv_task_create(lv_update_task, 1000, LV_TASK_PRIO_LOWEST, NULL);
    lv_task_create(lv_battery_task, 30000, LV_TASK_PRIO_LOWEST, NULL);
}

void updateStepCounter(uint32_t counter)
{
    statusBar.setStepCounter(counter);
}

static void updateTime()
{
    time_t now;
    struct tm  info;
    char buf[64];
    time(&now);
    localtime_r(&now, &info);
    strftime(buf, sizeof(buf), "%H:%M", &info);
    lv_label_set_text(timeLabel, buf);
    lv_obj_align(timeLabel, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);
}

void updateBatteryLevel()
{
    TTGOClass *ttgo = TTGOClass::getWatch();
    int p = ttgo->power->getBattPercentage();
    statusBar.updateLevel(p);
}

void updateBatteryIcon(lv_icon_battery_t icon)
{
    if (icon >= LV_ICON_CALCULATION) {
        TTGOClass *ttgo = TTGOClass::getWatch();
        int level = ttgo->power->getBattPercentage();
        if (level > 95)icon = LV_ICON_BAT_FULL;
        else if (level > 80)icon = LV_ICON_BAT_3;
        else if (level > 45)icon = LV_ICON_BAT_2;
        else if (level > 20)icon = LV_ICON_BAT_1;
        else icon = LV_ICON_BAT_EMPTY;
    }
    statusBar.updateBatteryIcon(icon);
}


static void lv_update_task(struct _lv_task_t *data)
{
    updateTime();
}

static void lv_battery_task(struct _lv_task_t *data)
{
    updateBatteryLevel();
}

static void view_event_handler(lv_obj_t *obj, lv_event_t event)
{
    int size = sizeof(_cfg) / sizeof(_cfg[0]);
    if (event == LV_EVENT_SHORT_CLICKED) {
        if (obj == menuBar.exitBtn()) {
            menuBar.hidden();
            lv_obj_set_hidden(mainBar, false);
            return;
        }
        for (int i = 0; i < size; i++) {
            if (obj == menuBar.obj(i)) {
                if (_cfg[i].event_cb != nullptr) {
                    menuBar.hidden();
                    _cfg[i].event_cb();
                }
                return;
            }
        }
    }
}

/*****************************************************************
 *
 *          ! Keyboard Class
 *
 */


class Keyboard
{
public:
    typedef enum {
        KB_EVENT_OK,
        KB_EVENT_EXIT,
    } kb_event_t;

    typedef void (*kb_event_cb)(kb_event_t event);

    Keyboard()
    {
        _kbCont = nullptr;
    };

    ~Keyboard()
    {
        if (_kbCont)
            lv_obj_del(_kbCont);
        _kbCont = nullptr;
    };

    void create(lv_obj_t *parent =  nullptr)
    {
        static lv_style_t kbStyle;

        lv_style_init(&kbStyle);
        lv_style_set_radius(&kbStyle, LV_OBJ_PART_MAIN, 0);
        lv_style_set_bg_color(&kbStyle, LV_OBJ_PART_MAIN, LV_COLOR_GRAY);
        lv_style_set_bg_opa(&kbStyle, LV_OBJ_PART_MAIN, LV_OPA_0);
        lv_style_set_border_width(&kbStyle, LV_OBJ_PART_MAIN, 0);
        lv_style_set_text_color(&kbStyle, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);
        lv_style_set_image_recolor(&kbStyle, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);

        if (parent == nullptr) {
            parent = lv_scr_act();
        }

        _kbCont = lv_cont_create(parent, NULL);
        lv_obj_set_size(_kbCont, LV_HOR_RES, LV_VER_RES - 30);
        lv_obj_align(_kbCont, NULL, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_style(_kbCont, LV_OBJ_PART_MAIN, &kbStyle);

        lv_obj_t *ta = lv_textarea_create(_kbCont, NULL);
        lv_obj_set_height(ta, 40);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_pwd_mode(ta, false);
        lv_textarea_set_text(ta, "");

        lv_obj_align(ta, _kbCont, LV_ALIGN_IN_TOP_MID, 10, 10);

        lv_obj_t *kb = lv_keyboard_create(_kbCont, NULL);
        lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER, btnm_mapplus[0]);
        lv_obj_set_height(kb, LV_VER_RES / 3 * 2);
        lv_obj_set_width(kb, 240);
        lv_obj_align(kb, _kbCont, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_textarea(kb, ta);

        lv_obj_add_style(kb, LV_OBJ_PART_MAIN, &kbStyle);
        lv_obj_add_style(ta, LV_OBJ_PART_MAIN, &kbStyle);

        lv_obj_set_event_cb(kb, __kb_event_cb);

        _kb = this;
    }

    void align(const lv_obj_t *base, lv_align_t align, lv_coord_t x = 0, lv_coord_t y = 0)
    {
        lv_obj_align(_kbCont, base, align, x, y);
    }

    static void __kb_event_cb(lv_obj_t *kb, lv_event_t event)
    {
        if (event != LV_EVENT_VALUE_CHANGED && event != LV_EVENT_LONG_PRESSED_REPEAT) return;
        lv_keyboard_ext_t *ext = (lv_keyboard_ext_t *)lv_obj_get_ext_attr(kb);
        const char *txt = lv_btnmatrix_get_active_btn_text(kb);
        if (txt == NULL) return;
        static int index = 0;
        if (strcmp(txt, LV_SYMBOL_OK) == 0) {
            strcpy(__buf, lv_textarea_get_text(ext->ta));
            if (_kb->_cb != nullptr) {
                _kb->_cb(KB_EVENT_OK);
            }
            return;
        } else if (strcmp(txt, "Exit") == 0) {
            if (_kb->_cb != nullptr) {
                _kb->_cb(KB_EVENT_EXIT);
            }
            return;
        } else if (strcmp(txt, LV_SYMBOL_RIGHT) == 0) {
            index = index + 1 >= sizeof(btnm_mapplus) / sizeof(btnm_mapplus[0]) ? 0 : index + 1;
            lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER, btnm_mapplus[index]);
            return;
        } else if (strcmp(txt, "Del") == 0) {
            lv_textarea_del_char(ext->ta);
        } else {
            lv_textarea_add_text(ext->ta, txt);
        }
    }

    void setKeyboardEvent(kb_event_cb cb)
    {
        _cb = cb;
    }

    const char *getText()
    {
        return (const char *)__buf;
    }

    void hidden(bool en = true)
    {
        lv_obj_set_hidden(_kbCont, en);
    }

private:
    lv_obj_t *_kbCont = nullptr;
    kb_event_cb _cb = nullptr;
    static const char *btnm_mapplus[10][23];
    static Keyboard *_kb;
    static char __buf[128];
};
char Keyboard::__buf[128];
Keyboard *Keyboard::_kb = nullptr;
const char *Keyboard::btnm_mapplus[10][23] = {
    {
        "a", "b", "c",   "\n",
        "d", "e", "f",   "\n",
        "g", "h", "i",   "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "j", "k", "l", "\n",
        "n", "m", "o",  "\n",
        "p", "q", "r",  "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "s", "t", "u",   "\n",
        "v", "w", "x", "\n",
        "y", "z", " ", "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "A", "B", "C",  "\n",
        "D", "E", "F",   "\n",
        "G", "H", "I",  "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "J", "K", "L", "\n",
        "N", "M", "O",  "\n",
        "P", "Q", "R", "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "S", "T", "U",   "\n",
        "V", "W", "X",   "\n",
        "Y", "Z", " ", "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "1", "2", "3",  "\n",
        "4", "5", "6",  "\n",
        "7", "8", "9",  "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "0", "+", "-",  "\n",
        "/", "*", "=",  "\n",
        "!", "?", "#",  "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "<", ">", "@",  "\n",
        "%", "$", "(",  "\n",
        ")", "{", "}",  "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "[", "]", ";",  "\n",
        "\"", "'", ".", "\n",
        ",", ":",  " ", "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    }
};


/*****************************************************************
 *
 *          ! Switch Class
 *
 */
class Switch
{
public:
    typedef struct {
        const char *name;
        void (*cb)(uint8_t, bool);
    } switch_cfg_t;

    typedef void (*exit_cb)();
    Switch()
    {
        _swCont = nullptr;
    }
    ~Switch()
    {
        if (_swCont)
            lv_obj_del(_swCont);
        _swCont = nullptr;
    }

    void create(switch_cfg_t *cfg, uint8_t count, exit_cb cb, lv_obj_t *parent = nullptr)
    {
        static lv_style_t swlStyle;
        lv_style_init(&swlStyle);
        lv_style_set_radius(&swlStyle, LV_OBJ_PART_MAIN, 0);
        lv_style_set_bg_color(&swlStyle, LV_OBJ_PART_MAIN, LV_COLOR_GRAY);
        lv_style_set_bg_opa(&swlStyle, LV_OBJ_PART_MAIN, LV_OPA_0);
        lv_style_set_border_width(&swlStyle, LV_OBJ_PART_MAIN, 0);
        lv_style_set_border_opa(&swlStyle, LV_OBJ_PART_MAIN, LV_OPA_50);
        lv_style_set_text_color(&swlStyle, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);
        lv_style_set_image_recolor(&swlStyle, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);


        if (parent == nullptr) {
            parent = lv_scr_act();
        }
        _exit_cb = cb;

        _swCont = lv_cont_create(parent, NULL);
        lv_obj_set_size(_swCont, LV_HOR_RES, LV_VER_RES - 30);
        lv_obj_align(_swCont, NULL, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_style(_swCont, LV_OBJ_PART_MAIN, &swlStyle);

        _count = count;
        _sw = new lv_obj_t *[count];
        _cfg = new switch_cfg_t [count];

        memcpy(_cfg, cfg, sizeof(switch_cfg_t) * count);

        lv_obj_t *prev = nullptr;
        for (int i = 0; i < count; i++) {
            lv_obj_t *la1 = lv_label_create(_swCont, NULL);
            lv_label_set_text(la1, cfg[i].name);
            i == 0 ? lv_obj_align(la1, NULL, LV_ALIGN_IN_TOP_LEFT, 30, 20) : lv_obj_align(la1, prev, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
            _sw[i] = lv_imgbtn_create(_swCont, NULL);
            lv_imgbtn_set_src(_sw[i], LV_BTN_STATE_ACTIVE, &off);
            lv_imgbtn_set_src(_sw[i], LV_BTN_STATE_RELEASED, &off);
            lv_imgbtn_set_src(_sw[i], LV_BTN_STATE_PRESSED, &off);
            lv_imgbtn_set_src(_sw[i], LV_BTN_STATE_CHECKED_RELEASED, &off);
            lv_imgbtn_set_src(_sw[i], LV_BTN_STATE_CHECKED_PRESSED, &off);
            lv_obj_set_click(_sw[i], true);

            lv_obj_align(_sw[i], la1, LV_ALIGN_OUT_RIGHT_MID, 80, 0);
            lv_obj_set_event_cb(_sw[i], __switch_event_cb);
            prev = la1;
        }

        _exitBtn = lv_imgbtn_create(_swCont, NULL);
        lv_imgbtn_set_src(_exitBtn, LV_BTN_STATE_ACTIVE, &iexit);
        lv_imgbtn_set_src(_exitBtn, LV_BTN_STATE_RELEASED, &iexit);
        lv_imgbtn_set_src(_exitBtn, LV_BTN_STATE_PRESSED, &iexit);
        lv_imgbtn_set_src(_exitBtn, LV_BTN_STATE_CHECKED_RELEASED, &iexit);
        lv_imgbtn_set_src(_exitBtn, LV_BTN_STATE_CHECKED_PRESSED, &iexit);
        lv_obj_set_click(_exitBtn, true);

        lv_obj_align(_exitBtn, _swCont, LV_ALIGN_IN_BOTTOM_MID, 0, -5);
        lv_obj_set_event_cb(_exitBtn, __switch_event_cb);

        _switch = this;
    }

    void align(const lv_obj_t *base, lv_align_t align, lv_coord_t x = 0, lv_coord_t y = 0)
    {
        lv_obj_align(_swCont, base, align, x, y);
    }

    void hidden(bool en = true)
    {
        lv_obj_set_hidden(_swCont, en);
    }

    static void __switch_event_cb(lv_obj_t *obj, lv_event_t event)
    {
        if (event == LV_EVENT_SHORT_CLICKED) {
            Serial.println("LV_EVENT_SHORT_CLICKED");
            if (obj == _switch->_exitBtn) {
                if ( _switch->_exit_cb != nullptr) {
                    _switch->_exit_cb();
                    return;
                }
            }
        }

        if (event == LV_EVENT_SHORT_CLICKED) {
            Serial.println("LV_EVENT_VALUE_CHANGED");
            for (int i = 0; i < _switch->_count ; i++) {
                lv_obj_t *sw = _switch->_sw[i];
                if (obj == sw) {
                    const void *src =  lv_imgbtn_get_src(sw, LV_BTN_STATE_RELEASED);
                    const void *dst = src == &off ? &on : &off;
                    bool en = src == &off;
                    lv_imgbtn_set_src(sw, LV_BTN_STATE_ACTIVE, dst);
                    lv_imgbtn_set_src(sw, LV_BTN_STATE_RELEASED, dst);
                    lv_imgbtn_set_src(sw, LV_BTN_STATE_PRESSED, dst);
                    lv_imgbtn_set_src(sw, LV_BTN_STATE_CHECKED_RELEASED, dst);
                    lv_imgbtn_set_src(sw, LV_BTN_STATE_CHECKED_PRESSED, dst);
                    if (_switch->_cfg[i].cb != nullptr) {
                        _switch->_cfg[i].cb(i, en);
                    }
                    return;
                }
            }
        }
    }

    void setStatus(uint8_t index, bool en)
    {
        if (index > _count)return;
        lv_obj_t *sw = _sw[index];
        const void *dst =  en ? &on : &off;
        lv_imgbtn_set_src(sw, LV_BTN_STATE_ACTIVE, dst);
        lv_imgbtn_set_src(sw, LV_BTN_STATE_RELEASED, dst);
        lv_imgbtn_set_src(sw, LV_BTN_STATE_PRESSED, dst);
        lv_imgbtn_set_src(sw, LV_BTN_STATE_CHECKED_RELEASED, dst);
        lv_imgbtn_set_src(sw, LV_BTN_STATE_CHECKED_PRESSED, dst);
    }

private:
    static Switch *_switch;
    lv_obj_t *_swCont = nullptr;
    uint8_t _count;
    lv_obj_t **_sw = nullptr;
    switch_cfg_t *_cfg = nullptr;
    lv_obj_t *_exitBtn = nullptr;
    exit_cb _exit_cb = nullptr;
};

Switch *Switch::_switch = nullptr;


/*****************************************************************
 *
 *          ! Preload Class
 *
 */
class Preload
{
public:
    Preload()
    {
        _preloadCont = nullptr;
    }
    ~Preload()
    {
        if (_preloadCont == nullptr) return;
        lv_obj_del(_preloadCont);
        _preloadCont = nullptr;
    }
    void create(lv_obj_t *parent = nullptr)
    {
        if (parent == nullptr) {
            parent = lv_scr_act();
        }
        if (_preloadCont == nullptr) {
            static lv_style_t plStyle;
            lv_style_init(&plStyle);
            lv_style_set_radius(&plStyle, LV_OBJ_PART_MAIN, 0);
            lv_style_set_bg_color(&plStyle, LV_OBJ_PART_MAIN, LV_COLOR_GRAY);
            lv_style_set_bg_opa(&plStyle, LV_OBJ_PART_MAIN, LV_OPA_0);
            lv_style_set_border_width(&plStyle, LV_OBJ_PART_MAIN, 0);
            lv_style_set_text_color(&plStyle, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);
            lv_style_set_image_recolor(&plStyle, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);


            static lv_style_t style;
            lv_style_init(&style);
            lv_style_set_radius(&style, LV_OBJ_PART_MAIN, 0);
            lv_style_set_bg_color(&style, LV_OBJ_PART_MAIN, LV_COLOR_GRAY);
            lv_style_set_bg_opa(&style, LV_OBJ_PART_MAIN, LV_OPA_0);
            lv_style_set_border_width(&style, LV_OBJ_PART_MAIN, 0);
            lv_style_set_text_color(&style, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);
            lv_style_set_image_recolor(&style, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);

            _preloadCont = lv_cont_create(parent, NULL);
            lv_obj_set_size(_preloadCont, LV_HOR_RES, LV_VER_RES - 30);
            lv_obj_align(_preloadCont, NULL, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
            lv_obj_add_style(_preloadCont, LV_OBJ_PART_MAIN, &plStyle);

            lv_obj_t *preload = lv_spinner_create(_preloadCont, NULL);
            lv_obj_set_size(preload, lv_obj_get_width(_preloadCont) / 2, lv_obj_get_height(_preloadCont) / 2);
            lv_obj_add_style(preload, LV_OBJ_PART_MAIN, &style);
            lv_obj_align(preload, _preloadCont, LV_ALIGN_CENTER, 0, 0);
        }
    }
    void align(const lv_obj_t *base, lv_align_t align, lv_coord_t x = 0, lv_coord_t y = 0)
    {
        lv_obj_align(_preloadCont, base, align, x, y);
    }

    void hidden(bool en = true)
    {
        lv_obj_set_hidden(_preloadCont, en);
    }

private:
    lv_obj_t *_preloadCont = nullptr;
};


/*****************************************************************
 *
 *          ! List Class
 *
 */

class List
{
public:
    typedef void(*list_event_cb)(const char *);
    List()
    {
    }
    ~List()
    {
        if (_listCont == nullptr) return;
        lv_obj_del(_listCont);
        _listCont = nullptr;
    }
    void create(lv_obj_t *parent = nullptr)
    {
        if (parent == nullptr) {
            parent = lv_scr_act();
        }
        if (_listCont == nullptr) {
            static lv_style_t listStyle;
            lv_style_init(&listStyle);
            lv_style_set_radius(&listStyle, LV_OBJ_PART_MAIN, 0);
            lv_style_set_bg_color(&listStyle, LV_OBJ_PART_MAIN, LV_COLOR_GRAY);
            lv_style_set_bg_opa(&listStyle, LV_OBJ_PART_MAIN, LV_OPA_0);
            lv_style_set_border_width(&listStyle, LV_OBJ_PART_MAIN, 0);
            lv_style_set_text_color(&listStyle, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);
            lv_style_set_image_recolor(&listStyle, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);

            _listCont = lv_list_create(lv_scr_act(), NULL);
            lv_list_set_scrollbar_mode(_listCont, LV_SCROLLBAR_MODE_OFF);
            lv_obj_set_size(_listCont, LV_HOR_RES, LV_VER_RES - 30);

            lv_obj_add_style(_listCont, LV_OBJ_PART_MAIN, &listStyle);
            lv_obj_align(_listCont, NULL, LV_ALIGN_CENTER, 0, 0);
        }
        _list = this;
    }

    void add(const char *txt, void *imgsrc = (void *)LV_SYMBOL_WIFI)
    {
        lv_obj_t *btn = lv_list_add_btn(_listCont, imgsrc, txt);
        lv_obj_set_event_cb(btn, __list_event_cb);
    }

    void align(const lv_obj_t *base, lv_align_t align, lv_coord_t x = 0, lv_coord_t y = 0)
    {
        lv_obj_align(_listCont, base, align, x, y);
    }

    void hidden(bool en = true)
    {
        lv_obj_set_hidden(_listCont, en);
    }

    static void __list_event_cb(lv_obj_t *obj, lv_event_t event)
    {
        if (event == LV_EVENT_SHORT_CLICKED) {
            const char *txt = lv_list_get_btn_text(obj);
            if (_list->_cb != nullptr) {
                _list->_cb(txt);
            }
        }
    }
    void setListCb(list_event_cb cb)
    {
        _cb = cb;
    }
private:
    lv_obj_t *_listCont = nullptr;
    static List *_list ;
    list_event_cb _cb = nullptr;
};
List *List::_list = nullptr;

/*****************************************************************
 *
 *          ! Task Class
 *
 */
class Task
{
public:
    Task()
    {
        _handler = nullptr;
        _cb = nullptr;
    }
    ~Task()
    {
        if ( _handler == nullptr)return;
        Serial.println("Free Task Func");
        lv_task_del(_handler);
        _handler = nullptr;
        _cb = nullptr;
    }

    void create(lv_task_cb_t cb, uint32_t period = 1000, lv_task_prio_t prio = LV_TASK_PRIO_LOW)
    {
        _handler = lv_task_create(cb,  period,  prio, NULL);
    };

private:
    lv_task_t *_handler = nullptr;
    lv_task_cb_t _cb = nullptr;
};


/*****************************************************************
 *
 *          ! GLOBAL VALUE
 *
 */
static Keyboard *kb = nullptr;
static Switch *sw = nullptr;
static Preload *pl = nullptr;
static List *list = nullptr;
static Task *task = nullptr;
static Ticker *gTicker = nullptr;
static MessageBox *messageBox = nullptr;

static char ssid[64], password[64];

/*****************************************************************
 *
 *          !WIFI EVENT
 *
 */
void wifi_connect_status(bool result)
{
    if (gTicker != nullptr) {
        delete gTicker;
        gTicker = nullptr;
    }
    if (kb != nullptr) {
        delete kb;
        kb = nullptr;
    }
    if (sw != nullptr) {
        delete sw;
        sw = nullptr;
    }
    if (pl != nullptr) {
        delete pl;
        pl = nullptr;
    }
    if (result) {
        statusBar.show(LV_STATUS_BAR_WIFI);
    } else {
        statusBar.hidden(LV_STATUS_BAR_WIFI);
    }
    menuBar.hidden(false);
}


void wifi_kb_event_cb(Keyboard::kb_event_t event)
{
    if (event == 0) {
        kb->hidden();
        Serial.println(kb->getText());
        strlcpy(password, kb->getText(), sizeof(password));
        pl->hidden(false);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        WiFi.begin(ssid, password);
        gTicker = new Ticker;
        gTicker->once_ms(5 * 1000, []() {
            wifi_connect_status(false);
        });
    } else if (event == 1) {
        delete kb;
        delete sw;
        delete pl;
        pl = nullptr;
        kb = nullptr;
        sw = nullptr;
        menuBar.hidden(false);
    }
}

static void wifi_sync_messagebox_cb(lv_task_t *t)
{
    static  struct tm timeinfo;
    bool ret = false;
    static int retry = 0;
    configTzTime(RTC_TIME_ZONE, "pool.ntp.org");
    while (1) {
        ret = getLocalTime(&timeinfo);
        if (!ret) {
            Serial.printf("get ntp fail,retry : %d \n", retry++);
        } else {
            //! del preload
            delete pl;
            pl = nullptr;

            char format[256];
            snprintf(format, sizeof(format), "Time acquisition is: %d-%d-%d/%d:%d:%d. Synchronize?", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            Serial.println(format);
            delete task;
            task = nullptr;

            //! messagebox
            static const char *btns[] = {"Ok", "Cancel", ""};
            messageBox = new MessageBox;
            messageBox->create(format, [](lv_obj_t *obj, lv_event_t event) {
                if (event == LV_EVENT_VALUE_CHANGED) {
                    const char *txt =  lv_msgbox_get_active_btn_text(obj);
                    if (!strcmp(txt, "Ok")) {

                        //!sync to rtc
                        struct tm *info =  (struct tm *)messageBox->getData();
                        Serial.printf("read use data = %d:%d:%d - %d:%d:%d \n", info->tm_year + 1900, info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);

                        TTGOClass *ttgo = TTGOClass::getWatch();
                        ttgo->rtc->setDateTime(info->tm_year + 1900, info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
                    } else if (!strcmp(txt, "Cancel")) {
                        //!cancel
                        // Serial.println("Cancel press");
                    }
                    delete messageBox;
                    messageBox = nullptr;
                    sw->hidden(false);
                }
            });
            messageBox->setBtn(btns);
            messageBox->setData(&timeinfo);
            return;
        }
    }
}




void wifi_sw_event_cb(uint8_t index, bool en)
{
    switch (index) {
    case 0:
        if (en) {
            WiFi.begin();
        } else {
            WiFi.disconnect();
            statusBar.hidden(LV_STATUS_BAR_WIFI);
        }
        break;
    case 1:
        sw->hidden();
        pl = new Preload;
        pl->create();
        pl->align(statusBar.self(), LV_ALIGN_OUT_BOTTOM_MID);
        WiFi.disconnect();
        WiFi.scanNetworks(true);
        break;
    case 2:
        if (!WiFi.isConnected()) {
            //TODO pop-up window
            Serial.println("WiFi is no connect");
            return;
        } else {
            if (task != nullptr) {
                Serial.println("task is runing ...");
                return;
            }
            task = new Task;
            task->create(wifi_sync_messagebox_cb);
            sw->hidden();
            pl = new Preload;
            pl->create();
            pl->align(statusBar.self(), LV_ALIGN_OUT_BOTTOM_MID);
        }
        break;
    default:
        break;
    }
}

void wifi_list_cb(const char *txt)
{
    strlcpy(ssid, txt, sizeof(ssid));
    delete list;
    list = nullptr;
    kb = new Keyboard;
    kb->create();
    kb->align(statusBar.self(), LV_ALIGN_OUT_BOTTOM_MID);
    kb->setKeyboardEvent(wifi_kb_event_cb);
}

void wifi_list_add(const char *ssid)
{
    if (list == nullptr) {
        pl->hidden();
        list = new List;
        list->create();
        list->align(statusBar.self(), LV_ALIGN_OUT_BOTTOM_MID);
        list->setListCb(wifi_list_cb);
    }
    list->add(ssid);
}


static void wifi_event_cb()
{
    Switch::switch_cfg_t cfg[3] = {{"Switch", wifi_sw_event_cb}, {"Scan", wifi_sw_event_cb}, {"NTP Sync", wifi_sw_event_cb}};
    sw = new Switch;
    sw->create(cfg, 3, []() {
        delete sw;
        sw = nullptr;
        menuBar.hidden(false);
    });
    sw->align(statusBar.self(), LV_ALIGN_OUT_BOTTOM_MID);
    sw->setStatus(0, WiFi.isConnected());
}


static void wifi_destory()
{
    Serial.printf("globalIndex:%d\n", globalIndex);
    switch (globalIndex) {
    //! wifi management main
    case 0:
        menuBar.hidden(false);
        delete sw;
        sw = nullptr;
        break;
    //! wifi ap list
    case 1:
        if (list != nullptr) {
            delete list;
            list = nullptr;
        }
        if (gTicker != nullptr) {
            delete gTicker;
            gTicker = nullptr;
        }
        if (kb != nullptr) {
            delete kb;
            kb = nullptr;
        }
        if (pl != nullptr) {
            delete pl;
            pl = nullptr;
        }
        sw->hidden(false);
        break;
    //! wifi keyboard
    case 2:
        if (gTicker != nullptr) {
            delete gTicker;
            gTicker = nullptr;
        }
        if (kb != nullptr) {
            delete kb;
            kb = nullptr;
        }
        if (pl != nullptr) {
            delete pl;
            pl = nullptr;
        }
        sw->hidden(false);
        break;
    case 3:
        break;
    default:
        break;
    }
    globalIndex--;
}


/*****************************************************************
 *
 *          !SETTING EVENT
 *
 */
static void setting_event_cb()
{


}


/*****************************************************************
 *
 *          ! LIGHT EVENT
 *
 */
static void light_sw_event_cb(uint8_t index, bool en)
{
    //Add lights that need to be controlled
}

static void light_event_cb()
{
    const uint8_t cfg_count = 4;
    Switch::switch_cfg_t cfg[cfg_count] = {
        {"light1", light_sw_event_cb},
        {"light2", light_sw_event_cb},
        {"light3", light_sw_event_cb},
        {"light4", light_sw_event_cb},
    };
    sw = new Switch;
    sw->create(cfg, cfg_count, []() {
        delete sw;
        sw = nullptr;
        menuBar.hidden(false);
    });

    sw->align(statusBar.self(), LV_ALIGN_OUT_BOTTOM_MID);

    //Initialize switch status
    for (int i = 0; i < cfg_count; i++) {
        sw->setStatus(i, 0);
    }
}


/*****************************************************************
 *
 *          ! MessageBox EVENT
 *
 */
static lv_obj_t *messagebox1 = nullptr;

static void create_messagebox(const char *txt, lv_event_cb_t event_cb)
{
    if (messagebox1 != nullptr)return;
    static const char *btns[] = {"Ok", ""};
    messagebox1 = lv_msgbox_create(lv_scr_act(), NULL);
    lv_msgbox_set_text(messagebox1, txt);
    lv_msgbox_add_btns(messagebox1, btns);
    lv_obj_set_width(messagebox1, LV_HOR_RES - 40);
    lv_obj_set_event_cb(messagebox1, event_cb);
    lv_obj_align(messagebox1, NULL, LV_ALIGN_CENTER, 0, 0);
}

static void destory_messagebox()
{
    if (pl != nullptr) {
        delete pl;
        pl = nullptr;
    }
    if (list != nullptr) {
        delete list;
        list = nullptr;
    }
    if (messagebox1 != nullptr) {
        lv_obj_del(messagebox1);
        messagebox1 = nullptr;
    }
}

/*****************************************************************
 *
 *          ! SD CARD EVENT
 *
 */

static void sd_event_cb()
{

}

/*****************************************************************
*
 *          ! Modules EVENT
 *
 */
static void modules_event_cb()
{

}


/*****************************************************************
*
 *          ! Camera EVENT
 *
 */

static void camera_event_cb()
{

}
