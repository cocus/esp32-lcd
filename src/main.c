#include "esp_system.h"
#include "esp_app_desc.h"

#include "common.h"
#include "config.h"
#include "i2s_parallel.h"
#include "gfx3d.h"
#include "cct.h"

#include "rectbmp.h"
#include "lenabmp.h"

#define TAG "main"

#ifdef DOUBLE_BUFFERED
int DrawBufID;
uint8_t FrameBuffer[2][FRAME_SIZE];
#else
uint8_t FrameBuffer[FRAME_SIZE];
#endif

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
            16, // 0 : d0
            4,  // 1 : d1
            2,  // 2 : d2
            15, // 3 : d3
            18, // 4 : HS
            19, // 5 : VS
            -1, // 6 : CLK_EN gate
            -1  // 7 : unused
        },
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
    DrawBufID = 1;
    CNFGClearScreen(0);
    DrawBufID = 0;
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

void lcdTask(void *pvParameters)
{
    int counter = 0;
    int drawState = 0;

    lcd_config();

    // CNFGClearScreen(0);
    i2s_parallel_flip_to_buffer(&I2S1, DrawBufID);
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
}

void app_main()
{
    // pinMode(PIN_DPY_EN, OUTPUT);
    // digitalWrite(PIN_DPY_EN, 0);
    // pinMode(PIN_BIAS_EN, OUTPUT);
    // digitalWrite(PIN_BIAS_EN, 0);
    // pinMode(PIN_BTN, INPUT_PULLUP);

    xTaskCreatePinnedToCore(&lcdTask, "lcdTask", 2048, NULL, 20, NULL, 1);
}
