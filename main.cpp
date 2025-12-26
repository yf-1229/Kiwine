#include <iostream>

#include "main.h"
#include <atomic>
#include <random>
#include <iostream>
#include <mutex>
#include "pico/stdlib.h"
#include "pico/aon_timer.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "external/ydf/ydf_model.h"

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
bool game_status = true;
bool screen_updated = false;

// pinecones
struct PineconeData {
    uint16_t x = 0;
    uint16_t y = 128;
    bool active = false;  // true=表示、false=非表示
};
static mutex_t g_mutex;
std::vector<PineconeData> g_pinecones;
const size_t MAX_PINECONES = 100;  // 最大数

// pine
uint8_t pine_thickness = 1;
uint8_t pine_height = 1;
uint16_t pine_x = 64;

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

KiwiStatus kiwi_status = KiwiStatus::Idle;
uint16_t kiwi_x = 0;

// --- Functions ---

void init_pinecones() { // use this function is only for test
    g_pinecones.reserve(MAX_PINECONES);

    // 最初に5個を配置
    g_pinecones.push_back({10, true});
    g_pinecones.push_back({30, true});
    g_pinecones.push_back({50, true});
    g_pinecones.push_back({70, true});
    g_pinecones.push_back({90, true});
}
void update_pinecones(uint16_t x) {
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



// update User and pine Parameter
void core1_entry() {
    // ハンドシェイク
    multicore_fifo_push_blocking(HELLO_MSG);
    while (true) {
        uint32_t rcvDat = multicore_fifo_pop_blocking();
        if (rcvDat == EXIT_LOOP) {
            printf("CORE1: Received");
            break;
        }
        
        mutex_enter_blocking(&g_mutex);

        // grow_pine() // TODO
        param_changed = true;
        
        mutex_exit(&g_mutex);
        
        sleep_ms(100);
    }
    printf("CORE1: IDLE.\r\n");
    multicore_fifo_push_blocking(EXIT_MSG);
    while (true) {
        tight_loop_contents();
    }
    
}

void draw_pinecones() { // TODO: separate class draw_pine and set_pine
    for (const auto& pinecone : pinecones) {
        Paint_DrawRectangle(
        pinecone.x,
        pinecone_size_y,
        pinecone.x + 5,
        pinecone_size_y - 5,
        0xF800,
        DOT_PIXEL_4X4, DRAW_FILL_EMPTY);
    }
}

void draw_pine() {
    Paint_DrawRectangle(
        pine_x,
        LCD_1IN3_HEIGHT,
        pine_x + 5,
        LCD_1IN3_HEIGHT - 5,
        0xF800,
        DOT_PIXEL_4X4, DRAW_FILL_EMPTY);
}

void draw_kiwi(uint16_t x) {
    const std::atomic_uint16_t x_start(x);
    const std::atomic_uint16_t y_start(0);

    switch (kiwi_status) {
        case KiwiStatus::Eating: // pakupaku
            Paint_DrawCircle(
            x_start.load(),
            y_start.load(),
            3,
            0x07E0,
            DOT_PIXEL_4X4,
            DRAW_FILL_EMPTY
        );
            break;

        case KiwiStatus::Wet: // buruburu
            Paint_DrawCircle(
                    x_start.load(),
                    y_start.load(),
                    3,
                    0x07E0,
                    DOT_PIXEL_4X4,
                    DRAW_FILL_EMPTY
                    );
            kiwi_status = KiwiStatus::Idle;
            break;

        case KiwiStatus::Idle:
          default:
            break;

    }
 }

int LCD(std::vector<Pinecone> &pinecones) {
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
    uint16_t elapsed_time = 0;
    LCD_1IN3_Display(BlackImage);
    uint32_t core1_msg = 0;

    while (true) {
        screen_updated = false;
        while (true) {
            elapsed_time++;
            // --- Draw Screen ---
            draw_pine();
            draw_pinecones(pinecones);
            // TODO: replace to core1?

            if (DEV_Digital_Read(keyUp) == 0) {
                screen_updated = true;
                kiwi_status = KiwiStatus::Wet;
                draw_kiwi(kiwi_x);
                kiwi_status = KiwiStatus::Idle;
            }
            if (DEV_Digital_Read(keyDown) == 0) {
                screen_updated = true;
                kiwi_status = KiwiStatus::Eating;
                draw_kiwi(kiwi_x);
                kiwi_status = KiwiStatus::Idle;
            }
            if (DEV_Digital_Read(keyLeft) == 0) {
                screen_updated = true;
                kiwi_x --;
                draw_kiwi(kiwi_x);
            }
            if (DEV_Digital_Read(keyRight) == 0) {
                screen_updated = true;
                kiwi_x ++;
                draw_kiwi(kiwi_x)
            }

            // User Action
            if (DEV_Digital_Read(keyA)) {
                // show_statics() // TODO: make this function
                screen_updated = true;
            }
            if (DEV_Digital_Read(keyB)) {
                // confirmation_dialog() // TODO: make this function
                // burn_pine(id) // TODO: make this function
                screen_updated = true;
            }
            if (DEV_Digital_Read(keyX)) {
                kiwi_status = KiwiStatus::Eating;
                screen_updated = true;
            }
            if (DEV_Digital_Read(keyY)) {
                // water_pine() // TODO: make this function
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

    std::vector<Pinecone> pinecones;
    pinecones.emplace_back(false, 10);
    pinecones.emplace_back(false, 20);
    pinecones.emplace_back(false, 30);
    pinecones.emplace_back(false, 40);
    pinecones.emplace_back(false, 50);

    g_pinecones = &pinecones;

    LCD(pinecones); // core1での更新待つ？

    return 0;
}