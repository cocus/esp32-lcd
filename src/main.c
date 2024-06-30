#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"


#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "ugui.h"

#include "i2s_parallel.h"
#include "i2s_lcd4bit.h"

#include "driver/ledc.h"

#include "driver/i2c_master.h"


#define TAG "main"

typedef enum
{
    BTN_HELP = 1,
    BTN_BACKLIGHT = 2,
    BTN_UP = 3,
    BTN_DOWN = 4,
    BTN_RIGHT = 5,
    BTN_LEFT = 6,
    BTN_5 = 7,
    BTN_4 = 8,
    BTN_3 = 9,
    BTN_2 = 10,
    BTN_1 = 11,
} kb_btn_t;

static struct
{
    uint8_t bit_col_select;
    uint8_t bit_row;
    kb_btn_t btn;
    const char *friendly_name;
    bool state;
    int64_t last_ev_timestamp;
} _kb[] = {
    // col0: 0xee,     0xde,     0xbe
    // col0: help,     backlig,  up
    // col0: 11101110, 11011110, 10111110
    // sel0: 11111110, 11111110, 11111110

    // col1: 0xbd,     0xed,     0xdd,     0xf5
    // col1: down,     right,    left,     btn5
    // col1: 10111101, 11101101, 11011101, 11110101
    // sel1: 11111101, 11111101, 11111101, 11111101

    // col2: 0xf3,     0xeb,     0xdb,     0xbb
    // col2: btn4,     btn3,     btn2,     btn1
    // col2: 11110011, 11101011, 11011011, 10111011
    // sel2: 11111011, 11111011, 11111011, 11111011
    {0, 4, BTN_HELP, "HELP", false, 0},
    {0, 5, BTN_BACKLIGHT, "BACKLIGHT", false, 0},
    {0, 6, BTN_UP, "UP", false, 0},
    {1, 3, BTN_5, "5", false, 0},
    {1, 4, BTN_RIGHT, "RIGHT", false, 0},
    {1, 5, BTN_LEFT, "LEFT", false, 0},
    {1, 6, BTN_DOWN, "DOWN", false, 0},
    {2, 3, BTN_4, "4", false, 0},
    {2, 4, BTN_3, "3", false, 0},
    {2, 5, BTN_2, "2", false, 0},
    {2, 6, BTN_1, "1", false, 0}};

typedef struct
{
    kb_btn_t btn;
    bool pressed;
    int64_t timestamp;
} kb_event_t;

static QueueHandle_t _kb_queue = NULL;

static i2c_master_dev_handle_t _pcf8574_dev_handle;

void keyboardTask(void *)
{
    while (1)
    {
        uint8_t scan_result[3] = {0xff, 0xff, 0xff};
        uint8_t data;

        for (size_t scan_idx = 0; scan_idx < sizeof(scan_result) / sizeof(scan_result[0]); scan_idx++)
        {
            data = ~(1 << scan_idx);

            esp_err_t ret = i2c_master_transmit(_pcf8574_dev_handle, &data, 1, 50);
            if (ret == ESP_ERR_TIMEOUT)
            {
                ESP_LOGW(TAG, "Write: Bus is busy");
                continue;
            }
            else if (ret != ESP_OK)
            {
                ESP_LOGW(TAG, "Write: Failed");
                continue;
            }

            ret = i2c_master_receive(_pcf8574_dev_handle, &scan_result[scan_idx], 1, 50);
            if (ret == ESP_ERR_TIMEOUT)
            {
                ESP_LOGW(TAG, "Read: Bus is busy");
                continue;
            }
            else if (ret != ESP_OK)
            {
                ESP_LOGW(TAG, "Read: Failed");
                continue;
            }
        }

        for (size_t kb_idx = 0; kb_idx < sizeof(_kb) / sizeof(_kb[0]); kb_idx++)
        {
            uint8_t match_mask = 1 << _kb[kb_idx].bit_row;
            bool new_state = (match_mask & scan_result[_kb[kb_idx].bit_col_select]) == 0;

            if (_kb[kb_idx].state != new_state)
            {
                kb_event_t ev;
                ev.btn = _kb[kb_idx].btn;
                ev.pressed = new_state;
                ev.timestamp = esp_timer_get_time();
                _kb[kb_idx].last_ev_timestamp = ev.timestamp;
                _kb[kb_idx].state = new_state;
                xQueueSend(_kb_queue, (void *)&ev, (TickType_t)0);
            }
        }

        /* delay for keyboard scan rate */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t keyboardInit(void)
{
    const gpio_num_t i2c_gpio_sda = 25;
    const gpio_num_t i2c_gpio_scl = 26;
    const i2c_port_t i2c_port = I2C_NUM_0;

    /* register the I2C bus */
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = i2c_port,
        .scl_io_num = i2c_gpio_scl,
        .sda_io_num = i2c_gpio_sda,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_kb_bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_kb_bus_handle));

    /* register the PCF8574 device on the previously registered I2C bus */
    i2c_device_config_t i2c_dev_conf = {
        .scl_speed_hz = 100 * 1000,
        .device_address = 0x20,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_kb_bus_handle, &i2c_dev_conf, &_pcf8574_dev_handle));

    _kb_queue = xQueueCreate(50, sizeof(kb_event_t));

    xTaskCreatePinnedToCore(&keyboardTask, "kb", 4096, NULL, 20, NULL, 1);

    return ESP_OK;
}



#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO (23) // Define the output GPIO
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY (4096)                // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY (505)            // Frequency in Hertz. Set frequency at

static QueueHandle_t _pwm_queue = NULL;

void pwmTask(void*)
{
    uint8_t new_duty = 0;
    while(1)
    {
        if (xQueueReceive(_pwm_queue, (void *)&new_duty, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        if (new_duty > 100)
        {
            new_duty = 100;
        }

        // Set duty to 50%
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (2 * 13 * new_duty)));
        // Update duty to apply the new value
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

        ESP_LOGI(TAG, "Duty set to %d", new_duty);
    }
}

esp_err_t pwmInit(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY, // Set output frequency at 4 kHz
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEDC_OUTPUT_IO,
        .duty = 50, // Set duty to 50%
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    _pwm_queue = xQueueCreate(5, sizeof(uint8_t));

    xTaskCreatePinnedToCore(&pwmTask, "pwm", 4096, NULL, 20, NULL, 1);

    return ESP_OK;
}



#define MAX_OBJS 15

// Global Vars
UG_DEVICE device;
UG_GUI ugui;

UG_WINDOW wnd;
UG_BUTTON btn0;
UG_BUTTON btn1;
UG_BUTTON btn2;
UG_BUTTON btn3;
UG_CHECKBOX chb0;
UG_CHECKBOX chb1;
UG_CHECKBOX chb2;
UG_CHECKBOX chb3;
UG_TEXTBOX txt0;
UG_TEXTBOX txt1;
UG_TEXTBOX txt2;
UG_TEXTBOX txt3;
UG_PROGRESS pgb0;
UG_PROGRESS pgb1;
UG_OBJECT objs[MAX_OBJS];

#define INITIAL_MARGIN 3
#define BTN_WIDTH 100
#define BTN_HEIGHT 30
#define CHB_WIDTH 100
#define CHB_HEIGHT 14

#define OBJ_Y(i) BTN_HEIGHT *i + ((i + 1))

void windowHandler(UG_MESSAGE *msg)
{
    static UG_U16 x0, y0;

    (void)x0;
    (void)y0;

    // decode_msg(msg);

#if defined(UGUI_USE_TOUCH)
    if (msg->type == MSG_TYPE_OBJECT)
    {
        UG_OBJECT *obj = msg->src;
        if (obj)
        {
            if (obj->touch_state & OBJ_TOUCH_STATE_CHANGED)
                printf("|CHANGED");
            if (obj->touch_state & OBJ_TOUCH_STATE_PRESSED_ON_OBJECT)
                printf("|PRESSED_ON_OBJECT");
            if (obj->touch_state & OBJ_TOUCH_STATE_PRESSED_OUTSIDE_OBJECT)
                printf("|PRESSED_OUTSIDE_OBJECT");
            if (obj->touch_state & OBJ_TOUCH_STATE_RELEASED_ON_OBJECT)
                printf("|RELEASED_ON_OBJECT");
            if (obj->touch_state & OBJ_TOUCH_STATE_RELEASED_OUTSIDE_OBJECT)
                printf("|RELEASED_OUTSIDE_OBJECT");
            if (obj->touch_state & OBJ_TOUCH_STATE_IS_PRESSED_ON_OBJECT)
                printf("|IS_PRESSED_ON_OBJECT");
            if (obj->touch_state & OBJ_TOUCH_STATE_IS_PRESSED)
                printf("|IS_PRESSED");
            if (obj->touch_state & OBJ_TOUCH_STATE_INIT)
                printf("|INIT");
            printf("\n");
            if (obj->touch_state & OBJ_TOUCH_STATE_IS_PRESSED)
            {
                x0 = UG_GetGUI()->touch.xp;
                y0 = UG_GetGUI()->touch.yp;
            }
        }

        if (UG_ProgressGetProgress(&wnd, PGB_ID_0) == 100)
            UG_ProgressSetProgress(&wnd, PGB_ID_0, 0);
        else
            UG_ProgressSetProgress(&wnd, PGB_ID_0, UG_ProgressGetProgress(&wnd, PGB_ID_0) + 1);

        if (UG_ProgressGetProgress(&wnd, PGB_ID_1) == 100)
            UG_ProgressSetProgress(&wnd, PGB_ID_1, 0);
        else
            UG_ProgressSetProgress(&wnd, PGB_ID_1, UG_ProgressGetProgress(&wnd, PGB_ID_1) + 1);
    }
#endif
}
char buffer[64] = {'\0'};

void GUI_Setup(UG_DEVICE *device)
{
    // Setup Window
    UG_WindowCreate(&wnd, objs, MAX_OBJS, &windowHandler);
    wnd.xs = 120;
    UG_WindowSetTitleTextFont(&wnd, FONT_6X8);
    UG_WindowSetTitleText(&wnd, "App Title");

    // Buttons
    UG_ButtonCreate(&wnd, &btn0, BTN_ID_0, UGUI_POS(INITIAL_MARGIN, OBJ_Y(0), BTN_WIDTH, BTN_HEIGHT));
    UG_ButtonSetFont(&wnd, BTN_ID_0, FONT_6X8);
    UG_ButtonSetText(&wnd, BTN_ID_0, "Btn 3D");
    UG_ButtonSetStyle(&wnd, BTN_ID_0, BTN_STYLE_3D);

    UG_OBJECT *obj = _UG_SearchObject(&wnd, OBJ_TYPE_BUTTON, BTN_ID_0);
    UG_SetFocus(obj);

    UG_ButtonCreate(&wnd, &btn1, BTN_ID_1, UGUI_POS(INITIAL_MARGIN, OBJ_Y(1), BTN_WIDTH, BTN_HEIGHT));
    UG_ButtonSetFont(&wnd, BTN_ID_1, FONT_6X8);
    UG_ButtonSetText(&wnd, BTN_ID_1, "Btn 2D T");
    UG_ButtonSetStyle(&wnd, BTN_ID_1, BTN_STYLE_2D | BTN_STYLE_TOGGLE_COLORS);

    UG_ButtonCreate(&wnd, &btn2, BTN_ID_2, UGUI_POS(INITIAL_MARGIN, OBJ_Y(2), BTN_WIDTH, BTN_HEIGHT));
    UG_ButtonSetFont(&wnd, BTN_ID_2, FONT_6X8);
    UG_ButtonSetText(&wnd, BTN_ID_2, "Btn 3D Alt");
    UG_ButtonSetStyle(&wnd, BTN_ID_2, BTN_STYLE_3D | BTN_STYLE_USE_ALTERNATE_COLORS);
    UG_ButtonSetAlternateForeColor(&wnd, BTN_ID_2, C_BLACK);
    UG_ButtonSetAlternateBackColor(&wnd, BTN_ID_2, C_WHITE);

    UG_ButtonCreate(&wnd, &btn3, BTN_ID_3, UGUI_POS(INITIAL_MARGIN, OBJ_Y(3), BTN_WIDTH, BTN_HEIGHT));
    UG_ButtonSetFont(&wnd, BTN_ID_3, FONT_6X8);
    UG_ButtonSetText(&wnd, BTN_ID_3, "Btn NoB");
    UG_ButtonSetStyle(&wnd, BTN_ID_3, BTN_STYLE_NO_BORDERS | BTN_STYLE_TOGGLE_COLORS);

    // Checkboxes
    UG_CheckboxCreate(&wnd, &chb0, CHB_ID_0, UGUI_POS(INITIAL_MARGIN + 50 + BTN_WIDTH, OBJ_Y(0) + 7, CHB_WIDTH, CHB_HEIGHT));
    UG_CheckboxSetFont(&wnd, CHB_ID_0, FONT_6X8);
    UG_CheckboxSetText(&wnd, CHB_ID_0, "CHB");
    UG_CheckboxSetStyle(&wnd, CHB_ID_0, CHB_STYLE_3D);
    UG_CheckboxSetAlignment(&wnd, CHB_ID_0, ALIGN_TOP_LEFT);
#if !defined(UGUI_USE_COLOR_BW)
    UG_CheckboxSetBackColor(&wnd, CHB_ID_0, C_PALE_TURQUOISE);
#endif

    UG_CheckboxCreate(&wnd, &chb1, CHB_ID_1, UGUI_POS(INITIAL_MARGIN + 50 + BTN_WIDTH, OBJ_Y(1) + 7, CHB_WIDTH, CHB_HEIGHT));
    UG_CheckboxSetFont(&wnd, CHB_ID_1, FONT_6X8);
    UG_CheckboxSetText(&wnd, CHB_ID_1, "CHB");
    UG_CheckboxSetStyle(&wnd, CHB_ID_1, CHB_STYLE_2D | CHB_STYLE_TOGGLE_COLORS);
    UG_CheckboxSetAlignment(&wnd, CHB_ID_1, ALIGN_CENTER);
    UG_CheckboxShow(&wnd, CHB_ID_1);

    UG_CheckboxCreate(&wnd, &chb2, CHB_ID_2, UGUI_POS(INITIAL_MARGIN + 50 + BTN_WIDTH, OBJ_Y(2) + 7, CHB_WIDTH, CHB_HEIGHT));
    UG_CheckboxSetFont(&wnd, CHB_ID_2, FONT_6X8);
    UG_CheckboxSetText(&wnd, CHB_ID_2, "CHB");
    UG_CheckboxSetStyle(&wnd, CHB_ID_2, CHB_STYLE_3D | CHB_STYLE_USE_ALTERNATE_COLORS);
    UG_CheckboxSetAlignment(&wnd, CHB_ID_2, ALIGN_BOTTOM_LEFT);
    UG_CheckboxShow(&wnd, CHB_ID_2);

    UG_CheckboxCreate(&wnd, &chb3, CHB_ID_3, UGUI_POS(INITIAL_MARGIN + 50 + BTN_WIDTH, OBJ_Y(3) + 7, CHB_WIDTH, CHB_HEIGHT));
    UG_CheckboxSetFont(&wnd, CHB_ID_3, FONT_6X8);
    UG_CheckboxSetText(&wnd, CHB_ID_3, "CHB");
    UG_CheckboxSetStyle(&wnd, CHB_ID_3, CHB_STYLE_NO_BORDERS | CHB_STYLE_TOGGLE_COLORS);
    UG_CheckboxSetAlignment(&wnd, CHB_ID_3, ALIGN_BOTTOM_RIGHT);
    UG_CheckboxShow(&wnd, CHB_ID_3);

    // Texts
    UG_TextboxCreate(&wnd, &txt0, TXB_ID_0, UGUI_POS(INITIAL_MARGIN * 2 + BTN_WIDTH + CHB_WIDTH, OBJ_Y(0), 100, 15));
    UG_TextboxSetFont(&wnd, TXB_ID_0, FONT_4X6);
    UG_TextboxSetText(&wnd, TXB_ID_0, "Small TEXT");
#if !defined(UGUI_USE_COLOR_BW)
    UG_TextboxSetBackColor(&wnd, TXB_ID_0, C_PALE_TURQUOISE);
#endif

    UG_TextboxCreate(&wnd, &txt1, TXB_ID_1, UGUI_POS(INITIAL_MARGIN + 50 + BTN_WIDTH + CHB_WIDTH, OBJ_Y(1) - 15, 200, 30));
    UG_TextboxSetFont(&wnd, TXB_ID_1, FONT_12X20);
    snprintf(buffer, sizeof(buffer), "%s", esp_app_get_description()->date); //, esp_app_get_description()->time);
    ESP_LOGI(TAG, "buffer is '%s'", buffer);
    UG_TextboxSetText(&wnd, TXB_ID_1, buffer);
#if !defined(UGUI_USE_COLOR_BW)
    UG_TextboxSetBackColor(&wnd, TXB_ID_1, C_PALE_TURQUOISE);
#endif
    UG_TextboxSetAlignment(&wnd, TXB_ID_1, ALIGN_TOP_RIGHT);

    UG_TextboxCreate(&wnd, &txt2, TXB_ID_2, UGUI_POS(INITIAL_MARGIN * 2 + BTN_WIDTH + CHB_WIDTH, OBJ_Y(2) - 15, 100, 45));
    UG_TextboxSetFont(&wnd, TXB_ID_2, FONT_24X40);
    UG_TextboxSetText(&wnd, TXB_ID_2, "Text");
#if !defined(UGUI_USE_COLOR_BW)
    UG_TextboxSetBackColor(&wnd, TXB_ID_2, C_PALE_TURQUOISE);
#endif

    UG_TextboxCreate(&wnd, &txt3, TXB_ID_3, UGUI_POS(INITIAL_MARGIN * 2 + BTN_WIDTH + CHB_WIDTH, OBJ_Y(3), 100, 53));
    UG_TextboxSetFont(&wnd, TXB_ID_3, FONT_32X53);
    UG_TextboxSetText(&wnd, TXB_ID_3, "50");
#if !defined(UGUI_USE_COLOR_BW)
    UG_TextboxSetBackColor(&wnd, TXB_ID_3, C_PALE_TURQUOISE);
#endif

    // Progress Bar
    UG_ProgressCreate(&wnd, &pgb0, PGB_ID_0, UGUI_POS(INITIAL_MARGIN, OBJ_Y(4) + 20, 157, 20));
    UG_ProgressSetProgress(&wnd, PGB_ID_0, 50);

    UG_ProgressCreate(&wnd, &pgb1, PGB_ID_1, UGUI_POS(159 + INITIAL_MARGIN * 2, OBJ_Y(4) + 25, 156, 10));
    UG_ProgressSetStyle(&wnd, PGB_ID_1, PGB_STYLE_2D | PGB_STYLE_FORE_COLOR_MESH);
    UG_ProgressSetProgress(&wnd, PGB_ID_1, 75);

    UG_WindowShow(&wnd);
}

void GUI_Process()
{
    UG_Update();
}

// Internal
void esp32_lcd_thing_pset(UG_S16 x, UG_S16 y, UG_COLOR c)
{
    if (c != C_WHITE)
    {
        LCD_PixelSet(x, y, true);
    }
    else
    {
        LCD_PixelSet(x, y, false);
    }
}

void esp32_lcd_thing_flush(void)
{
    // ESP_LOGI(TAG, "flush");
    //  i2s_parallel_flip_to_buffer(&I2S1, DrawBufID);
    //  DrawBufID ^= 1;
}

void lcdTask(void *pvParameters)
{
    kb_event_t ev;
    char text_buffer[10] = {'\0'};
    uint8_t pb = 50;

    GUI_Setup(&device);

    while (1)
    {
        /* Delay 10ms */
        if (xQueueReceive(_kb_queue, (void *)&ev, pdMS_TO_TICKS(10)) != pdTRUE)
        {
            goto update_screen;
        }

        if (ev.pressed == false)
        {
            goto update_screen;
        }

        UG_OBJECT *obj = NULL;
        switch (ev.btn)
        {
        case BTN_RIGHT:
            pb += 5;
            if (pb > 100)
                pb = 100;
            UG_ProgressSetProgress(&wnd, PGB_ID_0, pb);
            // Set duty to 50%
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (2 * 13 * pb)));
            // Update duty to apply the new value
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
            snprintf(text_buffer, sizeof(text_buffer), "%d", pb);
            UG_TextboxSetText(&wnd, TXB_ID_3, text_buffer);
            xQueueSend(_pwm_queue, &pb, 0);

            break;

        case BTN_LEFT:
            if (pb >= 5)
                pb -= 5;
            UG_ProgressSetProgress(&wnd, PGB_ID_0, pb);
            // Set duty to 50%
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (2 * 13 * pb)));
            // Update duty to apply the new value
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
            snprintf(text_buffer, sizeof(text_buffer), "%d", pb);
            UG_TextboxSetText(&wnd, TXB_ID_3, text_buffer);
            xQueueSend(_pwm_queue, &pb, 0);

            break;

        case BTN_UP:
            obj = UG_FocusNext(&wnd);// _UG_SearchObject(&wnd, OBJ_TYPE_BUTTON, BTN_ID_0);
            ESP_LOGI(TAG, "set: up, obj %p", obj);
            break;

        case BTN_DOWN:
            obj = _UG_SearchObject(&wnd, OBJ_TYPE_BUTTON, BTN_ID_2);
            ESP_LOGI(TAG, "set: down, obj %p", obj);
            UG_SetFocus(obj);
            break;

        default:
            break;
        };

    update_screen:
        GUI_Process();
    }
}

esp_err_t lcdInit(void)
{
    i2s_parallel_buffer_desc_t bufdesc[2] = {
        {.memory = LCD_GetFrameBuffer(0), .size = FRAME_SIZE},
        {.memory = LCD_GetFrameBuffer(1), .size = FRAME_SIZE},
    };

    i2s_parallel_config_t cfg = {
        .gpio_bus = {
            [0] = {.gpio = 19, .inverted = 0}, // 0 : d0
            [1] = {.gpio = 4, .inverted = 0},  // 1 : d1
            [2] = {.gpio = 2, .inverted = 0},  // 2 : d2
            [3] = {.gpio = 15, .inverted = 0}, // 3 : d3
            [4] = {.gpio = 17, .inverted = 0}, // 4 : HS
            [5] = {.gpio = 16, .inverted = 0}, // 5 : VS
            [6] = {.gpio = -1, .inverted = 0}, // 6 : CLK_EN gate
        },
        .gpio_clk = 5, // XCK

        .bits = I2S_PARALLEL_BITS_8,
        .clkspeed_hz = 2 * 1000 * 1500, // resulting pixel clock = 1.5MHz (~42fps)

        .bufa = &bufdesc[0],
        .bufb = NULL
    };

    // make sure both front and back buffers have encoded syncs
    LCD_SelectFrameBuffer(0);
    LCD_ClearScreen(0);

    if (bufdesc[1].memory != NULL)
    {
        cfg.bufb = &bufdesc[1];

        LCD_SelectFrameBuffer(1);
        LCD_ClearScreen(0);
    }

    gpio_set_direction(18, GPIO_MODE_OUTPUT);
    gpio_set_level(18, 0); // ENABLE

    // this lcd power up sequence must be followed to avoid damage
    vTaskDelay(pdMS_TO_TICKS(50)); // allow supply voltage to stabilize
    i2s_parallel_setup(&I2S1, &cfg);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(18, 1); // ENABLE

    i2s_parallel_flip_to_buffer(&I2S1, LCD_GetFrameBufferSelected());

    // Setup UGUI
    device.x_dim = NUM_COLS;
    device.y_dim = NUM_ROWS;
    device.pset = &esp32_lcd_thing_pset;
    device.flush = &esp32_lcd_thing_flush;

    // Setup UGUI
    UG_Init(&ugui, &device);

    // clear background
    UG_FillScreen(C_WHITE);

    xTaskCreatePinnedToCore(&lcdTask, "lcdTask", 4096, NULL, 20, NULL, 1);

    return ESP_OK;
}

void app_main()
{
    pwmInit();
    keyboardInit();
    lcdInit();
}
