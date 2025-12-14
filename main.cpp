#include <iostream>

#include "main.h"
#include <atomic>
#include <functional>
#include <random>
#include <iostream>
#include <mutex>
#include "pico/stdlib.h"
#include "pico/mutex.h"
#include "pico/aon_timer.h"
#include "pico/multicore.h"
#include "external/ydf/ydf_model.h"
#include "Infrared.h"
#include "pico/stdio.h"
#include <vector>

extern "C" {
#include <stdio.h>
#include "LCD_1in3.h"
#include "external/Config/DEV_Config.h"
#include "GUI_Paint.h"
#include "external/Fonts/fonts.h"
#include "hardware/adc.h"
#include "Infrared.h"
}

// --- Parameters ---
bool param_changed = false;
bool game_status = false;
bool screen_updated = false;


// bamboo
uint8_t bamboo_sum = 0;
uint8_t selected_bamboo = 0;
// user
uint8_t watered_times = 0;
uint8_t burned_times = 0;
uint8_t logged_times = 0;

class Bamboo {
    public:
    bool selected = false;
    uint16_t x;
    uint8_t thickness;
    uint8_t height;

public:
    Bamboo(bool s, uint16_t x, uint8_t t, uint8_t h) : selected(s), x(x), thickness(t), height(h) {}

    uint16_t get_bamboo_paramater(uint8_t watered_times) {
        // get_section_y() // TODO: from ydf
        // return std::make_pair(y1, y2, y3);
    }
};

class Rotten_bamboo {};


// --- Functions ---
// update User and Bamboo Parameter
void core1_entry() {
    // ハンドシェイク
    multicore_fifo_push_blocking(HELLO_MSG);
    while (1) {
        uint32_t rcvDat = multicore_fifo_pop_blocking();
        if (rcvDat == EXIT_LOOP) {
            printf("CORE1: Received");
            break;
        }
        std::vector<Bamboo> bamboos;
        bamboos.emplace_back(false, 10, 2, 50);
        bamboos.emplace_back(false, 20, 3, 80);
        bamboos.emplace_back(false, 30, 5, 20);
        bamboos.emplace_back(false, 40, 5, 20);
        bamboos.emplace_back(false, 50, 5, 20);
        for (auto& bamboo : bamboos) {
            bamboo.get_bamboo_paramater(watered_times);
        }
        // grow_bamboo() // TODO
        param_changed = true;
    }
    printf("CORE1: IDLE.\r\n");
    multicore_fifo_push_blocking(EXIT_MSG);
    while (true) {
        tight_loop_contents();
    }
}

void draw_bamboo(const std::vector<Bamboo>& bamboos) {
    for (const auto& bamboo : bamboos) {
        const std::atomic_uint16_t x_start(bamboo.x);
        const std::atomic_uint16_t x_end(bamboo.x + 2);
        const std::atomic_uint16_t y_start(LCD_1IN3.HEIGHT);
        const std::atomic_uint16_t y_end(LCD_1IN3.HEIGHT - bamboo.height - 5);

        Paint_DrawRectangle(
        x_start.load(),
        y_start.load(),
        x_end.load() + 5,
        y_end.load() + 5,
        0xFFFF,  // 白色
        DOT_PIXEL_4X4, DRAW_FILL_EMPTY);
    }
}

void draw_rottened_bamboo(uint8_t n) {
}

void draw_player_selected(const std::vector<Bamboo>& bamboos, uint8_t n) {
    const std::atomic_uint16_t x_start(bamboos[n].x);
    const std::atomic_uint16_t x_end(bamboos[n].x + 2);
    const std::atomic_uint16_t y_start(LCD_1IN3.HEIGHT);
    const std::atomic_uint16_t y_end(LCD_1IN3.HEIGHT - bamboos[n].height - 10);

    Paint_DrawRectangle(
        x_start.load(),
        y_start.load(),
        x_end.load() + 5,
        y_end.load() + 5,
        0xFFFF,  // 白色
        DOT_PIXEL_4X4, DRAW_FILL_EMPTY);
}

int LCD() {
    DEV_Delay_ms(100);
    printf("LCD_1in3_test \r\n");
    if (DEV_Module_Init() != 0) {
        return -1;
    }
    DEV_SET_PWM(50);
    printf("1.3inch LCD init...\r\n");
    LCD_1IN3_Init(HORIZONTAL);
    LCD_1IN3_Clear(WHITE);

    UDOUBLE Imagesize = LCD_1IN3_HEIGHT * LCD_1IN3_WIDTH * 2;
    UWORD *BlackImage;
    if ((BlackImage = (UWORD *) malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        exit(0);
    }

    Paint_NewImage((UBYTE *) BlackImage, LCD_1IN3.WIDTH, LCD_1IN3.HEIGHT, 0, WHITE);
    Paint_SetScale(65);
    Paint_Clear(WHITE);
    Paint_SetRotate(ROTATE_0);
    Paint_Clear(WHITE);

    // ボタンピンの初期化
    SET_Infrared_PIN(keyA);
    SET_Infrared_PIN(keyB);
    SET_Infrared_PIN(keyX);
    SET_Infrared_PIN(keyY);
    SET_Infrared_PIN(keyUp);
    SET_Infrared_PIN(keyDown);
    SET_Infrared_PIN(keyLeft);
    SET_Infrared_PIN(keyRight);
    SET_Infrared_PIN(keyCtrl);

    // 初期描画
    bool screen_updated = false;
    LCD_1IN3_Display(BlackImage);
    uint32_t core1_msg = 0;

    while (true) {
        screen_updated = false;
        while (true) {
            draw_bamboo(std::vector<Bamboo>{});
            // Select Bamboo
            if (DEV_Digital_Read(keyUp) == 0) {
                screen_updated = true;
                
            }
            if (DEV_Digital_Read(keyDown) == 0) {

                screen_updated = true;
            }
            if (DEV_Digital_Read(keyLeft) == 0) {

                screen_updated = true;
            }
            if (DEV_Digital_Read(keyRight) == 0) {

                screen_updated = true;
            }

            // User Action
            if (DEV_Digital_Read(keyA)) {
                // show_statics() // TODO: make this function
                screen_updated = true;
            }
            if (DEV_Digital_Read(keyB)) {
                // confirmation_dialog() // TODO: make this function
                // burn_bamboo(id) // TODO: make this function
                screen_updated = true;
            }
            if (DEV_Digital_Read(keyX)) {
                // confirmation_dialog() // TODO: make this function
                // cut_bamboo(id, selected_y) // TODO: make this function
                screen_updated = true;
            }
            if (DEV_Digital_Read(keyY)) {
                // water_bamboo() // TODO: make this function
                watered_times++;
                screen_updated = true;
            }

            // - Refresh Screen -
            if (screen_updated || param_changed) {
                LCD_1IN3_Display(BlackImage);
            }

            if (!game_status) {
                multicore_fifo_push_blocking(EXIT_LOOP);
                break;
            }
        }

        // Core1からのメッセージ受信
        core1_msg = multicore_fifo_pop_blocking();
        if (core1_msg != EXIT_MSG) {
            printf("Unexpected CORE1 EXIT MESSAGE.\r\n");
            return 1;
        }
        multicore_reset_core1();
        LCD_1IN3_Display(BlackImage);
        sleep_ms(16);  // 約60fps
        puts("Off");
        break;
    }

    free(BlackImage);
    BlackImage = NULL;
    DEV_Module_Exit();

    return 0;
}


// TIP コードを<b>Run</b>するには、<shortcut actionId="Run"/> を押すか、ガターにある <icon src="AllIcons.Actions.Execute"/> アイコンをクリックします。
int main() {
    stdio_init_all();
    printf("CORE0: start.\r\n");
    multicore_launch_core1(core1_entry);
    uint32_t core1_msg = multicore_fifo_pop_blocking();
    if (core1_msg != HELLO_MSG) {
        printf("Unexpected CORE1 HELLO MESSAGE.\r\n");
        return 1;
    }
    printf("CORE0: HELLO MESSAGE received.\r\n");

    while (!multicore_fifo_wready()) { // when FIFO has no room for more data
        printf("Waiting CORE1.\r\n");
    }

    LCD();

    return 0;
}