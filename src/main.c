#include "esp_system.h"
#include "esp_app_desc.h"

#include "common.h"
#include "config.h"
#include "i2s_parallel.h"
#include "gfx3d.h"
#include "cct.h"

// #include "rectbmp.h"
// #include "lenabmp.h"

#include "ugui.h"

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
            {.gpio = 16, .inverted = 0, .initial_value = 0}, // 0 : d0
            {.gpio = 4, .inverted = 0, .initial_value = 0},  // 1 : d1
            {.gpio = 2, .inverted = 0, .initial_value = 0},  // 2 : d2
            {.gpio = 15, .inverted = 0, .initial_value = 0}, // 3 : d3
            {.gpio = 18, .inverted = 0, .initial_value = 0}, // 4 : HS
            {.gpio = 19, .inverted = 0, .initial_value = 0}, // 5 : VS
            {.gpio = -1, .inverted = 0, .initial_value = 0}, // 6 : CLK_EN gate
            {.gpio = -1, .inverted = 0, .initial_value = 0}, // 7 : unused
        },
        .num_gpio = 6,
        .gpio_clk = 17, // XCK

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

#define INITIAL_MARGIN 120
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
    UG_WindowSetTitleTextFont(&wnd, FONT_6X8);
    UG_WindowSetTitleText(&wnd, "App Title mas largo la concha de estos pelotudos que cortaron la LCD");

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
    UG_TextboxSetText(&wnd, TXB_ID_3, "ABC");
#if !defined(UGUI_USE_COLOR_BW)
    UG_TextboxSetBackColor(&wnd, TXB_ID_3, C_PALE_TURQUOISE);
#endif

    // Progress Bar
    UG_ProgressCreate(&wnd, &pgb0, PGB_ID_0, UGUI_POS(INITIAL_MARGIN, OBJ_Y(4) + 20, 157, 20));
    UG_ProgressSetProgress(&wnd, PGB_ID_0, 0);

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

void lcdTask(void *pvParameters)
{
    lcd_config();

    // CNFGClearScreen(0);
    i2s_parallel_flip_to_buffer(&I2S1, DrawBufID);

    // Setup UGUI
    device.x_dim = 640;
    device.y_dim = 200;
    device.pset = &esp32_lcd_thing_pset;
    device.flush = &esp32_lcd_thing_flush;

    GUI_Setup(&device);

    uint8_t pb = 0;
    cct_SetMarker();

    while (1)
    {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        uint32_t elapsedUs = cct_ElapsedTimeUs();
        if (elapsedUs >= 1000)
        {
            if (++pb > 100)
            {
                pb = 0;
            }
            UG_ProgressSetProgress(&wnd, PGB_ID_0, pb);
            cct_SetMarker();
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

void app_main()
{
    // pinMode(PIN_DPY_EN, OUTPUT);
    // digitalWrite(PIN_DPY_EN, 0);
    // pinMode(PIN_BIAS_EN, OUTPUT);
    // digitalWrite(PIN_BIAS_EN, 0);
    // pinMode(PIN_BTN, INPUT_PULLUP);

    xTaskCreatePinnedToCore(&lcdTask, "lcdTask", 4096, NULL, 20, NULL, 1);
}
