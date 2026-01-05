#include <iostream>

#include "main.h"
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

// --- Parameters ---
bool game_status = true;


// LCD refresh rate
constexpr uint16_t LCD_REFRESH_DELAY_MS = 50;  // ~20 FPS

// pinecones
struct PineconeData {
    uint16_t x = 0;
    bool active = false;  // true=表示、false=非表示
};
static mutex_t g_mutex;
std::vector<PineconeData> pinecones;
constexpr size_t MAX_PINECONES = 50;  // 最大数

// pine
uint16_t pine_thickness = 32;
uint16_t pine_height = 50;
constexpr uint16_t pine_x = 104;

// kiwi
enum class KiwiStatus : uint8_t {
    Idle,
    Eating,
    Wet
};
KiwiStatus kiwi_status = KiwiStatus::Idle;
uint16_t kiwi_x = 40; // body
constexpr uint16_t kiwi_y = LCD_1IN3_HEIGHT - 30; // body
constexpr uint8_t kiwi_size = 20; // body
constexpr uint16_t kiwi_head_size = 7;

bool move_positive = true; // true = right, false = left

uint8_t kiwi_speed = 3;

// user
uint8_t watered_times = 0;
uint8_t burned_times = 0;


// --- Functions ---

void core1_entry() {
    using namespace ydf_model;
    uint32_t rcvDat = 0;

    while (true) {
        Instance input{};
        input.rain_freq_monthly = watered_times;
        input.soil_nutrients = burned_times;
        input.current_height_px = pine_height;
        float predicted_growth = Predict(input);
        printf("CORE1: Predicted Growth: %.4f\r\n", predicted_growth);

        if (pine_height > LCD_1IN3_HEIGHT) {
            pine_height = LCD_1IN3_HEIGHT;
        } else {
            pine_height += static_cast<uint16_t>(predicted_growth);
        }

        if (multicore_fifo_rvalid()) {
            rcvDat = multicore_fifo_pop_blocking();
            if (rcvDat == EXIT_MSG) {
                break;
            }
        }

        sleep_ms(100);
    }
    printf("CORE1: IDLE.\r\n");
    while (true) {
        tight_loop_contents();
    }
}

// Pinecone functions --->
void init_pinecones() { // use this function is only for test
    pinecones.reserve(MAX_PINECONES);

    // 最初に5個を配置
    pinecones.push_back({.x = 10, .active = true});
    pinecones.push_back({.x = 30, .active = true});
    pinecones.push_back({.x = 50, .active = true});
    pinecones.push_back({.x = 70, .active = true});
    pinecones.push_back({.x = 90, .active = true});
}

void update_pinecones(const uint16_t x) {
    mutex_enter_blocking(&g_mutex);

    for (auto& pc : pinecones) {
        if (!pc.active) {
            pc.x = x;
            pc.active = true;
            break;
        }
    }

    mutex_exit(&g_mutex);
}

void remove_pinecones(uint16_t target_x, const uint8_t pineconeCollisionDistance = 5) {
    mutex_enter_blocking(&g_mutex);

    for (auto& pc : pinecones) {
        if (pc.active && abs(static_cast<int>(pc.x) - static_cast<int>(target_x)) < pineconeCollisionDistance) {
            pc.active = false;
            break;
        }
    }

    mutex_exit(&g_mutex);
}

// Draw functions --->
void draw_pinecones() {
    for (const auto& pc : pinecones) {
        if (pc.active) {
            constexpr int height = LCD_1IN3_HEIGHT;

            Paint_DrawChar(
                pc.x,
                height - 20,
                '#',
                &Font20,
                BLACK,
                WHITE
                );
        }
    }
}

void draw_pine(const uint16_t height) {
    Paint_DrawRectangle(
        pine_x,
        LCD_1IN3_HEIGHT - height,
        pine_x + pine_thickness,
        LCD_1IN3_HEIGHT,
        BLACK,
        DOT_PIXEL_4X4, DRAW_FILL_FULL);
}

void draw_kiwi(const uint16_t x) {
    uint16_t kiwi_head_x; // increase from (kiwi_x + kiwi_size)
    uint16_t kiwi_head_y;

    switch (kiwi_status) {
        case KiwiStatus::Eating:
            if (move_positive) {
                kiwi_head_x = x + kiwi_size + kiwi_head_size;
                kiwi_head_y = kiwi_y + kiwi_size;
            } else {
                kiwi_head_x = x - kiwi_size - kiwi_head_size;
                kiwi_head_y = kiwi_y + kiwi_size;
            }

            Paint_DrawCircle( // head
                kiwi_head_x,
                kiwi_head_y,
                kiwi_head_size,
                BROWN,
                DOT_PIXEL_4X4,
                DRAW_FILL_EMPTY
                );
            Paint_DrawCircle(
                kiwi_x,
                kiwi_y,
                kiwi_size,
                WHITE,
                DOT_PIXEL_4X4,
                DRAW_FILL_FULL
            );
            Paint_DrawCircle( // body
                x,
                kiwi_y,
                kiwi_size,
                GREEN,
                DOT_PIXEL_6X6,
                DRAW_FILL_EMPTY
                );
            break;

        case KiwiStatus::Wet:
            if (move_positive) {
                kiwi_head_x = x + kiwi_size + kiwi_head_size;
                kiwi_head_y = kiwi_y;
            } else {
                kiwi_head_x = x - kiwi_size - kiwi_head_size;
                kiwi_head_y = kiwi_y;
            }

            Paint_DrawCircle( // head
                kiwi_head_x,
                kiwi_head_y,
                kiwi_head_size,
                BROWN,
                DOT_PIXEL_4X4,
                DRAW_FILL_EMPTY
                );
            Paint_DrawCircle(
                kiwi_x,
                kiwi_y,
                kiwi_size,
                WHITE,
                DOT_PIXEL_4X4,
                DRAW_FILL_FULL
            );
            Paint_DrawCircle( // body
                x,
                kiwi_y,
                kiwi_size,
                GREEN,
                DOT_PIXEL_6X6,
                DRAW_FILL_EMPTY
                );
            break;

        case KiwiStatus::Idle:
        default:
            if (move_positive) {
                kiwi_head_x = x + kiwi_size + kiwi_head_size;
                kiwi_head_y = kiwi_y - kiwi_size;
            } else {
                kiwi_head_x = x - kiwi_size - kiwi_head_size;
                kiwi_head_y = kiwi_y - kiwi_size;
            }

            Paint_DrawCircle( // head
                kiwi_head_x,
                kiwi_head_y,
                kiwi_head_size,
                BROWN,
                DOT_PIXEL_4X4,
                DRAW_FILL_EMPTY
                );
            Paint_DrawCircle(
                kiwi_x,
                kiwi_y,
                kiwi_size,
                WHITE,
                DOT_PIXEL_4X4,
                DRAW_FILL_FULL
            );
            Paint_DrawCircle( // body
                x,
                kiwi_y,
                kiwi_size,
                GREEN,
                DOT_PIXEL_6X6,
                DRAW_FILL_EMPTY
                );
            break;
    }
 }


int LCD() {

    if (DEV_Module_Init() != 0) {
        return -1;
    }
    DEV_SET_PWM(50);
    printf("1.3inch LCD init...\r\n");
    LCD_1IN3_Init(HORIZONTAL);
    LCD_1IN3_Clear(BLACK);

    UDOUBLE Imagesize = LCD_1IN3_HEIGHT * LCD_1IN3_WIDTH * 2;
    UWORD *BlackImage;
    if ((BlackImage = static_cast<uint16_t *>(malloc(Imagesize))) == nullptr) {
        printf("Failed to apply for black memory...\r\n");
        exit(0);
    }

    Paint_NewImage(reinterpret_cast<uint8_t *>(BlackImage), LCD_1IN3.WIDTH, LCD_1IN3.HEIGHT, 0, WHITE);
    Paint_SetScale(65);
    Paint_Clear(BLACK);
    Paint_SetRotate(ROTATE_0);

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
    bool update_need = false;

    uint16_t kiwi_space = kiwi_size*2 + kiwi_head_size * 2;
    draw_kiwi(kiwi_x);
    draw_pine(pine_height);

    while (true) {
        Paint_Clear(WHITE);

        if (DEV_Digital_Read(keyUp) == 0) {
            printf("keyUp Pressed!\r\n"); // for Debug
            kiwi_status = KiwiStatus::Wet;
            update_need = true;
            sleep_ms(LCD_REFRESH_DELAY_MS);
        }
        if (DEV_Digital_Read(keyDown) == 0) {
            printf("keyDown Pressed!\r\n"); // for Debug
            kiwi_status = KiwiStatus::Eating;
            remove_pinecones(kiwi_x);
            update_need = true;
            sleep_ms(LCD_REFRESH_DELAY_MS);
        }
        if (DEV_Digital_Read(keyLeft) == 0 && kiwi_x > kiwi_space) {
            printf("keyLeft Pressed!\r\n"); // for Debug
            kiwi_x -= kiwi_speed;
            move_positive = false;
            update_need = true;
            sleep_ms(LCD_REFRESH_DELAY_MS);
        } else {
            draw_kiwi(kiwi_x);
        }

        if (DEV_Digital_Read(keyRight) == 0 && kiwi_x < (LCD_1IN3_WIDTH + kiwi_space) ) {
            printf("KeyRight Pressed!\r\n"); // for Debug
            kiwi_x += kiwi_speed;
            move_positive = true;
            update_need = true;
            sleep_ms(LCD_REFRESH_DELAY_MS);
        } else {
            draw_kiwi(kiwi_x);
        }

        // User Action
        if (DEV_Digital_Read(keyA) == 0 ) {
            printf("KeyA Pressed!\r\n");
            // show_statics() // TODO: make this function
            pine_height += 10; // TODO : For Debug
            update_need = true;
            sleep_ms(LCD_REFRESH_DELAY_MS);
        } else {
        	draw_pine(pine_height);
        }
        
        if (DEV_Digital_Read(keyB) == 0) {
            printf("KeyB Pressed!\r\n");
            sleep_ms(LCD_REFRESH_DELAY_MS);
            multicore_fifo_push_blocking(EXIT_MSG);
            break;
        }
        
        if (DEV_Digital_Read(keyX) == 0) {
            printf("KeyX Pressed!\r\n");
            kiwi_status = KiwiStatus::Eating;
            update_need = true;
            sleep_ms(LCD_REFRESH_DELAY_MS);
        } else {
        	draw_kiwi(kiwi_x);
        }

        if (DEV_Digital_Read(keyY) == 0) {
            printf("KeyY Pressed!\r\n");
            // water_pine() // TODO: make this function
            watered_times++;
        } else {
            draw_pinecones();
        }

        if (update_need) {
            printf("Screen Updated!\r\n");
            LCD_1IN3_Display(BlackImage);
            kiwi_status = KiwiStatus::Idle;
        	update_need = false;
            sleep_ms(LCD_REFRESH_DELAY_MS);
        }
    }
    multicore_reset_core1();
    LCD_1IN3_Display(BlackImage);
    sleep_ms(16);  // 約60fps
    puts("Off");
    free(BlackImage);
    BlackImage = NULL;
    DEV_Module_Exit();

    return 0;
}


int main() {
    stdio_init_all();
    printf("CORE0: start.\r\n");
    multicore_launch_core1(core1_entry);
    mutex_init(&g_mutex);
    init_pinecones();

    while (!multicore_fifo_wready()) { // when FIFO has no room for more data
        printf("Waiting CORE1.\r\n");
    }

    LCD();

    if (!game_status) {
        printf("GAME OVER.\r\n");
    }

    return 0;
}
