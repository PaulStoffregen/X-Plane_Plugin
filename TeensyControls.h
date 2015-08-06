#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

//#define USE_PRINTF_DEBUG
//#define PRINTF_ADDR "10.0.0.123"
//#define PRINTF_ADDR "127.0.0.1"

#if defined(MACOSX)
#define APL 1
#define IBM 0
#define LIN 0
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDDevice.h>
#elif defined(LINUX)
#define APL 0
#define IBM 0
#define LIN 1
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <pthread.h>
#include <time.h>
#ifdef USE_PRINTF_DEBUG
#include <execinfo.h>
#endif
#include <libudev.h>	// sudo apt-get install libudev-dev
#include <linux/hidraw.h>
#elif defined(WINDOWS) || defined(WINDOWS64)
#define APL 0
#define IBM 1
#define LIN 0
#ifndef WINVER
#define WINVER 0x0500
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
//#include <winsock2.h>
#include <windows.h>
//#include <ws2tcpip.h>
//#include <winuser.h>
#include <setupapi.h>
//#include <ddk/hidsdi.h>
#include <hidsdi.h>
//#include <ddk/hidclass.h>
#include <dbt.h>
#include "thread.h"
#endif
#define XPLM200 1
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMUtilities.h"
#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"
#include "XPLMMenus.h"



#if defined(MACOSX)
typedef struct {
	IOHIDDeviceRef dev;
	uint8_t inbuf[64];
} usb_t;

#elif defined(LINUX)
typedef struct {
	int fd;
	int error_count;
} usb_t;

#elif defined(WINDOWS) || defined(WINDOWS64)
typedef struct {
	HANDLE handle;
	int error_count;
	HANDLE rx_event;
	OVERLAPPED rx_ov;
	HANDLE tx_event;
	OVERLAPPED tx_ov;
	char devpath[1024];
} usb_t;

#endif



typedef struct item_struct {
	int id;			// ID assigned by Teensy
	int type;		// data type on Teensy, 0=cmd, 1=long, 2=float
	int index;		// -1 if not an array, 0 to more for array vars
	char name[64];		// X-Plane Command or Data name
	XPLMCommandRef cmdref;
	uint8_t command_queue[128];  // 4=begin, 5=end, 6=once
	int command_count;	// number of cmds in command_queue
	int command_began;	// non-zero if command begin but no end yet
	XPLMDataRef dataref;
	XPLMDataTypeID datatype;
	int datawritable;
	int32_t intval;		// int value, most recent
	int32_t intval_remote;	// int value, as exists on Teensy
	float floatval;		// float value, most recent
	float floatval_remote;	// float value, as exists on Teensy
	int changed_by_teensy;	// non-zero if teensy changed data, not yet written to xplane
	struct item_struct *prev;
	struct item_struct *next;
} item_t;

#define INPUT_BUFSIZE 160
#define OUTPUT_BUFSIZE 50

typedef struct teensy_struct {
	usb_t usb;
	volatile int online;		// created as 1, set to 0 when device goes offline
	item_t *items;
	pthread_mutex_t input_mutex;	// must lock when accessing input head, tail, buffer
	volatile int input_thread_quit;
	volatile int input_head;
	volatile int input_tail;
	uint8_t input_buffer[64*INPUT_BUFSIZE];
	pthread_mutex_t output_mutex;	// must lock when accessing output head, tail, buffer
	pthread_cond_t output_event;
	volatile int output_thread_quit;
	volatile int output_thread_waiting;
	volatile int output_head;
	volatile int output_tail;
	uint8_t output_buffer[64*OUTPUT_BUFSIZE];
	uint8_t output_packet[64];
	int output_packet_len;
	int unknown_id_heard;
	//item_t *itemlists[64]; // TODO: rapid lookup by ID....
	
	uint8_t input_packet[256];
	uint8_t expect_fragment_id;
	uint8_t *input_packet_ptr;
	int32_t input_packet_bytes_missing;
	struct teensy_struct *next;
} teensy_t;



// io.c
void TeensyControls_input(float elapsed, int flags);
void TeensyControls_update_xplane(float elapsed);
void TeensyControls_output(float elapsed, int flags);

// usb.c
int TeensyControls_usb_init(void);
void TeensyControls_find_new_usb_devices(void);
void TeensyControls_usb_close(void);

// memory.c
extern teensy_t * TeensyControls_first_teensy;
teensy_t * TeensyControls_new_teensy(void);
void TeensyControls_delete_offline_teensy(void);
void TeensyControls_input_store(teensy_t *t, const uint8_t *packet);
int  TeensyControls_input_fetch(teensy_t *t, uint8_t *packet);
void TeensyControls_output_store(teensy_t *t, const uint8_t *packet);
int  TeensyControls_output_fetch(teensy_t *t, uint8_t *packet);
void TeensyControls_new_item(teensy_t *t, int id, int type, const char *name, int namelen);
item_t * TeensyControls_find_item(teensy_t *t, int id);

// display.c
extern int TeensyControls_show;
#define display(...) (TeensyControls_show ? TeensyControls_display(__VA_ARGS__) : 0)
void TeensyControls_display_init(void);
int TeensyControls_display(const char *format, ...) __attribute__ ((format (printf, 1, 2)));;
int TeensyControls_display(const char *format, ...);

// thread.c
int thread_start(void (*function)(void*), void *arg);

// printf.c
#ifdef USE_PRINTF_DEBUG
#define PRINTF_PORT 29372
#define printf(...) TeensyControls_printf(__VA_ARGS__)
void TeensyControls_printf_init(void);
int TeensyControls_printf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));;
int TeensyControls_printf(const char *format, ...);
#else
#define printf(...)
#define TeensyControls_printf_init()
#endif
