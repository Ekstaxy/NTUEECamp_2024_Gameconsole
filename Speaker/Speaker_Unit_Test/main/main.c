// 標頭檔
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "string.h"
#include "speaker.h"
#include "st7789.h"
#include "fontx.h"
#include "pngle.h"
#include "decode_png.h"

void app_main(void) {

    // 螢幕相關設定
    TFT_t TFT_t;
    spi_clock_speed(60000000);
    spi_master_init(&TFT_t, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO, CONFIG_BL_GPIO);
    lcdInit(&TFT_t, CONFIG_WIDTH, CONFIG_HEIGHT, CONFIG_OFFSETX, CONFIG_OFFSETY);
    lcdFillScreen(&TFT_t, WHITE);
    lcdDrawFinish(&TFT_t);

    // 圖片檔案
    char *files[] ={"/spiffs/frame-01.png","/spiffs/frame-02.png","/spiffs/frame-03.png",
    "/spiffs/frame-04.png","/spiffs/frame-05.png","/spiffs/frame-06.png","/spiffs/frame-07.png",
    "/spiffs/frame-08.png","/spiffs/frame-09.png","/spiffs/frame-10.png","/spiffs/frame-11.png",
    "/spiffs/frame-12.png","/spiffs/frame-13.png","/spiffs/frame-14.png","/spiffs/frame-15.png",
    "/spiffs/frame-16.png","/spiffs/frame-17.png","/spiffs/frame-18.png","/spiffs/frame-19.png",
    "/spiffs/frame-20.png","/spiffs/frame-21.png","/spiffs/frame-22.png","/spiffs/frame-23.png",
    "/spiffs/frame-24.png","/spiffs/frame-25.png","/spiffs/frame-26.png","/spiffs/frame-27.png",
    };
    
    // 初始化音頻系統
    initialize_audio_system();

    // 設置播放文件
    set_audio("/spiffs/sample_music.mp3");

    // 播放
    play_audio();

    // 控制音量
    set_volume(-20);

    while (1) {
        for (int i = 0; i < 27; i++) {

            // 圖片相關函式
            lcdShowPNG(&TFT_t, 20, 94, files[i], 200, 113);
            lcdDrawFinish(&TFT_t);
            handle_audio_events();

            // TO-DO
            /*
                此次的實作為音檔播放完畢時的重啟
                可用的 FUNCTION 有以下：
                set_audio();
                play_audio();
                stop_audio();
                pause_audio();
                resume_audio();
            */
            if(get_audio_state() == AEL_STATE_FINISHED) {

                // 中止播放
                stop_audio();

                // 重新設定播放檔案
                set_audio("/spiffs/sample_music.mp3");

                // 播放檔案
                play_audio();

                break;
            }
            // TO-DO END
        }
    }

    // 停止並銷毀
    terminate_audio();
}
