#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "string.h"
#include "speaker.h"

void app_main(void) {
    
    // 初始化音頻系統
    initialize_audio_system();

    // 設置播放文件
    set_audio("/spiffs/adf_music.mp3", 0);
    set_audio("/spiffs/song_pixel.mp3", 1);

    // 控制音量
    set_volume(0);

    // 播放
    play_audio(0);
    play_audio(1);
    play_audio(2);  

    
    while (1) {
        // 監測播放狀態
        handle_audio_events();

        printf("pipeline 1 state : %d\n", get_audio_state(0));
        printf("pipeline 2 state : %d\n", get_audio_state(1));
        printf("pipeline mix state : %d\n", get_audio_state(2));
        
        if(get_audio_state(1) == AEL_STATE_FINISHED || get_audio_state(1) == AEL_STATE_STOPPED)
        {
            stop_audio(2);
            stop_audio(1);
            play_audio(1);
            play_audio(2);
        }
    }

    // 停止並銷毀
    terminate_audio();
}
