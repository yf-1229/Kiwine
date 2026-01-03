#include <iostream>

#include "main.h"
#include <atomic>
#include <random>
#include "pico/stdlib.h"
#include "pico/aon_timer.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "external/ydf/ydf_model.h"

extern "C" {
#include "GUI_Paint.h"
#include "Infrared.h"
#include "LCD_1in3.h"
#include "external/Config/DEV_Config.h"
#include <stdio.h>
}
// --- Parameters --
bool game_status = true;

// LCD refresh rate
constexpr uint16_t LCD_REFRESH_DELAY_MS = 50;  // ~20 FPS

// pinecones
struct PineconeData {
    uint16_t x = 0;
    bool active = false;  // true=表示、false=非表示
};
static mutex_t g_mutex;
std::vector<PineconeData> g_pinecones;
constexpr size_t MAX_PINECONES = 100;  // 最大数

// pine
uint16_t pine_thickness = 32;
uint16_t pine_height = 10;
uint16_t pine_x = 104;

// user
uint8_t watered_times = 0;
uint8_t burned_times = 0;
uint8_t logged_times = 0;

// kiwi
enum class KiwiStatus : uint8_t {
    Idle,
    Eating,
    Wet
};

KiwiStatus kiwi_status = KiwiStatus::Idle; // TODO make Idle
uint16_t kiwi_x = 5;
uint16_t kiwi_y = 30;

// --- Functions ---

// Pinecone functions --->
void init_pinecones() { // use this function is only for test
    g_pinecones.reserve(MAX_PINECONES);

    // 最初に5個を配置
    g_pinecones.push_back({.x = 10, .active = true});
    g_pinecones.push_back({.x = 30, .active = true});
    g_pinecones.push_back({.x = 50, .active = true});
    g_pinecones.push_back({.x = 70, .active = true});
    g_pinecones.push_back({.x = 90, .active = true});
}

void update_pinecones(const uint16_t x) {
    mutex_enter_blocking(&g_mutex);

    for (auto& pc : g_pinecones) {
        if (!pc.active) {
            pc.x = x;
            pc.active = true;
            break;
        }
    }

    mutex_exit(&g_mutex);
}

void remove_pinecones(const uint16_t target_x, const uint8_t pineconeCollisionDistance = 5) {
    mutex_enter_blocking(&g_mutex);

    for (auto& pc : g_pinecones) {
        if (pc.active && abs(static_cast<int>(pc.x) - static_cast<int>(target_x)) < pineconeCollisionDistance) {
            pc.active = false;
            break;
        }
    }

    mutex_exit(&g_mutex);
}

// Draw functions --->
void draw_pinecones(const uint8_t size = 1) {
    mutex_enter_blocking(&g_mutex);
    const std::vector<PineconeData> local_pinecones = g_pinecones;
    mutex_exit(&g_mutex);
    for (const auto& pc : local_pinecones) {
        if (pc.active) {
            Paint_DrawRectangle(
                pc.x,
                LCD_1IN3_HEIGHT - size,
                pc.x + size,
                LCD_1IN3_HEIGHT,
                BROWN,
                DOT_PIXEL_4X4, DRAW_FILL_EMPTY);
        }
    }
}

void draw_pine(uint16_t height) {
    Paint_DrawRectangle(
        pine_x,
        LCD_1IN3_HEIGHT - height,
        pine_x + pine_thickness,
        LCD_1IN3_HEIGHT,
        BLACK,
        DOT_PIXEL_4X4, DRAW_FILL_FULL);
}

void draw_kiwi(uint16_t x) {
          Paint_DrawCircle(
           x,
           kiwi_y,
           3,
           GREEN,
           DOT_PIXEL_4X4,
           DRAW_FILL_EMPTY
          );
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
    LCD_1IN3_Display(BlackImage);
    uint32_t core1_msg = 0;
    game_status = true;

    while (1) {
        if (DEV_Digital_Read(keyUp) == 0) {
            printf("Button Pressed!"); // for Debug
            kiwi_status = KiwiStatus::Wet;
            kiwi_status = KiwiStatus::Idle;
        }
        if (DEV_Digital_Read(keyDown) == 0) {
            printf("Button Pressed!"); // for Debug
            kiwi_status = KiwiStatus::Eating;
            remove_pinecones(kiwi_x);
            kiwi_status = KiwiStatus::Idle;

            sleep_ms(200);
        }
        if (DEV_Digital_Read(keyLeft) == 0) {
            printf("Button Pressed!"); // for Debug
            if (kiwi_x < LCD_1IN3_HEIGHT) {
                kiwi_x --;
            }

            sleep_ms(200);
        }
        if (DEV_Digital_Read(keyRight) == 0) {
            printf("Button Pressed!"); // for Debug
            if (kiwi_x < LCD_1IN3_HEIGHT) {
            	kiwi_x ++;
            }

            sleep_ms(200);
        }

        // User Action
        if (DEV_Digital_Read(keyA) == 0 ) {
            // show_statics() // TODO: make this function
            pine_height += 10;
        }
        if (DEV_Digital_Read(keyB) == 0) {
        }
        if (DEV_Digital_Read(keyX) == 0) {
            kiwi_status = KiwiStatus::Eating;
        }
        if (DEV_Digital_Read(keyY) == 0) {
            // water_pine() // TODO: make this function
            watered_times++;
        }
        
        Paint_Clear(WHITE);
        draw_kiwi(kiwi_x);
        draw_pine(pine_height);
        draw_pinecones();  // アクティブなもののみ描画
        
        sleep_ms(LCD_REFRESH_DELAY_MS);
    }

    Paint_Clear(WHITE);
    free(BlackImage);
    BlackImage = NULL;
    DEV_Module_Exit();

    return 0;
}


// TIP コードを<b>Run</b>するには、<shortcut actionId="Run"/> を押すか、ガターにある <icon src="AllIcons.Actions.Execute"/> アイコンをクリックします。
int main() {
    stdio_init_all();
    printf("CORE0: start.\r\n");

    mutex_init(&g_mutex);
    init_pinecones();

    if (LCD() == 0) {
        printf("LCD failed\n");
    }
    while (true) {
        sleep_ms(1000);
    }

    return 0;
}
