#include "TeensyControls.h"

// http://www.xsquawkbox.net/xpsdk/mediawiki/Category:Documentation

static XPLMMenuID menu;
static int itemShow;
int TeensyControls_show=0;

static float main_callback(float elapsedPrev, float elapsedFlight, int counter, void *ptr)
{
	if (elapsedPrev < 0) elapsedPrev = 0;
	TeensyControls_delete_offline_teensy();
	TeensyControls_find_new_usb_devices();
	TeensyControls_input(elapsedPrev, 0);
	TeensyControls_update_xplane(elapsedPrev);
	TeensyControls_output(elapsedPrev, 0);
	return -1;  // -1 = run every frame
}

void menu_callback(void *inMenuRef, void *inItemRef)
{
	TeensyControls_show = !TeensyControls_show;
	TeensyControls_display_init();
	XPLMSetMenuItemName(menu, itemShow, (TeensyControls_show) ?
		"Hide Communication" : "Show Communication", 1);
}

PLUGIN_API int XPluginStart(char *name, char *sig, char *desc)
{
	XPLMMenuID pmenu;
	int mitem;

	TeensyControls_printf_init();
	strcpy(name, "TeensyControls");
	strcpy(sig, "pjrc.TeensyControls");
	strcpy(desc, "Teensy-based USB Devices For X-Plane");
	if (!TeensyControls_usb_init()) return 0;
	XPLMRegisterFlightLoopCallback(main_callback, -1, NULL);
	pmenu = XPLMFindPluginsMenu();
	mitem = XPLMAppendMenuItem(pmenu, "TeensyControls", NULL, 1);
	menu = XPLMCreateMenu("TeensyControls", pmenu, mitem, menu_callback, NULL);
	itemShow = XPLMAppendMenuItem(menu, "Show Communication", "show", 1);
#ifdef USE_PRINTF_DEBUG
	menu_callback(NULL, NULL); // testing only: show display by default
#endif
	return 1; // 1 = plugin started ok
}

PLUGIN_API void XPluginStop(void)
{
	printf("Plugin Stop\n");
	TeensyControls_usb_close();
	TeensyControls_delete_offline_teensy();
	printf("Plugin End\n");
}

PLUGIN_API int XPluginEnable(void)
{
	printf("Plugin Enable\n");
	TeensyControls_output(0.0, 1);
	return 1;
}

PLUGIN_API void XPluginDisable(void)
{
	printf("Plugin Disable\n");
	TeensyControls_output(0.0, 2);
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, long msg, void *param)
{
}


