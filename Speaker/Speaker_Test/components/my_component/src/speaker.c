#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "sdkconfig.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "esp_peripherals.h"
#include "periph_spiffs.h"
#include "spiffs_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "driver/i2s_std.h"
#include "audio_forge.h"
#include "raw_stream.h"

#include "speaker.h"

#define DEFAULT_SAMPLERATE 44100
#define DEFAULT_CHANNEL 1
#define DEST_SAMPLERATE 44100
#define DEST_CHANNEL 1
#define TRANSMITTIME 0
#define MUSIC_GAIN_DB 0
#define NUMBER_SOURCE_FILE 2

typedef enum {
    PIPELINE_FIRST = 0,
    PIPELINE_SECOND,
    PIPELINE_MIX
} pipeline_index_t;

/* 
    audio_pipeline_handler 

    pre_pipeline_1 --------- \
                               -------- mix_pipeline
    pre_pipeline_2 --------- /

*/
static audio_pipeline_handle_t pre_pipeline[NUMBER_SOURCE_FILE];
static audio_pipeline_handle_t mix_pipeline;

// audio_elements_handler 控制每個個別的音訊處理組成
// 含有 spiffs/mp3_decoder/i2s_stream_writer
static audio_element_handle_t spiffs_stream_reader[NUMBER_SOURCE_FILE];
static audio_element_handle_t audio_decoder[NUMBER_SOURCE_FILE];
static audio_element_handle_t raw_stream_writer[NUMBER_SOURCE_FILE];
static audio_element_handle_t audio_forge;
static audio_element_handle_t i2s_stream_writer;

// esp_peripheral_handler，在此處理 spiffs
static esp_periph_set_handle_t set;

// audio_event_handle 處理音訊播放
static audio_event_iface_handle_t evt;

// audio_event_message 處理音訊狀態
static audio_event_iface_msg_t msg;

int audio_forge_wr_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx) {
    audio_element_handle_t i2s_wr = (audio_element_handle_t)ctx;
    int ret = audio_element_output(i2s_wr, buf, len);
    return ret;
}

// initialize_audio_system
void initialize_audio_system() {

    // initialize esp_peripheral
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    set = esp_periph_set_init(&periph_cfg);

    periph_spiffs_cfg_t spiffs_cfg = {
        .root = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_periph_handle_t spiffs_handle = periph_spiffs_init(&spiffs_cfg);

    // start spiffs
    esp_periph_start(set, spiffs_handle);

    // Wait until spiffs is mounted
    while (!periph_spiffs_is_mounted(spiffs_handle)) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    // Initialize pipeline
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();

    // Initialize spiffs_stream
    spiffs_stream_cfg_t flash_cfg = SPIFFS_STREAM_CFG_DEFAULT();
    flash_cfg.type = AUDIO_STREAM_READER;
    
    // Initialize mp3_decoder
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();

    // Initialize raw_stream
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;

    // Initialize i2s_stream
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.use_alc = true;
    i2s_cfg.task_stack = 0;
    i2s_cfg.out_rb_size = 0;
    i2s_cfg.chan_cfg.auto_clear = true;
    // Set default audio volume to 0
    i2s_cfg.volume = 0;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);
    i2s_stream_set_clk(i2s_stream_writer, DEST_SAMPLERATE, 16, DEST_CHANNEL);

    // Initialize audio forge
    audio_forge_cfg_t audio_forge_cfg = AUDIO_FORGE_CFG_DEFAULT();
    audio_forge_cfg.audio_forge.component_select = AUDIO_FORGE_SELECT_RESAMPLE | AUDIO_FORGE_SELECT_DOWNMIX | AUDIO_FORGE_SELECT_ALC | AUDIO_FORGE_SELECT_EQUALIZER | AUDIO_FORGE_SELECT_SONIC;
    audio_forge_cfg.audio_forge.dest_samplerate = DEST_SAMPLERATE;
    audio_forge_cfg.audio_forge.dest_channel = DEST_CHANNEL;
    audio_forge_cfg.audio_forge.source_num = NUMBER_SOURCE_FILE;
    audio_forge = audio_forge_init(&audio_forge_cfg);
    audio_forge_src_info_t source_information = {
        .samplerate = DEFAULT_SAMPLERATE,
        .channel = DEFAULT_CHANNEL,
        .bit_num = 16,
    };
    audio_forge_downmix_t downmix_information = {
        .gain = {0, MUSIC_GAIN_DB},
        .transit_time = TRANSMITTIME,
    };
    audio_forge_src_info_t source_info[NUMBER_SOURCE_FILE] = {0};
    audio_forge_downmix_t downmix_info[NUMBER_SOURCE_FILE];
    for (int i = 0; i < NUMBER_SOURCE_FILE; i++) {
        source_info[i] = source_information;
        downmix_info[i] = downmix_information;
    }
    audio_forge_source_info_init(audio_forge, source_info, downmix_info);

    // Initialize audio listning system
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);

    // Regist audio elements and link pipeline
    for(int i = 0 ; i < NUMBER_SOURCE_FILE ; i++) {
        pre_pipeline[i] = audio_pipeline_init(&pipeline_cfg);
        mem_assert(pre_pipeline[i]);

        spiffs_stream_reader[i] = spiffs_stream_init(&flash_cfg);
        audio_decoder[i] = mp3_decoder_init(&mp3_cfg);
        raw_stream_writer[i] = raw_stream_init(&raw_cfg);

        audio_pipeline_register(pre_pipeline[i], spiffs_stream_reader[i], "file");
        audio_pipeline_register(pre_pipeline[i], audio_decoder[i], "mp3");
        audio_pipeline_register(pre_pipeline[i], raw_stream_writer[i], "raw");
        const char *link_tag[3] = {"file", "mp3", "raw"};
        audio_pipeline_link(pre_pipeline[i], &link_tag[0], 3);
        ringbuf_handle_t rb = audio_element_get_input_ringbuf(raw_stream_writer[i]);
        if (NUMBER_SOURCE_FILE != 1) {
            audio_element_set_multi_input_ringbuf(audio_forge, rb, i);
        } else {
            audio_element_set_input_ringbuf(audio_forge, rb);
        }
        audio_pipeline_set_listener(pre_pipeline[i], evt);
    }

    mix_pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(mix_pipeline);

    audio_pipeline_register(mix_pipeline, audio_forge, "audio_forge");
    audio_element_set_write_cb(audio_forge, audio_forge_wr_cb, i2s_stream_writer);
    audio_element_process_init(i2s_stream_writer);

    audio_pipeline_link(mix_pipeline, (const char *[]) {"audio_forge"}, 1);

    audio_pipeline_set_listener(mix_pipeline, evt);
}

// 設置 mp3 文件，選擇音檔
void set_audio(const char *file_path, int pipeline_index) {
    printf("Success : set_audio()\n");
    audio_element_set_uri(spiffs_stream_reader[pipeline_index], file_path);
}

// 播放 mp3
void play_audio(int pipeline_index) {
    printf("Success : play_audio()\n");
    if(pipeline_index == PIPELINE_MIX) {
        audio_pipeline_run(mix_pipeline);
    }
    else {
        audio_pipeline_run(pre_pipeline[pipeline_index]);
    }
}

// 處理音訊播放
void handle_audio_events()
{
    // Read message (wait 100 ms)
    esp_err_t ret = audio_event_iface_listen(evt, &msg, 1000/portTICK_PERIOD_MS);

    printf("%d\n", ret);

    if(get_audio_state(2) == AEL_STATE_INIT) {
        for(int i = 0 ; i < 2 ; i++) ret = audio_pipeline_run(pre_pipeline[i]);
        audio_pipeline_run(mix_pipeline);
    }

    // If fail to read, continue
    if (ret != ESP_OK) {
        return;
    }

    // Modify audio data
    for (int i = 0; i < NUMBER_SOURCE_FILE; i++) {
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)audio_decoder[i]
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            printf("Audio info is modified :\n");
            audio_element_info_t music_info = {0};
            audio_element_getinfo(audio_decoder[i], &music_info);
            audio_forge_src_info_t src_info = {
                .samplerate = music_info.sample_rates,
                .channel = music_info.channels,
                .bit_num = music_info.bits,
            };
            audio_forge_set_src_info(audio_forge, src_info, i);
            printf("%d audio sample_rates = %d, bit depth = %d\n", i+1, music_info.sample_rates, music_info.bits);
        }
    }

    // When finish playing, stop the pipeline and reset, wait for the next run
    if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) audio_decoder[PIPELINE_FIRST] && msg.cmd == AEL_MSG_CMD_REPORT_STATUS &&
        ((int)msg.data == AEL_STATUS_STATE_FINISHED)) {
        printf("Audio 1 is finished\n");
        audio_pipeline_stop(pre_pipeline[PIPELINE_FIRST]);
        audio_pipeline_wait_for_stop(pre_pipeline[PIPELINE_FIRST]);
        audio_pipeline_terminate(pre_pipeline[PIPELINE_FIRST]);
        audio_pipeline_reset_ringbuffer(pre_pipeline[PIPELINE_FIRST]);
        audio_pipeline_reset_elements(pre_pipeline[PIPELINE_FIRST]);
        audio_pipeline_change_state(pre_pipeline[PIPELINE_FIRST], AEL_STATE_INIT);
    }

    if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) audio_decoder[PIPELINE_SECOND] && msg.cmd == AEL_MSG_CMD_REPORT_STATUS &&
        ((int)msg.data == AEL_STATUS_STATE_FINISHED)) {
        printf("Audio 2 is finished\n");
        audio_pipeline_stop(pre_pipeline[PIPELINE_SECOND]);
        audio_pipeline_wait_for_stop(pre_pipeline[PIPELINE_SECOND]);
        audio_pipeline_terminate(pre_pipeline[PIPELINE_SECOND]);
        audio_pipeline_reset_ringbuffer(pre_pipeline[PIPELINE_SECOND]);
        audio_pipeline_reset_elements(pre_pipeline[PIPELINE_SECOND]);
        audio_pipeline_change_state(pre_pipeline[PIPELINE_SECOND], AEL_STATE_INIT);
    }
}

// 獲取音檔資訊
int get_audio_state(int pipeline_index)
{
    // return value comes from the data of i2s_stream_writer
    if(pipeline_index == PIPELINE_MIX) {
        return audio_element_get_state(audio_forge);
    }
    else {
        return audio_element_get_state(audio_decoder[pipeline_index]);
    }
    
}

// 暫停播放
void pause_audio(int pipeline_index) {
    if(get_audio_state(pipeline_index) == AEL_STATE_RUNNING) {
        printf("Success : pause_audio()\n");
        if(pipeline_index == PIPELINE_MIX) {
            audio_pipeline_pause(pre_pipeline[PIPELINE_FIRST]);
            audio_pipeline_pause(pre_pipeline[PIPELINE_SECOND]);
            audio_pipeline_pause(pre_pipeline[PIPELINE_MIX]);
        }
        else {
            audio_pipeline_pause(pre_pipeline[pipeline_index]);
        }
    }
    else {
        printf("Error : pause_audio() didn't work\n");
    }
}

// 恢復播放
void resume_audio(int pipeline_index) {
    if(get_audio_state(pipeline_index) == AEL_STATE_PAUSED) {
        printf("Success : resume_audio()\n");
        if(pipeline_index == PIPELINE_MIX) {
            audio_pipeline_resume(pre_pipeline[PIPELINE_FIRST]);
            audio_pipeline_resume(pre_pipeline[PIPELINE_SECOND]);
            audio_pipeline_resume(pre_pipeline[PIPELINE_MIX]);
        }
        else {
            audio_pipeline_resume(pre_pipeline[pipeline_index]);
        }
    }
    else {
        printf("Error : resume_audio() didn't work\n");
    }
}

// 停止播放
void stop_audio(int pipeline_index) {
    if(get_audio_state(pipeline_index) == AEL_STATE_RUNNING || get_audio_state(pipeline_index) == AEL_STATE_PAUSED) {
        printf("Success : stop_audio()\n");
        if(pipeline_index == PIPELINE_MIX) {
            audio_pipeline_stop(mix_pipeline);
            audio_pipeline_wait_for_stop(mix_pipeline);
            audio_pipeline_terminate(mix_pipeline);
            audio_pipeline_reset_ringbuffer(mix_pipeline);
            audio_pipeline_reset_elements(mix_pipeline);
            audio_pipeline_change_state(mix_pipeline, AEL_STATE_INIT);

            for(int i = 0 ; i < NUMBER_SOURCE_FILE ; i++) {
                audio_pipeline_stop(pre_pipeline[i]);
                audio_pipeline_wait_for_stop(pre_pipeline[i]);
                audio_pipeline_terminate(pre_pipeline[i]);
                audio_pipeline_reset_ringbuffer(pre_pipeline[i]);
                audio_pipeline_reset_elements(pre_pipeline[i]);
                audio_pipeline_change_state(pre_pipeline[i], AEL_STATE_INIT);
            }
        }
        else {
            audio_pipeline_stop(pre_pipeline[pipeline_index]);
            audio_pipeline_wait_for_stop(pre_pipeline[pipeline_index]);
            audio_pipeline_terminate(pre_pipeline[pipeline_index]);
            audio_pipeline_reset_ringbuffer(pre_pipeline[pipeline_index]);
            audio_pipeline_reset_elements(pre_pipeline[pipeline_index]);

            audio_pipeline_change_state(pre_pipeline[pipeline_index], AEL_STATE_INIT);
        }
        
    }
    else {
        printf("Error : stop_audio() didn't work\n");
    }
}

// 停止播放並銷毀
void terminate_audio() {
    for (int i = 0; i < NUMBER_SOURCE_FILE; i++) {
        audio_pipeline_stop(pre_pipeline[i]);
        audio_pipeline_wait_for_stop(pre_pipeline[i]);
        audio_pipeline_terminate(pre_pipeline[i]);
        audio_pipeline_unregister_more(pre_pipeline[i], spiffs_stream_reader[i], audio_decoder[i], raw_stream_writer[i], NULL);
        audio_pipeline_remove_listener(pre_pipeline[i]);
    }
    audio_pipeline_stop(mix_pipeline);
    audio_pipeline_wait_for_stop(mix_pipeline);
    audio_pipeline_terminate(mix_pipeline);
    audio_pipeline_unregister_more(mix_pipeline, audio_forge, spiffs_stream_reader, NULL);
    audio_pipeline_remove_listener(mix_pipeline);

    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);
    audio_event_iface_destroy(evt);

    for (int i = 0; i < NUMBER_SOURCE_FILE; i++) {
        /* Release resources */
        audio_pipeline_deinit(pre_pipeline[i]);
        audio_element_deinit(spiffs_stream_reader[i]);
        audio_element_deinit(audio_decoder[i]);
        audio_element_deinit(raw_stream_writer[i]);
    }
    /* Release resources */
    audio_pipeline_deinit(mix_pipeline);
    audio_element_deinit(audio_forge);
    audio_element_deinit(i2s_stream_writer);

    esp_periph_set_destroy(set);
}

// 設置音量大小
void set_volume(int volume)
{
    i2s_alc_volume_set(i2s_stream_writer, volume);
}

