
#include <time.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "tftspi.h"
#include "tft.h"
#include "spiffs_vfs.h"
#include "tft_ctl.h"




// ==========================================================
// Define which spi bus to use TFT_VSPI_HOST or TFT_HSPI_HOST
#define SPI_BUS TFT_HSPI_HOST
// ==========================================================


static int _demo_pass = 0;
static uint8_t doprint = 1;
static uint8_t run_gs_demo = 0; // Run gray scale demo if set to 1
static struct tm* tm_info;
static char tmp_buff[64];
static time_t time_now, time_last = 0;
static const char *file_fonts[3] = {"/spiffs/fonts/DotMatrix_M.fon", "/spiffs/fonts/Ubuntu.fon", "/spiffs/fonts/Grotesk24x48.fon"};

#define GDEMO_TIME 1000
#define GDEMO_INFO_TIME 5000


//----------------------
static void _checkTime()
{
	time(&time_now);
	if (time_now > time_last) {
		color_t last_fg, last_bg;
		time_last = time_now;
		tm_info = localtime(&time_now);
		sprintf(tmp_buff, "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

		TFT_saveClipWin();
		TFT_resetclipwin();

		Font curr_font = cfont;
		last_bg = _bg;
		last_fg = _fg;
		_fg = TFT_YELLOW;
		_bg = (color_t){ 64, 64, 64 };
		TFT_setFont(DEFAULT_FONT, NULL);

		TFT_fillRect(1, _height-TFT_getfontheight()-8, _width-3, TFT_getfontheight()+6, _bg);
		TFT_print(tmp_buff, CENTER, _height-TFT_getfontheight()-5);

		cfont = curr_font;
		_fg = last_fg;
		_bg = last_bg;

		TFT_restoreClipWin();
	}
}

/*
//----------------------
static int _checkTouch()
{
	int tx, ty;
	if (TFT_read_touch(&tx, &ty, 0)) {
		while (TFT_read_touch(&tx, &ty, 1)) {
			vTaskDelay(20 / portTICK_RATE_MS);
		}
		return 1;
	}
	return 0;
}
*/

//---------------------
static int Wait(int ms)
{
	uint8_t tm = 1;
	if (ms < 0) {
		tm = 0;
		ms *= -1;
	}
	if (ms <= 50) {
		vTaskDelay(ms / portTICK_RATE_MS);
		//if (_checkTouch()) return 0;
	}
	else {
		for (int n=0; n<ms; n += 50) {
			vTaskDelay(50 / portTICK_RATE_MS);
			//if (tm) _checkTime();
			//if (_checkTouch()) return 0;
		}
	}
	return 1;
}

//-------------------------------------------------------------------
static unsigned int rand_interval(unsigned int min, unsigned int max)
{
    int r;
    const unsigned int range = 1 + max - min;
    const unsigned int buckets = RAND_MAX / range;
    const unsigned int limit = buckets * range;

    /* Create equal size buckets all in a row, then fire randomly towards
     * the buckets until you land in one of them. All buckets are equally
     * likely. If you land off the end of the line of buckets, try again. */
    do
    {
        r = rand();
    } while (r >= limit);

    return min + (r / buckets);
}

// Generate random color
//-----------------------------
static color_t random_color() {

	color_t color;
	color.r  = (uint8_t)rand_interval(8,252);
	color.g  = (uint8_t)rand_interval(8,252);
	color.b  = (uint8_t)rand_interval(8,252);
	return color;
}

//---------------------
static void _dispTime()
{
	Font curr_font = cfont;
    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);

    time(&time_now);
	time_last = time_now;
	tm_info = localtime(&time_now);
	sprintf(tmp_buff, "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
	TFT_print(tmp_buff, CENTER, _height-TFT_getfontheight()-5);

    cfont = curr_font;
}

//---------------------------------
static void disp_header(char *info)
{
	TFT_fillScreen(TFT_BLACK);
	TFT_resetclipwin();

	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };

    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);
	TFT_fillRect(0, 0, _width-1, TFT_getfontheight()+8, _bg);
	TFT_drawRect(0, 0, _width-1, TFT_getfontheight()+8, TFT_CYAN);

	TFT_fillRect(0, _height-TFT_getfontheight()-9, _width-1, TFT_getfontheight()+8, _bg);
	TFT_drawRect(0, _height-TFT_getfontheight()-9, _width-1, TFT_getfontheight()+8, TFT_CYAN);

	TFT_print(info, CENTER, 4);
	_dispTime();

	_bg = TFT_BLACK;
	TFT_setclipwin(0,TFT_getfontheight()+9, _width-1, _height-TFT_getfontheight()-10);
}

//---------------------------------------------
static void update_header(char *hdr, char *ftr)
{
	color_t last_fg, last_bg;

	TFT_saveClipWin();
	TFT_resetclipwin();

	Font curr_font = cfont;
	last_bg = _bg;
	last_fg = _fg;
	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };
    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);

	if (hdr) {
		TFT_fillRect(1, 1, _width-3, TFT_getfontheight()+6, _bg);
		TFT_print(hdr, CENTER, 4);
	}

	if (ftr) {
		TFT_fillRect(1, _height-TFT_getfontheight()-8, _width-3, TFT_getfontheight()+6, _bg);
		if (strlen(ftr) == 0) _dispTime();
		else TFT_print(ftr, CENTER, _height-TFT_getfontheight()-5);
	}

	cfont = curr_font;
	_fg = last_fg;
	_bg = last_bg;

	TFT_restoreClipWin();
}

//------------------------
static void test_times() {

	if (doprint) {
	    uint32_t tstart, t1, t2;
		disp_header("TIMINGS");
		// ** Show Fill screen and send_line timings
		tstart = clock();
		TFT_fillWindow(TFT_BLACK);
		t1 = clock() - tstart;
		printf("     Clear screen time: %u ms\r\n", t1);
		TFT_setFont(SMALL_FONT, NULL);
		sprintf(tmp_buff, "Clear screen: %u ms", t1);
		TFT_print(tmp_buff, 0, 140);

		color_t *color_line = heap_caps_malloc((_width*3), MALLOC_CAP_DMA);
		color_t *gsline = NULL;
		if (gray_scale) gsline = malloc(_width*3);
		if (color_line) {
			float hue_inc = (float)((10.0 / (float)(_height-1) * 360.0));
			for (int x=0; x<_width; x++) {
				color_line[x] = HSBtoRGB(hue_inc, 1.0, (float)x / (float)_width);
				if (gsline) gsline[x] = color_line[x];
			}
			disp_select();
			tstart = clock();
			for (int n=0; n<1000; n++) {
				if (gsline) memcpy(color_line, gsline, _width*3);
				send_data(0, 40+(n&63), dispWin.x2-dispWin.x1, 40+(n&63), (uint32_t)(dispWin.x2-dispWin.x1+1), color_line);
				wait_trans_finish(1);
			}
			t2 = clock() - tstart;
			disp_deselect();

			printf("Send color buffer time: %u us (%d pixels)\r\n", t2, dispWin.x2-dispWin.x1+1);
			free(color_line);

			sprintf(tmp_buff, "   Send line: %u us", t2);
			TFT_print(tmp_buff, 0, 144+TFT_getfontheight());
		}
		Wait(GDEMO_INFO_TIME);
    }
}

// Image demo
//-------------------------
static void disp_images() 
{
    uint32_t tstart;

	if (spiffs_is_mounted) {
		// ** Show scaled (1/8, 1/4, 1/2 size) JPG images
		/*TFT_jpg_image(CENTER, CENTER, 3, SPIFFS_BASE_PATH"/images/test1.jpg", NULL, 0);
		Wait(500);

		TFT_jpg_image(CENTER, CENTER, 2, SPIFFS_BASE_PATH"/images/test2.jpg", NULL, 0);
		Wait(500);

		TFT_jpg_image(CENTER, CENTER, 1, SPIFFS_BASE_PATH"/images/test4.jpg", NULL, 0);
		Wait(500);*/

		// ** Show full size JPG image
		TFT_jpg_image(CENTER, CENTER, 0, SPIFFS_BASE_PATH"/images/sun4.jpg", NULL, 0);
		font_transparent = 1; //无背景
		TFT_setFont(UBUNTU16_FONT, NULL);
		_fg = TFT_CYAN;
		TFT_print("ShiTong Technology", CENTER, CENTER);
		Wait(3000);
		//TFT_jpg_image(0, 50, 0, SPIFFS_BASE_PATH"/images/t1.jpg", NULL, 0);
		//Wait(3000);

		/*TFT_jpg_image(CENTER, CENTER, 0, SPIFFS_BASE_PATH"/images/sun2.jpg", NULL, 0);
		Wait(3000);
		TFT_jpg_image(CENTER, CENTER, 0, SPIFFS_BASE_PATH"/images/sun3.jpg", NULL, 0);
		Wait(3000);
		TFT_jpg_image(CENTER, CENTER, 0, SPIFFS_BASE_PATH"/images/sun4.jpg", NULL, 0);
		Wait(3000);*/

		// ** Show BMP image
		/*update_header("BMP IMAGE", "");
		for (int scale=5; scale >= 0; scale--) {
			tstart = clock();
			TFT_bmp_image(CENTER, CENTER, scale, SPIFFS_BASE_PATH"/images/tiger.bmp", NULL, 0);
			tstart = clock() - tstart;
			if (doprint) printf("    BMP time, scale: %d: %u ms\r\n", scale, tstart);
			sprintf(tmp_buff, "Decode time: %u ms", tstart);
			update_header(NULL, tmp_buff);
			Wait(-500);
		}
		Wait(-GDEMO_INFO_TIME);*/
	}
	else if (doprint) printf("  No file system found.\r\n");
}

//---------------------
static void font_demo()
{
	int x, y, n;
	uint32_t end_time;

	disp_header("FONT DEMO");

	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) 
	{
		y = 4;
		for (int f=DEFAULT_FONT; f<FONT_7SEG; f++) {
			_fg = random_color();
			TFT_setFont(f, NULL);
			TFT_print("Welcome to ESP32", 4, y);
			y += TFT_getfontheight() + 4;
			n++;
		}
	}
	sprintf(tmp_buff, "%d STRINGS", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	if (spiffs_is_mounted) {
		disp_header("FONT FROM FILE DEMO");

		text_wrap = 1;
		for (int f=0; f<3; f++) 
		{
			TFT_fillWindow(TFT_BLACK);
			update_header(NULL, "");

			TFT_setFont(USER_FONT, file_fonts[f]);
			if (f == 0) font_line_space = 4;
			end_time = clock() + GDEMO_TIME;
			n = 0;
			while ((clock() < end_time) && (Wait(0))) 
			{
				_fg = random_color();
				TFT_print("Welcome to ESP32\nThis is user font.", 0, 8);
				n++;
			}
			if ((_width < 240) || (_height < 240)) TFT_setFont(DEF_SMALL_FONT, NULL);
            else TFT_setFont(DEFAULT_FONT, NULL);
			_fg = TFT_YELLOW;
			TFT_print((char *)file_fonts[f], 0, (dispWin.y2-dispWin.y1)-TFT_getfontheight()-4);

            font_line_space = 0;
			sprintf(tmp_buff, "%d STRINGS", n);
			update_header(NULL, tmp_buff);
			Wait(-GDEMO_INFO_TIME);
		}
		text_wrap = 0;
	}

	disp_header("ROTATED FONT DEMO");

	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		for (int f=DEFAULT_FONT; f<FONT_7SEG; f++) {
			_fg = random_color();
			TFT_setFont(f, NULL);
			x = rand_interval(8, dispWin.x2-8);
			y = rand_interval(0, (dispWin.y2-dispWin.y1)-TFT_getfontheight()-2);
			font_rotate = rand_interval(0, 359);

			TFT_print("Welcome to ESP32", x, y);
			n++;
		}
	}
	font_rotate = 0;
	sprintf(tmp_buff, "%d STRINGS", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	disp_header("7-SEG FONT DEMO");

	int ms = 0;
	int last_sec = 0;
	uint32_t ctime = clock();
	end_time = clock() + GDEMO_TIME*2;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		y = 12;
		ms = clock() - ctime;
		time(&time_now);
		tm_info = localtime(&time_now);
		if (tm_info->tm_sec != last_sec) {
			last_sec = tm_info->tm_sec;
			ms = 0;
			ctime = clock();
		}

		_fg = TFT_ORANGE;
		sprintf(tmp_buff, "%02d:%02d:%03d", tm_info->tm_min, tm_info->tm_sec, ms);
		TFT_setFont(FONT_7SEG, NULL);
        if ((_width < 240) || (_height < 240)) set_7seg_font_atrib(8, 1, 1, TFT_DARKGREY);
		else set_7seg_font_atrib(12, 2, 1, TFT_DARKGREY);
		//TFT_clearStringRect(12, y, tmp_buff);
		TFT_print(tmp_buff, CENTER, y);
		n++;

		_fg = TFT_GREEN;
		y += TFT_getfontheight() + 12;
		if ((_width < 240) || (_height < 240)) set_7seg_font_atrib(9, 1, 1, TFT_DARKGREY);
        else set_7seg_font_atrib(14, 3, 1, TFT_DARKGREY);
		sprintf(tmp_buff, "%02d:%02d", tm_info->tm_sec, ms / 10);
		//TFT_clearStringRect(12, y, tmp_buff);
		TFT_print(tmp_buff, CENTER, y);
		n++;

		_fg = random_color();
		y += TFT_getfontheight() + 8;
		set_7seg_font_atrib(6, 1, 1, TFT_DARKGREY);
		getFontCharacters((uint8_t *)tmp_buff);
		//TFT_clearStringRect(12, y, tmp_buff);
		TFT_print(tmp_buff, CENTER, y);
		n++;
	}
	sprintf(tmp_buff, "%d STRINGS", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	disp_header("WINDOW DEMO");

	TFT_saveClipWin();
	TFT_resetclipwin();
	TFT_drawRect(38, 48, (_width*3/4) - 36, (_height*3/4) - 46, TFT_WHITE);
	TFT_setclipwin(40, 50, _width*3/4, _height*3/4);

	if ((_width < 240) || (_height < 240)) TFT_setFont(DEF_SMALL_FONT, NULL);
    else TFT_setFont(UBUNTU16_FONT, NULL);
	text_wrap = 1;
	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		_fg = random_color();
		TFT_print("This text is printed inside the window.\nLong line can be wrapped to the next line.\nWelcome to ESP32", 0, 0);
		n++;
	}
	text_wrap = 0;
	sprintf(tmp_buff, "%d STRINGS", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	TFT_restoreClipWin();
}

//---------------------
static void rect_demo()
{
	int x, y, w, h, n;

	disp_header("RECTANGLE DEMO");

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(4, dispWin.x2-4);
		y = rand_interval(4, dispWin.y2-2);
		w = rand_interval(2, dispWin.x2-x);
		h = rand_interval(2, dispWin.y2-y);
		TFT_drawRect(x,y,w,h,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d RECTANGLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("FILLED RECTANGLE", "");
	TFT_fillWindow(TFT_BLACK);
	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(4, dispWin.x2-4);
		y = rand_interval(4, dispWin.y2-2);
		w = rand_interval(2, dispWin.x2-x);
		h = rand_interval(2, dispWin.y2-y);
		TFT_fillRect(x,y,w,h,random_color());
		TFT_drawRect(x,y,w,h,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d RECTANGLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//----------------------
static void pixel_demo()
{
	int x, y, n;

	disp_header("DRAW PIXEL DEMO");

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(0, dispWin.x2);
		y = rand_interval(0, dispWin.y2);
		TFT_drawPixel(x,y,random_color(),1);
		n++;
	}
	sprintf(tmp_buff, "%d PIXELS", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//---------------------
static void line_demo()
{
	int x1, y1, x2, y2, n;

	disp_header("LINE DEMO");

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x1 = rand_interval(0, dispWin.x2);
		y1 = rand_interval(0, dispWin.y2);
		x2 = rand_interval(0, dispWin.x2);
		y2 = rand_interval(0, dispWin.y2);
		TFT_drawLine(x1,y1,x2,y2,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d LINES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//----------------------
static void aline_demo()
{
	int x, y, len, angle, n;

	disp_header("LINE BY ANGLE DEMO");

	x = (dispWin.x2 - dispWin.x1) / 2;
	y = (dispWin.y2 - dispWin.y1) / 2;
	if (x < y) len = x - 8;
	else len = y -8;

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		for (angle=0; angle < 360; angle++) {
			TFT_drawLineByAngle(x,y, 0, len, angle, random_color());
			n++;
		}
	}

	TFT_fillWindow(TFT_BLACK);
	end_time = clock() + GDEMO_TIME;
	while ((clock() < end_time) && (Wait(0))) {
		for (angle=0; angle < 360; angle++) {
			TFT_drawLineByAngle(x, y, len/4, len/4,angle, random_color());
			n++;
		}
		for (angle=0; angle < 360; angle++) {
			TFT_drawLineByAngle(x, y, len*3/4, len/4,angle, random_color());
			n++;
		}
	}
	sprintf(tmp_buff, "%d LINES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//--------------------
static void arc_demo()
{
	uint16_t x, y, r, th, n, i;
	float start, end;
	color_t color, fillcolor;

	disp_header("ARC DEMO");

	x = (dispWin.x2 - dispWin.x1) / 2;
	y = (dispWin.y2 - dispWin.y1) / 2;

	th = 6;
	uint32_t end_time = clock() + GDEMO_TIME;
	i = 0;
	while ((clock() < end_time) && (Wait(0))) {
		if (x < y) r = x - 4;
		else r = y - 4;
		start = 0;
		end = 20;
		n = 1;
		while (r > 10) {
			color = random_color();
			TFT_drawArc(x, y, r, th, start, end, color, color);
			r -= (th+2);
			n++;
			start += 30;
			end = start + (n*20);
			i++;
		}
	}
	sprintf(tmp_buff, "%d ARCS", i);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("OUTLINED ARC", "");
	TFT_fillWindow(TFT_BLACK);
	th = 8;
	end_time = clock() + GDEMO_TIME;
	i = 0;
	while ((clock() < end_time) && (Wait(0))) {
		if (x < y) r = x - 4;
		else r = y - 4;
		start = 0;
		end = 350;
		n = 1;
		while (r > 10) {
			color = random_color();
			fillcolor = random_color();
			TFT_drawArc(x, y, r, th, start, end, color, fillcolor);
			r -= (th+2);
			n++;
			start += 20;
			end -= n*10;
			i++;
		}
	}
	sprintf(tmp_buff, "%d ARCS", i);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//-----------------------
static void circle_demo()
{
	int x, y, r, n;

	disp_header("CIRCLE DEMO");

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(8, dispWin.x2-8);
		y = rand_interval(8, dispWin.y2-8);
		if (x < y) r = rand_interval(2, x/2);
		else r = rand_interval(2, y/2);
		TFT_drawCircle(x,y,r,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d CIRCLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("FILLED CIRCLE", "");
	TFT_fillWindow(TFT_BLACK);
	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(8, dispWin.x2-8);
		y = rand_interval(8, dispWin.y2-8);
		if (x < y) r = rand_interval(2, x/2);
		else r = rand_interval(2, y/2);
		TFT_fillCircle(x,y,r,random_color());
		TFT_drawCircle(x,y,r,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d CIRCLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//------------------------
static void ellipse_demo()
{
	int x, y, rx, ry, n;

	disp_header("ELLIPSE DEMO");

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(8, dispWin.x2-8);
		y = rand_interval(8, dispWin.y2-8);
		if (x < y) rx = rand_interval(2, x/4);
		else rx = rand_interval(2, y/4);
		if (x < y) ry = rand_interval(2, x/4);
		else ry = rand_interval(2, y/4);
		TFT_drawEllipse(x,y,rx,ry,random_color(),15);
		n++;
	}
	sprintf(tmp_buff, "%d ELLIPSES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("FILLED ELLIPSE", "");
	TFT_fillWindow(TFT_BLACK);
	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(8, dispWin.x2-8);
		y = rand_interval(8, dispWin.y2-8);
		if (x < y) rx = rand_interval(2, x/4);
		else rx = rand_interval(2, y/4);
		if (x < y) ry = rand_interval(2, x/4);
		else ry = rand_interval(2, y/4);
		TFT_fillEllipse(x,y,rx,ry,random_color(),15);
		TFT_drawEllipse(x,y,rx,ry,random_color(),15);
		n++;
	}
	sprintf(tmp_buff, "%d ELLIPSES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("ELLIPSE SEGMENTS", "");
	TFT_fillWindow(TFT_BLACK);
	end_time = clock() + GDEMO_TIME;
	n = 0;
	int k = 1;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(8, dispWin.x2-8);
		y = rand_interval(8, dispWin.y2-8);
		if (x < y) rx = rand_interval(2, x/4);
		else rx = rand_interval(2, y/4);
		if (x < y) ry = rand_interval(2, x/4);
		else ry = rand_interval(2, y/4);
		TFT_fillEllipse(x,y,rx,ry,random_color(), (1<<k));
		TFT_drawEllipse(x,y,rx,ry,random_color(), (1<<k));
		k = (k+1) & 3;
		n++;
	}
	sprintf(tmp_buff, "%d SEGMENTS", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//-------------------------
static void triangle_demo()
{
	int x1, y1, x2, y2, x3, y3, n;

	disp_header("TRIANGLE DEMO");

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x1 = rand_interval(4, dispWin.x2-4);
		y1 = rand_interval(4, dispWin.y2-2);
		x2 = rand_interval(4, dispWin.x2-4);
		y2 = rand_interval(4, dispWin.y2-2);
		x3 = rand_interval(4, dispWin.x2-4);
		y3 = rand_interval(4, dispWin.y2-2);
		TFT_drawTriangle(x1,y1,x2,y2,x3,y3,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d TRIANGLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("FILLED TRIANGLE", "");
	TFT_fillWindow(TFT_BLACK);
	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x1 = rand_interval(4, dispWin.x2-4);
		y1 = rand_interval(4, dispWin.y2-2);
		x2 = rand_interval(4, dispWin.x2-4);
		y2 = rand_interval(4, dispWin.y2-2);
		x3 = rand_interval(4, dispWin.x2-4);
		y3 = rand_interval(4, dispWin.y2-2);
		TFT_fillTriangle(x1,y1,x2,y2,x3,y3,random_color());
		TFT_drawTriangle(x1,y1,x2,y2,x3,y3,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d TRIANGLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//---------------------
static void poly_demo()
{
	uint16_t x, y, rot, oldrot;
	int i, n, r;
	uint8_t sides[6] = {3, 4, 5, 6, 8, 10};
	color_t color[6] = {TFT_WHITE, TFT_CYAN, TFT_RED,       TFT_BLUE,     TFT_YELLOW,     TFT_ORANGE};
	color_t fill[6]  = {TFT_BLUE,  TFT_NAVY,   TFT_DARKGREEN, TFT_DARKGREY, TFT_LIGHTGREY, TFT_OLIVE};

	disp_header("POLYGON DEMO");

	x = (dispWin.x2 - dispWin.x1) / 2;
	y = (dispWin.y2 - dispWin.y1) / 2;

	rot = 0;
	oldrot = 0;
	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		if (x < y) r = x - 4;
		else r = y - 4;
		for (i=5; i>=0; i--) {
			TFT_drawPolygon(x, y, sides[i], r, TFT_BLACK, TFT_BLACK, oldrot, 1);
			TFT_drawPolygon(x, y, sides[i], r, color[i], color[i], rot, 1);
			r -= 16;
            if (r <= 0) break;
			n += 2;
		}
		Wait(100);
		oldrot = rot;
		rot = (rot + 15) % 360;
	}
	sprintf(tmp_buff, "%d POLYGONS", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("FILLED POLYGON", "");
	rot = 0;
	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		if (x < y) r = x - 4;
		else r = y - 4;
		TFT_fillWindow(TFT_BLACK);
		for (i=5; i>=0; i--) {
			TFT_drawPolygon(x, y, sides[i], r, color[i], fill[i], rot, 2);
			r -= 16;
            if (r <= 0) break;
			n += 2;
		}
		Wait(500);
		rot = (rot + 15) % 360;
	}
	sprintf(tmp_buff, "%d POLYGONS", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}





void tft_demo() 
{

	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	TFT_resetclipwin();

	image_debug = 0;

    char dtype[16];
    
    switch (tft_disp_type) {
        case DISP_TYPE_ILI9341:
            sprintf(dtype, "ILI9341");
            break;
        case DISP_TYPE_ILI9488:
            sprintf(dtype, "ILI9488");
            break;
        case DISP_TYPE_ST7789V:
            sprintf(dtype, "ST7789V");
            break;
        case DISP_TYPE_ST7735:
            sprintf(dtype, "ST7735");
            break;
        case DISP_TYPE_ST7735R:
            sprintf(dtype, "ST7735R");
            break;
        case DISP_TYPE_ST7735B:
            sprintf(dtype, "ST7735B");
            break;
        default:
            sprintf(dtype, "Unknown");
    }

	printf("tft_disp_type=%s\n",dtype);
    
    uint8_t disp_rot = PORTRAIT;
	_demo_pass = 0;
	gray_scale = 0;
	doprint = 1;

	TFT_setRotation(disp_rot);

	TFT_setFont(COMIC24_FONT, NULL);
	int tempy = TFT_getfontheight() + 4;
	_fg = TFT_ORANGE;
	TFT_print("AIRBOX2", CENTER, (dispWin.y2-dispWin.y1)/2 - tempy);

	TFT_setFont(UBUNTU16_FONT, NULL);
	_fg = TFT_CYAN;
	TFT_print("ShiTong Technology", CENTER, LASTY+tempy);

	tempy = TFT_getfontheight() + 4;
	TFT_setFont(DEFAULT_FONT, NULL);
	_fg = TFT_GREEN;
	sprintf(tmp_buff, "Read speed: %5.2f MHz", (float)max_rdclock/1000000.0);
	TFT_print(tmp_buff, CENTER, LASTY+tempy);

	Wait(4000);


	while(1)
	{
		
		disp_images();
	}

	while (1) 
	{
		_bg = TFT_BLACK;
		TFT_setRotation(disp_rot);
		/*if (run_gs_demo) 
		{
			if (_demo_pass == 8) doprint = 0;
			// Change gray scale mode on every 2nd pass
			gray_scale = _demo_pass & 1;
			// change display rotation
			if ((_demo_pass % 2) == 0) {
				_bg = TFT_BLACK;
				TFT_setRotation(disp_rot);
				disp_rot++;
				disp_rot &= 3;
			}
		}
		else 
		{
			if (_demo_pass == 4) doprint = 0;
			// change display rotation
			_bg = TFT_BLACK;
			TFT_setRotation(disp_rot);
			disp_rot++;
			disp_rot &= 3;
		}*/

		if (doprint) 
		{
			if (disp_rot == 1) sprintf(tmp_buff, "PORTRAIT");
			if (disp_rot == 2) sprintf(tmp_buff, "LANDSCAPE");
			if (disp_rot == 3) sprintf(tmp_buff, "PORTRAIT FLIP");
			if (disp_rot == 0) sprintf(tmp_buff, "LANDSCAPE FLIP");
			printf("\r\n==========================================\r\nDisplay: %s: %s %d,%d %s\r\n\r\n",
					dtype, tmp_buff, _width, _height, ((gray_scale) ? "Gray" : "Color"));
		}


		font_demo();
		//arc_demo();
		disp_images();

		//_demo_pass++;
	}
}




void tft_init()
{

    // ========  PREPARE DISPLAY INITIALIZATION  =========

    esp_err_t ret;

    // === SET GLOBAL VARIABLES ==========================

    // ===================================================
    // ==== Set display type                         =====
    //tft_disp_type = DEFAULT_DISP_TYPE;
	tft_disp_type = DISP_TYPE_ILI9341;
	//tft_disp_type = DISP_TYPE_ILI9488;
	//tft_disp_type = DISP_TYPE_ST7735B;
    // ===================================================

	// ===================================================
	// === Set display resolution if NOT using default ===
	// === DEFAULT_TFT_DISPLAY_WIDTH &                 ===
    // === DEFAULT_TFT_DISPLAY_HEIGHT                  ===
	_width = DEFAULT_TFT_DISPLAY_WIDTH;  // smaller dimension
	_height = DEFAULT_TFT_DISPLAY_HEIGHT; // larger dimension
	//_width = 128;  // smaller dimension
	//_height = 160; // larger dimension
	// ===================================================

	// ===================================================
	// ==== Set maximum spi clock for display read    ====
	//      operations, function 'find_rd_speed()'    ====
	//      can be used after display initialization  ====
	max_rdclock = 8000000;
	// ===================================================

    // ====================================================================
    // === Pins MUST be initialized before SPI interface initialization ===
    // ====================================================================
    TFT_PinsInit();

    // ====  CONFIGURE SPI DEVICES(s)  ====================================================================================

    spi_lobo_device_handle_t spi;
	
    spi_lobo_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,				// set SPI MISO pin
        .mosi_io_num=PIN_NUM_MOSI,				// set SPI MOSI pin
        .sclk_io_num=PIN_NUM_CLK,				// set SPI CLK pin
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
		.max_transfer_sz = 6*1024,
    };
    spi_lobo_device_interface_config_t devcfg={
        .clock_speed_hz=8000000,                // Initial clock out at 8 MHz
        .mode=0,                                // SPI mode 0
        .spics_io_num=-1,                       // we will use external CS pin
		.spics_ext_io_num=PIN_NUM_CS,           // external CS pin
		.flags=LB_SPI_DEVICE_HALFDUPLEX,        // ALWAYS SET  to HALF DUPLEX MODE!! for display spi
    };

    // ====================================================================================================================


    vTaskDelay(500 / portTICK_RATE_MS);
	
	//init spi
	ret=spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
    assert(ret==ESP_OK);
	printf("SPI: display device added to spi bus (%d)\r\n", SPI_BUS);
	disp_spi = spi;

	// ==== Test select/deselect ====
	ret = spi_lobo_device_select(spi, 1);
    assert(ret==ESP_OK);
	ret = spi_lobo_device_deselect(spi);
    assert(ret==ESP_OK);

	printf("SPI: attached display device, speed=%u\r\n", spi_lobo_get_speed(spi));
	printf("SPI: bus uses native pins: %s\r\n", spi_lobo_uses_native_pins(spi) ? "true" : "false");


	// ==== Initialize the Display ====

	printf("SPI: display init...\r\n");
	TFT_display_init(); //屏幕全黑
    printf("OK\r\n");
    

	
	// ---- Detect maximum read speed ----
	max_rdclock = find_rd_speed();
	printf("SPI: Max rd speed = %u\r\n", max_rdclock);

    // ==== Set SPI clock used for display operations ====
	spi_lobo_set_speed(spi, DEFAULT_SPI_CLOCK);
	printf("SPI: Changed speed to %u\r\n", spi_lobo_get_speed(spi));

    printf("\r\n---------------------\r\n");
	printf("Graphics demo started\r\n");
	printf("---------------------\r\n");

	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	gray_scale = 0;
    TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
	TFT_setRotation(PORTRAIT);
	TFT_setFont(DEFAULT_FONT, NULL);
	TFT_resetclipwin();//清屏

	TFT_setFont(COMIC24_FONT, NULL);
	int tempy ;//= TFT_getfontheight() + 20;
	_fg = TFT_ORANGE;
	//TFT_print("AIRBOX2", CENTER, (dispWin.y2-dispWin.y1)/2 - tempy);
	TFT_print("AIRBOX2", CENTER,250);

	//tempy = TFT_getfontheight() + 4;
	//TFT_setFont(DEFAULT_FONT, NULL);
	//_fg = TFT_GREEN;
	//sprintf(tmp_buff, "Read speed: %5.2f MHz", (float)max_rdclock/1000000.0);
	//TFT_print(tmp_buff, CENTER, LASTY+tempy);

	


	TFT_setFont(DEFAULT_FONT, NULL);
    _fg = TFT_CYAN;
	TFT_print("Initializing SPIFFS...", CENTER, LASTY+TFT_getfontheight() + 20);
    // ==== Initialize the file system ====
    printf("\r\n\n");
	vfs_spiffs_register();
	TFT_jpg_image(CENTER, CENTER, 0, SPIFFS_BASE_PATH"/images/logo.jpg", NULL, 0);
	Wait(-1000);
    if (!spiffs_is_mounted) 
	{
		_fg = TFT_RED;
    	TFT_print("SPIFFS not mounted !", CENTER,LASTY+ TFT_getfontheight() + 4);
    }
    else 
	{
		_fg = TFT_GREEN;
    	TFT_print("SPIFFS Mounted！", CENTER,LASTY+ TFT_getfontheight() + 4);
    }
	Wait(-2000);


	//显示主图底图
	TFT_jpg_image(CENTER, CENTER, 0, SPIFFS_BASE_PATH"/images/main1.jpg", NULL, 0);
	//显示顶端图标
	TFT_jpg_image(0, 0, 0, SPIFFS_BASE_PATH"/images/temperature.jpg", NULL, 0);
	TFT_jpg_image(70, 0, 0, SPIFFS_BASE_PATH"/images/humidity.jpg", NULL, 0);
	TFT_jpg_image(180, 0, 0, SPIFFS_BASE_PATH"/images/wifi-error.jpg", NULL, 0);
	TFT_jpg_image(210, 0, 0, SPIFFS_BASE_PATH"/images/battery.jpg", NULL, 0);
	//显示四宫格标题和单位
	TFT_jpg_image(10, 31, 0, SPIFFS_BASE_PATH"/images/formaldehyde.jpg", NULL, 0);
	TFT_jpg_image(10, 125, 0, SPIFFS_BASE_PATH"/images/ugm3.jpg", NULL, 0);

	TFT_jpg_image(10,161, 0, SPIFFS_BASE_PATH"/images/tvoc.jpg", NULL, 0);
	TFT_jpg_image(10, 255, 0, SPIFFS_BASE_PATH"/images/ppb.jpg", NULL, 0);

	TFT_jpg_image(130, 31, 0, SPIFFS_BASE_PATH"/images/pm25.jpg", NULL, 0);
	TFT_jpg_image(130, 125, 0, SPIFFS_BASE_PATH"/images/ugm3.jpg", NULL, 0);

	TFT_jpg_image(130,161, 0, SPIFFS_BASE_PATH"/images/co2.jpg", NULL, 0);
	TFT_jpg_image(130, 255, 0, SPIFFS_BASE_PATH"/images/ppm.jpg", NULL, 0);

	//font_transparent = 1; //无背景
	//TFT_setFont(UBUNTU16_FONT, NULL);
	//_fg = TFT_WHITE;
	//TFT_print("Temperature", 5, 40);

	//TFT_print("Humidity", 150, 40);



	//tft_demo();
}



