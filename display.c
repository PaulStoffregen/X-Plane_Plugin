#include "TeensyControls.h"


#define WIDTH 890
#define LINES 30

static XPLMWindowID w=NULL;


static char text[LINES*256];
int head=0;

#define EQ(s1, s2) (strncmp(s1, s2, strlen(s2)) == 0)

void draw(XPLMWindowID inWindowID, void *inRefcon)
{
	int i, n, left, top, right, bottom, y;
	char *str;
	float yellow[] = {1.0, 1.0, 0.5};
	float red[] = {1.0, 0.4, 0.4};
	float lime[] = {0.8, 1.0, 0.6};
	float aqua[] = {0.4, 0.5, 0.8};
	float white[] = {1.0, 1.0, 1.0};
	float *color;

	XPLMGetWindowGeometry(inWindowID, &left, &top, &right, &bottom);
	XPLMDrawTranslucentDarkBox(left, top, right, bottom);
	y = bottom + 6;
	n = head;
	for (i=0; i < LINES; i++) {
		color = white;
		str = text + n * 256;
		if (EQ(str, "Write")) color = yellow;
		if (EQ(str, "Teensy")) color = red;
		if (EQ(str, "Command")) color = lime;
		if (EQ(str, "Update")) color = aqua;

		XPLMDrawString(color, left + 5, y, str, NULL, xplmFont_Basic);
		if (n == 0) n = LINES;
		n--;
		y += 18;
	}
}

void key(XPLMWindowID inWindowID, char inKey, XPLMKeyFlags inFlags,
	char inVirtualKey, void *inRefcon, int losingFocus)
{
}

int mouse(XPLMWindowID inWindowID, int x, int y, XPLMMouseStatus inMouse, void *inRefcon)
{
	return 0;
}


void TeensyControls_display_init(void)
{
	printf("TeensyControls_display_init\n");
	if (TeensyControls_show) {
		if (!w) {
			w = XPLMCreateWindow(50, 590, 950, 50, 1,
				draw, key, mouse, NULL);
			head = 0;
			memset(text, 0, sizeof(text));
		}
	} else {
		if (w) {
			XPLMDestroyWindow(w);
			w = NULL;
		}

	}
}

int TeensyControls_display(const char *format, ...)
{
	va_list args;
	int len;

	va_start(args, format);
	if (++head >= LINES) head = 0;
	len = vsnprintf(text + (head * 256), 255, format, args);
	if (len <= 0 || len > 255) return 0;
	// TODO: use XPLMMeasureString to trim string
	//XPLMMeasureString(xplmFont_Basic, "test", 4);

	return len;
}








