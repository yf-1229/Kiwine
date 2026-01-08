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
bool update_need = false;

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
constexpr uint8_t PINECONE_COLLISION_DISTANCE = 5;

// pine
constexpr uint16_t PINE_THICKNESS = 32;
uint16_t pine_height = 50;
constexpr uint16_t PINE_X = 104;

// kiwi
enum class KiwiStatus : uint8_t {
    Idle,
    Eating,
    Wet
};
KiwiStatus kiwi_status = KiwiStatus::Idle;
uint16_t kiwi_x = 40; // body
constexpr uint16_t KIWI_Y = LCD_1IN3_HEIGHT - 30; // body
constexpr uint8_t KIWI_SIZE = 20; // body
constexpr uint16_t KIWI_HEAD_SIZE = 7;
constexpr uint16_t KIWI_SPACE = KIWI_SIZE * 2 + KIWI_HEAD_SIZE * 2;

bool move_positive = true; // true = right, false = left

uint8_t kiwi_speed = 3;

// user
uint8_t watered_times = 30;
uint8_t burned_times = 1;


// --- Functions ---

void core1_entry() {
    using namespace ydf_model;
    uint32_t rcvDat = 0;
    uint16_t* const pine_height_ptr = &pine_height;
    bool* const update_need_ptr = &update_need;

    while (true) {
        Instance input{};
        input.rain_freq_monthly = watered_times;
        input.soil_nutrients = burned_times;
        input.current_height_px = *pine_height_ptr;
        float predicted_growth = Predict(input);
        printf("CORE1: Predicted Growth: %.4f\r\n", predicted_growth);

        if (*pine_height_ptr > LCD_1IN3_HEIGHT) {
            *pine_height_ptr = LCD_1IN3_HEIGHT;
        } else {
            *pine_height_ptr += static_cast<uint16_t>(predicted_growth);
            *update_need_ptr = true;
        }

        rcvDat = multicore_fifo_pop_blocking();
        if (rcvDat == EXIT_MSG) {
            break;
        }

        sleep_ms(100);
    }
    printf("CORE1: IDLE.\r\n");
    multicore_fifo_push_blocking(EXIT_MSG);
    while (true) {
        tight_loop_contents();
    }
}

void show_rain(bool active = false;)
{
    if (active) {
        constexpr int drop_count = 20;
        constexpr int drop_length = 10;
        constexpr int drop_spacing = 5;
        constexpr int start_y = 0;
        constexpr int end_y = LCD_1IN3_HEIGHT;

        for (uint8_t i = 0; i ++; drop_count) {
            Paint_DrawLine(
                // random
            )
        }
        
    } else {
        break;
    }
    

}

// Pinecone functions --->
void init_pinecones() { // use this function is only for test
    std::vector<PineconeData>* pinecones_ptr = &pinecones;
    pinecones_ptr->reserve(MAX_PINECONES);

    // 最初に5個を配置
    pinecones_ptr->push_back({.x = 10, .active = true});
    pinecones_ptr->push_back({.x = 30, .active = true});
    pinecones_ptr->push_back({.x = 50, .active = true});
    pinecones_ptr->push_back({.x = 70, .active = true});
    pinecones_ptr->push_back({.x = 90, .active = true});

}

void update_pinecones(const std::vector<PineconeData>* pinecones_ptr, const uint16_t target_x) {
    mutex_enter_blocking(&g_mutex);

    for (auto& pc : pinecones) {
        if (!pc.active) {
            pc.x = target_x;
            pc.active = true;
            break;
        }
    }

    mutex_exit(&g_mutex);
}

void remove_pinecones(const std::vector<PineconeData>* pinecones_ptr, const uint16_t target_x) {
    mutex_enter_blocking(&g_mutex);

    for (auto& pc : pinecones) {
        if (pc.active && abs(static_cast<int>(pc.x) - static_cast<int>(target_x)) < PINECONE_COLLISION_DISTANCE) {
            pc.active = false;
            break;
        }
    }

    mutex_exit(&g_mutex);
}

// Draw functions --->
void draw_pinecones(const std::vector<PineconeData>* pinecones_ptr) {
    for (const auto& pc : *pinecones_ptr) {
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
        PINE_X,
        LCD_1IN3_HEIGHT - height,
        PINE_X + PINE_THICKNESS,
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
                kiwi_head_x = x + KIWI_SIZE + KIWI_HEAD_SIZE;
                kiwi_head_y = KIWI_Y + KIWI_SIZE;
            } else {
                kiwi_head_x = x - KIWI_SIZE - KIWI_HEAD_SIZE;
                kiwi_head_y = KIWI_Y + KIWI_SIZE;
            }

            Paint_DrawCircle( // head
                kiwi_head_x,
                kiwi_head_y,
                KIWI_HEAD_SIZE,
                BROWN,
                DOT_PIXEL_4X4,
                DRAW_FILL_EMPTY
                );
            Paint_DrawCircle(
                kiwi_x,
                KIWI_Y,
                KIWI_SIZE,
                WHITE,
                DOT_PIXEL_4X4,
                DRAW_FILL_FULL
            );
            Paint_DrawCircle( // body
                x,
                KIWI_Y,
                KIWI_SIZE,
                GREEN,
                DOT_PIXEL_6X6,
                DRAW_FILL_EMPTY
                );
            break;

        case KiwiStatus::Wet:
            if (move_positive) {
                kiwi_head_x = x + KIWI_SIZE + KIWI_HEAD_SIZE;
                kiwi_head_y = KIWI_Y;
            } else {
                kiwi_head_x = x - KIWI_SIZE - KIWI_HEAD_SIZE;
                kiwi_head_y = KIWI_Y;
            }

            Paint_DrawCircle( // head
                kiwi_head_x,
                kiwi_head_y,
                KIWI_HEAD_SIZE,
                BROWN,
                DOT_PIXEL_4X4,
                DRAW_FILL_EMPTY
                );
            Paint_DrawCircle(
                kiwi_x,
                KIWI_Y,
                KIWI_SIZE,
                WHITE,
                DOT_PIXEL_4X4,
                DRAW_FILL_FULL
            );
            Paint_DrawCircle( // body
                x,
                KIWI_Y,
                KIWI_SIZE,
                GREEN,
                DOT_PIXEL_6X6,
                DRAW_FILL_EMPTY
                );
            break;

        case KiwiStatus::Idle:
        default:
            if (move_positive) {
                kiwi_head_x = x + KIWI_SIZE + KIWI_HEAD_SIZE;
                kiwi_head_y = KIWI_Y - KIWI_SIZE;
            } else {
                kiwi_head_x = x - KIWI_SIZE - KIWI_HEAD_SIZE;
                kiwi_head_y = KIWI_Y - KIWI_SIZE;
            }

            Paint_DrawCircle( // head
                kiwi_head_x,
                kiwi_head_y,
                KIWI_HEAD_SIZE,
                BROWN,
                DOT_PIXEL_4X4,
                DRAW_FILL_EMPTY
                );
            Paint_DrawCircle(
                kiwi_x,
                KIWI_Y,
                KIWI_SIZE,
                WHITE,
                DOT_PIXEL_4X4,
                DRAW_FILL_FULL
            );
            Paint_DrawCircle( // body
                x,
                KIWI_Y,
                KIWI_SIZE,
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
    bool* const update_need_ptr = &update_need;
    uint8_t* watered_times_ptr = &watered_times;
    uint16_t* kiwi_x_ptr = &kiwi_x;
    uint16_t* pine_height_ptr = &pine_height;
    draw_kiwi(*kiwi_x_ptr);
    draw_pine(*pine_height_ptr);
    std::vector<PineconeData>* pinecones_ptr = &pinecones;
    bool raining = false;
    while (true) {
        Paint_Clear(WHITE);

        if (DEV_Digital_Read(keyUp) == 0) {
            printf("keyUp Pressed!\r\n"); // for Debug
            kiwi_status = KiwiStatus::Wet;
            *watered_times_ptr ++;
            raining = false;
            *update_need_ptr = true;
            sleep_ms(LCD_REFRESH_DELAY_MS);
        } else {
            show_rain(raining);
        }
        if (DEV_Digital_Read(keyDown) == 0) {
            printf("keyDown Pressed!\r\n"); // for Debug
            kiwi_status = KiwiStatus::Eating;
            remove_pinecones(pinecones_ptr, *kiwi_x_ptr); // TODO to make pointer
            *update_need_ptr = true;
            sleep_ms(LCD_REFRESH_DELAY_MS);
        }
        if (DEV_Digital_Read(keyLeft) == 0 && *kiwi_x_ptr > KIWI_SPACE) {
            printf("keyLeft Pressed!\r\n"); // for Debug
            *kiwi_x_ptr -= kiwi_speed;
            move_positive = false;
            *update_need_ptr = true;
            sleep_ms(LCD_REFRESH_DELAY_MS);
        } else {
            draw_kiwi(*kiwi_x_ptr);
        }

        if (DEV_Digital_Read(keyRight) == 0 && *kiwi_x_ptr < (LCD_1IN3_WIDTH + KIWI_SPACE) ) {
            printf("KeyRight Pressed!\r\n"); // for Debug
            *kiwi_x_ptr += kiwi_speed;
            move_positive = true;
            *update_need_ptr = true;
            sleep_ms(LCD_REFRESH_DELAY_MS);
        } else {
            draw_kiwi(*kiwi_x_ptr);
        }

        // User Action
        if (DEV_Digital_Read(keyA) == 0 )
        {
            printf("KeyA Pressed!\r\n");
            multicore_fifo_push_blocking(EXIT_MSG);
            sleep_ms(LCD_REFRESH_DELAY_MS);
            break;
        }

        
        if (DEV_Digital_Read(keyB) == 0) {
            printf("KeyB Pressed!\r\n");
            // show_statics() // TODO: make this function
            sleep_ms(LCD_REFRESH_DELAY_MS);
        } else {
            draw_pine(pine_height);
        }
        
        if (DEV_Digital_Read(keyX) == 0) {
            printf("KeyX Pressed!\r\n");
            kiwi_status = KiwiStatus::Eating;
            *update_need_ptr = true;
            sleep_ms(LCD_REFRESH_DELAY_MS);
        } else {
        	draw_kiwi(*kiwi_x_ptr);
        }

        if (DEV_Digital_Read(keyY) == 0) {
            printf("KeyY Pressed!\r\n");
            // water_pine() // TODO: make this function
        } else {
            draw_pinecones(pinecones_ptr);
        }

        if (*update_need_ptr) {
            printf("Screen Updated!\r\n");
            LCD_1IN3_Display(BlackImage);

            mutex_enter_blocking(&g_mutex);
            kiwi_status = KiwiStatus::Idle;
        	*update_need_ptr = false;
            mutex_exit(&g_mutex);

            sleep_ms(LCD_REFRESH_DELAY_MS);
        }

        printf("CORE0: Send");
        multicore_fifo_push_blocking(HELLO_MSG);
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
