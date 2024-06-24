#include "esp_system.h"
#include "esp_app_desc.h"

#include "common.h"
#include "config.h"
#include "i2s_parallel.h"
#include "gfx3d.h"
#include "cct.h"

#include "driver/ledc.h"


// #include "rectbmp.h"
// #include "lenabmp.h"

#include "ugui.h"

#include "driver/i2c_master.h"


#define TAG "main"

#ifdef DOUBLE_BUFFERED
int DrawBufID;
uint8_t FrameBuffer[2][FRAME_SIZE];
#else
uint8_t FrameBuffer[FRAME_SIZE];
#endif

#if 0
// from https://github.com/cnlohr/channel3
void SetupMatrix()
{
    tdIdentity(ProjectionMatrix);
    tdIdentity(ModelviewMatrix);
    Perspective(600, 250, 50, 8192, ProjectionMatrix);
}

// from https://github.com/cnlohr/channel3
void DrawSphere(int frameIndex)
{
    CNFGPenX = 0;
    CNFGPenY = 0;
    CNFGClearScreen(0);
    CNFGColor(1);
    tdIdentity(ModelviewMatrix);
    tdIdentity(ProjectionMatrix);
    int x = 0;
    int y = 0;
    CNFGDrawText("Matrix-based 3D engine", 3);
    SetupMatrix();
    tdRotateEA(ProjectionMatrix, -20, 0, 0);
    tdRotateEA(ModelviewMatrix, frameIndex, 0, 0);
    int sphereset = (frameIndex / 120);
    if (sphereset > 2)
        sphereset = 2;
    for (y = -sphereset; y <= sphereset; y++)
    {
        for (x = -sphereset; x <= sphereset; x++)
        {
            if (y == 2)
                continue;
            ModelviewMatrix[11] = 1000 + tdSIN((x + y) * 40 + frameIndex * 2);
            ModelviewMatrix[3] = 500 * x;
            ModelviewMatrix[7] = 500 * y + 800;
            DrawGeoSphere();
        }
    }
}
#endif
void lcd_config()
{
#ifdef DOUBLE_BUFFERED
    i2s_parallel_buffer_desc_t bufdesc[2] = {
        {.memory = &FrameBuffer[0], .size = sizeof(FrameBuffer[0])},
        {.memory = &FrameBuffer[1], .size = sizeof(FrameBuffer[1])},
    };
#else
    i2s_parallel_buffer_desc_t bufdesc = {
        .memory = &FrameBuffer, .size = sizeof(FrameBuffer)};
#endif

    i2s_parallel_config_t cfg = {
        .gpio_bus = {
            {.gpio = 19, .inverted = 0, .initial_value = 0}, // 0 : d0
            {.gpio = 4, .inverted = 0, .initial_value = 0},  // 1 : d1
            {.gpio = 2, .inverted = 0, .initial_value = 0},  // 2 : d2
            {.gpio = 15, .inverted = 0, .initial_value = 0}, // 3 : d3
            {.gpio = 17, .inverted = 0, .initial_value = 0}, // 4 : HS
            {.gpio = 16, .inverted = 0, .initial_value = 0}, // 5 : VS
            {.gpio = -1, .inverted = 0, .initial_value = 0}, // 6 : CLK_EN gate
           // {.gpio = -1, .inverted = 0, .initial_value = 0}, // 7 : unused
        },
        .num_gpio = 6,
        .gpio_clk = 5, // XCK

        .bits = I2S_PARALLEL_BITS_8,
        .clkspeed_hz = 2 * 1000 * 1500, // resulting pixel clock = 1.5MHz (~42fps)

#ifdef DOUBLE_BUFFERED
        .bufa = &bufdesc[0],
        .bufb = &bufdesc[1]
#else
        .buf = &bufdesc
#endif
    };

#ifdef DOUBLE_BUFFERED
    // make sure both front and back buffers have encoded syncs
    DrawBufID = 0;
    CNFGClearScreen(0);
    DrawBufID = 1;
#endif
    // make sure buffer has encoded syncs
    CNFGClearScreen(0);

    // this lcd power up sequence must be followed to avoid damage
    delayMs(50); // allow supply voltage to stabilize
    i2s_parallel_setup(&I2S1, &cfg);
    delayMs(50);
    // digitalWrite(PIN_BIAS_EN, 1); // enable lcd bias voltage V0 after clocks are available
    delayMs(50);
    // digitalWrite(PIN_DPY_EN, 1); // enable lcd
}

#define WIDTH NUM_COLS
#define HEIGHT NUM_ROWS
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
    // Setup UGUI
    UG_Init(&ugui, device);

    UG_FillScreen(C_WHITE);

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
#ifdef DOUBLE_BUFFERED
    uint8_t *pPkt = FrameBuffer[DrawBufID];
#else
    uint8_t *pPkt = FrameBuffer;
#endif
    uint32_t pos = (164 * (y + 1)) + 1 + (x >> 2);
    uint8_t pixel = x % 4;
    static const uint8_t pixel_masks[4] = {7, 0xb, 0xd, 0xe};
    static const uint8_t pixel_values[4] = {8, 4, 2, 1};
    uint8_t curr = pPkt[ESP32_CRAP_ALIGN(pos)] & pixel_masks[pixel];
    if (c != C_WHITE)
    {
        curr |= pixel_values[pixel];
    }
    pPkt[ESP32_CRAP_ALIGN(pos)] = curr;
}

void esp32_lcd_thing_flush(void)
{
    // ESP_LOGI(TAG, "flush");
    //  i2s_parallel_flip_to_buffer(&I2S1, DrawBufID);
    //  DrawBufID ^= 1;
}


#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (23) // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4096) // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY          (505) // Frequency in Hertz. Set frequency at 



i2c_master_bus_handle_t tool_bus_handle;




void lcdTask(void *pvParameters)
{
    lcd_config();

    gpio_set_direction(18, GPIO_MODE_OUTPUT);
    gpio_set_level(18, 1); // ENABLE

    // CNFGClearScreen(0);
    i2s_parallel_flip_to_buffer(&I2S1, DrawBufID);

    // Setup UGUI
    device.x_dim = 640;
    device.y_dim = 200;
    device.pset = &esp32_lcd_thing_pset;
    device.flush = &esp32_lcd_thing_flush;

    GUI_Setup(&device);


    i2c_device_config_t i2c_dev_conf = {
        .scl_speed_hz = 100 * 1000,
        .device_address = 0x20,
    };
    i2c_master_dev_handle_t dev_handle;
    if (i2c_master_bus_add_device(tool_bus_handle, &i2c_dev_conf, &dev_handle) != ESP_OK) {
        return;
    }

    uint8_t scan_matrix[] = { 0xfe, 0xfd, 0xfb };
    uint8_t pushed[3] = { 0xfe, 0xfd, 0xfb };
    uint8_t pb = 50;
    char buffer[10] = { '\0' };
    cct_SetMarker();

    while (1)
    {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        uint32_t elapsedUs = cct_ElapsedTimeUs();
        if (elapsedUs >= 10000)
        {
            cct_SetMarker();

            // Set duty to 50%
            //ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (2*13*pb)));
            // Update duty to apply the new value
            //ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

            // col0: 0xee, 0xde, 0xbe
            // col1: 0xbd, 0xed, 0xdd, 0xf5
            // col2: 0xf3, 0xeb, 0xdb, 0xbb

            uint8_t scan_result[3] = { 0, 0, 0 };
            for (int scan_idx = 0; scan_idx < 3; scan_idx++)
            {
                uint8_t data;
                data = scan_matrix[scan_idx];

                esp_err_t ret = i2c_master_transmit(dev_handle, &data, 1, 50);
                if (ret == ESP_OK) {
                    //ESP_LOGI(TAG, "Write OK");
                } else if (ret == ESP_ERR_TIMEOUT) {
                    ESP_LOGW(TAG, "Bus is busy");
                } else {
                    ESP_LOGW(TAG, "Write Failed");
                }

                ret = i2c_master_receive(dev_handle, &data, 1, 50);
                if (ret == ESP_OK) {
                    //ESP_LOGI(TAG, "Read: 0x%x", data);
                    scan_result[scan_idx] = data;
                } else if (ret == ESP_ERR_TIMEOUT) {
                    ESP_LOGW(TAG, "Bus is busy");
                } else {
                    ESP_LOGW(TAG, "Read failed");
                }
            }

            //ESP_LOGI(TAG, "Keyboard: 0x%.2x 0x%.2x 0x%.2x", scan_result[0], scan_result[1], scan_result[2]);

            // right
            if (scan_result[1] == 0xed)
            {
                if (pushed[1] != 0xed)
                {
                    pb += 5;
                    if (pb > 100) pb = 100;
                    UG_ProgressSetProgress(&wnd, PGB_ID_0, pb);
                    // Set duty to 50%
                    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (2*13*pb)));
                    // Update duty to apply the new value
                    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
                    snprintf(buffer, sizeof(buffer), "%d", pb);
                    UG_TextboxSetText(&wnd, TXB_ID_3, buffer);

                    ESP_LOGI(TAG, "Duty set to %d", pb);
                    pushed[1] = scan_result[1];
                }
            }
            // left
            else if (scan_result[1] == 0xdd)
            {
                if (pushed[1] != 0xdd)
                {
                    if (pb >= 5) pb -= 5;
                    UG_ProgressSetProgress(&wnd, PGB_ID_0, pb);
                    // Set duty to 50%
                    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (2*13*pb)));
                    // Update duty to apply the new value
                    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
                    snprintf(buffer, sizeof(buffer), "%d", pb);
                    UG_TextboxSetText(&wnd, TXB_ID_3, buffer);

                    ESP_LOGI(TAG, "Duty set to %d", pb);
                    pushed[1] = scan_result[1];
                }
            }
            else
            {
                pushed[1] = scan_result[1];
            }
        }

        GUI_Process();
    }

#if 0
    int counter = 0;
    int drawState = 0;

    // uint8_t *pImg = (counter / 100) & 1 ? (uint8_t *)lenaBitmap : (uint8_t *)rectBitmap;
    // CNFGLoadBitmap(lenaBitmap);
    // FrameBuffer[DrawBufID][ESP32_CRAP_ALIGN(163)] |= 1;
    // FrameBuffer[DrawBufID][ESP32_CRAP_ALIGN(165)] |= 1;
    // FrameBuffer[DrawBufID][168] |= 1;
    // DrawBufID ^= 1;
    // while(1) { delayMs(1000); };
    while (1)
    {

        switch (drawState)
        {
        case 0:
        default:
            if ((counter % 100) == 0)
            {
                cct_SetMarker();

                CNFGColor(1);
                CNFGPenX = rand() % 12;
                CNFGPenY = rand() % 60;
                char sztext[10];
                sprintf(sztext, "%02d:%02d%s", CNFGPenX, CNFGPenY, rand() & 1 ? "pm" : "am");
                CNFGDrawText(sztext, 3 + (rand() % 8));
                uint32_t elapsedUs = cct_ElapsedTimeUs();
                ESP_LOGI(TAG, "vector txt : %luus", elapsedUs);
#ifdef DOUBLE_BUFFERED
                i2s_parallel_flip_to_buffer(&I2S1, DrawBufID);
                DrawBufID ^= 1;
#endif
            }
            if (counter > 300)
            {
                drawState = 1;
                counter = -1;
            }
            break;

        case 1:
            if (counter == 0)
            {
                cct_SetMarker();
                CNFGClearScreen(0);
                CNFGColor(1);
                gfx_printSz(0, 0, "Lorem ipsum dolor sit amet, consectetur");
                gfx_printSz(8, 0, "adipiscing elit. Curabitur adipiscing ");
                gfx_printSz(16, 0, "ante sed nibh tincidunt feugiat.Maecenas");
                gfx_printSz(24, 0, "enim massa, fringilla sed malesuada et,");
                gfx_printSz(32, 0, "malesuada sit amet turpis.Sed porttitor");
                gfx_printSz(40, 0, "neque ut ante pretium vitae malesuada");
                gfx_printSz(48, 0, "nunc bibendum. Nullam aliquet ultrices");
                gfx_printSz(56, 0, "massa eu hendrerit. Ut sed nisi lorem.");
                gfx_printSz(64, 0, "In vestibulum purus a tortor imperdiet");
                gfx_printSz(72, 0, "posuere.");
                gfx_printSz(88, 0, "Cocus Was Here :D");

                gfx_printf(96, 0, "%s built on %s %s",
                           esp_app_get_description()->project_name,
                           esp_app_get_description()->date,
                           esp_app_get_description()->time);

                uint32_t elapsedUs = cct_ElapsedTimeUs();
                ESP_LOGI(TAG, "bitmap txt : %luus", elapsedUs);
#ifdef DOUBLE_BUFFERED
                i2s_parallel_flip_to_buffer(&I2S1, DrawBufID);
                DrawBufID ^= 1;
#endif
            }
            if (counter > 250)
            {
                drawState = 2;
                counter = -1;
            }
            break;

        case 2:
            if ((counter % 100) == 0)
            {
                cct_SetMarker();
                CNFGClearScreen(0);
                uint8_t *pImg = (counter / 100) & 1 ? (uint8_t *)lenaBitmap : (uint8_t *)rectBitmap;
                CNFGLoadBitmap(pImg, (NUM_COLS - 240) / 2, (NUM_ROWS - 160) / 2, 240, 160);
                uint32_t elapsedUs = cct_ElapsedTimeUs();
                ESP_LOGI(TAG, "Load bmp : %luus", elapsedUs);
#ifdef DOUBLE_BUFFERED
                i2s_parallel_flip_to_buffer(&I2S1, DrawBufID);
                DrawBufID ^= 1;
#endif
            }
            if (counter == 199)
            {
                drawState = 3;
                counter = -1;
                CNFGClearScreen(0);
            }
            break;

        case 3:
            cct_SetMarker();
            DrawSphere(counter % 240);
            // uint32_t elapsedUs = cct_ElapsedTimeUs();
            // ESP_LOGI(TAG,"Draw sph : %dus", elapsedUs);
#ifdef DOUBLE_BUFFERED
            i2s_parallel_flip_to_buffer(&I2S1, DrawBufID);
            DrawBufID ^= 1;
#endif
            break;
        }
        delayMs(20);
        counter++;
    }
#endif
}


/* Warning:
 * For ESP32, ESP32S2, ESP32S3, ESP32C3, ESP32C2, ESP32C6, ESP32H2, ESP32P4 targets,
 * when LEDC_DUTY_RES selects the maximum duty resolution (i.e. value equal to SOC_LEDC_TIMER_BIT_WIDTH),
 * 100% duty cycle is not reachable (duty cannot be set to (2 ** SOC_LEDC_TIMER_BIT_WIDTH)).
 */

static void example_ledc_init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

static gpio_num_t i2c_gpio_sda = 25;
static gpio_num_t i2c_gpio_scl = 26;

static i2c_port_t i2c_port = I2C_NUM_0;


void app_main()
{
    // pinMode(PIN_DPY_EN, OUTPUT);
    // digitalWrite(PIN_DPY_EN, 0);
    // pinMode(PIN_BIAS_EN, OUTPUT);
    // digitalWrite(PIN_BIAS_EN, 0);
    // pinMode(PIN_BTN, INPUT_PULLUP);

    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = i2c_port,
        .scl_io_num = i2c_gpio_scl,
        .sda_io_num = i2c_gpio_sda,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_new_master_bus(&i2c_bus_config, &tool_bus_handle);


    uint8_t address;
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    for (int i = 0; i < 128; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j++) {
            fflush(stdout);
            address = i + j;
            esp_err_t ret = i2c_master_probe(tool_bus_handle, address, 50);
            if (ret == ESP_OK) {
                printf("%02x ", address);
            } else if (ret == ESP_ERR_TIMEOUT) {
                printf("UU ");
            } else {
                printf("-- ");
            }
        }
        printf("\r\n");
    }


    example_ledc_init();

    // Set duty to 50%
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (2*13*50)));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

    xTaskCreatePinnedToCore(&lcdTask, "lcdTask", 4096, NULL, 20, NULL, 1);
}
