#include "TeensyControls.h"


// count the number of running I/O threads
// this is the same for all platforms
static int num_thread_alive(void)
{
	int count=0;
	teensy_t *t;
	for (t = TeensyControls_first_teensy; t; t = t->next) {
		if (!t->input_thread_quit) count++;
		if (!t->output_thread_quit) count++;
	}
	return count;
}


/*************************************************************************/
/**                                                                     **/
/**                               Mac OS X                              **/
/**                                                                     **/
/*************************************************************************/

#if defined(MACOSX)



// called from input thread
static void input_callback(void *context, IOReturn r, void *dev, IOHIDReportType type,
		uint32_t id, uint8_t *data, CFIndex len)
{
	teensy_t *t = (teensy_t *)context;
	printf("HID, input callback, len=%d\n", (int)len);
	TeensyControls_input_store(t, t->usb.inbuf);
}

static void input_thread(void *arg)
{
	teensy_t *t = (teensy_t *)arg;
	printf("input_thread begin\n");
	IOHIDDeviceRegisterInputReportCallback(t->usb.dev,
		t->usb.inbuf, 64, input_callback, t);
	while (1) {
		int r;
		if (t->online == 0) break;
		r = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, true);
	}
	t->input_thread_quit = 1;
	printf("input_thread end\n");
}

static void output_thread(void *arg)
{
	teensy_t *t = (teensy_t *)arg;
	struct timeval tv;
	struct timespec ts;
	uint8_t buf[64];
	int r;

	printf("output_thread begin\n");
	while (1) {
		//printf("output_thread\n");
		if (t->online == 0) break;
		if (TeensyControls_output_fetch(t, buf) && t->online) {
			//printf("output_thread: send\n");
			IOHIDDeviceSetReport(t->usb.dev,
				kIOHIDReportTypeOutput,
				0, buf, 64);
		} else {
			pthread_mutex_lock(&t->output_mutex);
			if (t->output_head == t->output_tail) {
				t->output_thread_waiting = 1;
				//clock_gettime(CLOCK_REALTIME, &ts);
				gettimeofday(&tv, NULL);
				ts.tv_sec = tv.tv_sec;
				ts.tv_nsec = tv.tv_usec*1000;
				ts.tv_sec += 1;
				r = pthread_cond_timedwait(&t->output_event,
					&t->output_mutex, &ts);
				t->output_thread_waiting = 0;
				//printf("output_thread, r: %d, errno: %d\n", r, errno);
			}
			pthread_mutex_unlock(&t->output_mutex);
			// wait for event signal, 1 sec timeout
		}
	}
	t->output_thread_quit = 1;
	printf("output_thread end\n");
}

// called from main thread
static void attach_callback(void *context, IOReturn r, void *mgr, IOHIDDeviceRef dev)
{
	IOReturn ret;
	teensy_t *t;

	printf("HID attach callback\n");
	ret = IOHIDDeviceOpen(dev, kIOHIDOptionsTypeNone);
	if (ret == kIOReturnSuccess) {
		t = TeensyControls_new_teensy();
		if (t) {
			t->usb.dev = dev;
			if (!thread_start(input_thread, t)) t->input_thread_quit = 1;
			if (!thread_start(output_thread, t)) t->output_thread_quit = 1;
		}
	}
}

// called from main thread
static void detach_callback(void *context, IOReturn r, void *mgr, IOHIDDeviceRef dev)
{
	teensy_t *t;

	printf("HID detach callback\n");
	for (t = TeensyControls_first_teensy; t; t = t->next) {
		if (t->usb.dev == dev) {
			t->online = 0;
		}
	}
}

static void add_to_array(CFMutableArrayRef array, uint32_t vendor_id,
        uint32_t product_id, uint32_t usage_page, uint32_t usage)
{
        CFMutableDictionaryRef dict;
        CFNumberRef n_vid, n_pid, n_upg, n_u;

        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!dict) return;
        n_vid = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vendor_id);
        if (!n_vid) return;
        n_pid = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &product_id);
        if (!n_pid) return;
        n_upg = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage_page);
        if (!n_upg) return;
        n_u = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
        if (!n_u) return;
        CFDictionarySetValue(dict, CFSTR(kIOHIDVendorIDKey), n_vid);
        CFDictionarySetValue(dict, CFSTR(kIOHIDProductIDKey), n_pid);
        CFDictionarySetValue(dict, CFSTR(kIOHIDPrimaryUsagePageKey), n_upg);
        CFDictionarySetValue(dict, CFSTR(kIOHIDPrimaryUsageKey), n_u);
        CFRelease(n_vid);
        CFRelease(n_pid);
        CFRelease(n_upg);
        CFRelease(n_u);
        CFArrayAppendValue(array, dict);
        CFRelease(dict);
}

// http://developer.apple.com/technotes/tn2007/tn2187.html

static IOHIDManagerRef hmgr = NULL;

int TeensyControls_usb_init(void)
{
	CFMutableArrayRef array = NULL;
	IOReturn ret;

	hmgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	if (!hmgr) goto fail;
	if (CFGetTypeID(hmgr) != IOHIDManagerGetTypeID()) goto fail;
	array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	if (!array) goto fail;
	add_to_array(array, 0x16C0, 0x0488, 0xFF1C, 0xA739); // Teensyduino Flight Sim
	IOHIDManagerSetDeviceMatchingMultiple(hmgr, array);
	CFRelease(array);
	array = NULL;
	IOHIDManagerScheduleWithRunLoop(hmgr, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	IOHIDManagerRegisterDeviceMatchingCallback(hmgr, attach_callback, NULL);
	IOHIDManagerRegisterDeviceRemovalCallback(hmgr, detach_callback, NULL);
	ret = IOHIDManagerOpen(hmgr, kIOHIDOptionsTypeNone);
	if (ret != kIOReturnSuccess) {
		IOHIDManagerUnscheduleFromRunLoop(hmgr,
			CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
		goto fail;
	}
	return 1;
fail:
	if (array) CFRelease(array);
	if (hmgr) CFRelease(hmgr);
	return 0;
}


void TeensyControls_find_new_usb_devices(void)
{
	while (1) {
		int r = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
		if (r != kCFRunLoopRunHandledSource) break;
	}
}



void TeensyControls_usb_close(void)
{
	teensy_t *t;
	int wait=0;

	for (t = TeensyControls_first_teensy; t; t = t->next) {
		if (t->online) {
			printf("close USB device\n");
			IOHIDDeviceRegisterInputReportCallback(t->usb.dev,
				t->usb.inbuf, 64, NULL, NULL);
			// TODO: how to terminate input thread?
			// TODO: how to terminate output thread?
			pthread_cond_signal(&t->output_event);
			IOHIDDeviceClose(t->usb.dev, 0);
			t->online = 0;
		}
	}
	if (hmgr) {
		printf("closing hid manager\n");
		IOHIDManagerRegisterDeviceMatchingCallback(hmgr, NULL, NULL);
		IOHIDManagerRegisterDeviceRemovalCallback(hmgr, NULL, NULL);
		IOHIDManagerClose(hmgr, 0);
		IOHIDManagerUnscheduleFromRunLoop(hmgr, CFRunLoopGetCurrent(),
			kCFRunLoopDefaultMode);
		CFRelease(hmgr);
		hmgr = NULL;
	}
	while (++wait < 20 && num_thread_alive() > 0) {
		usleep(10000);
		printf("wait #%d for thread exit\n", wait);
	}
}


/*************************************************************************/
/**                                                                     **/
/**                               Linux                                 **/
/**                                                                     **/
/*************************************************************************/

#elif defined(LINUX)


static void input_thread(void *arg)
{
	teensy_t *t = (teensy_t *)arg;
	fd_set rfds, efds;
	uint8_t buf[64];
	int fd, n, r;

	printf("input_thread begin\n");
	fd = t->usb.fd;
	while (t->online) {
		printf("input_thread\n");
		FD_ZERO(&rfds);
		FD_ZERO(&efds);
		FD_SET(fd, &rfds);
		FD_SET(fd, &efds);
		r = select(fd+1, &rfds, NULL, &efds, NULL);
		if (r > 0 && FD_ISSET(fd, &rfds)) {
			n = read(fd, buf, 64);
			if (n == 64) {
				TeensyControls_input_store(t, buf);
				t->usb.error_count = 0;
			} else {
				printf("read error, n = %d, errno = %d", n, errno);
				printf(", count = %d\n", t->usb.error_count);
				if (n < 0 && (errno == EAGAIN || errno == EINTR)) continue;
				if (n < 0 && errno == ENODEV) {
					t->online = 0;
				} else {
					if (++t->usb.error_count > 8) t->online = 0;
				}
			}
		} else {
			printf("input: select, r = %d", r);
			if (++t->usb.error_count > 8) t->online = 0;
		}
	}
	t->input_thread_quit = 1;
	printf("input_thread end\n");
}



static void output_thread(void *arg)
{
	teensy_t *t = (teensy_t *)arg;
	struct timespec ts;
	uint8_t buf[65];
	int n;

	printf("output_thread begin\n");
	while (t->online) {
		//printf("output_thread\n");
		if (TeensyControls_output_fetch(t, buf + 1) && t->online) {
			//printf("output_thread: send\n");
			tryagain:
			buf[0] = 0;
			n = write(t->usb.fd, buf, 65);
			if (n == 65) {
				t->usb.error_count = 0;
			} else {
				printf("write error, n=%d, errno=%d\n", n, errno);
				if (n < 0 && errno == EINTR) {
					usleep(5000);
					if (++t->usb.error_count < 20) {
						goto tryagain;
					}
				} else if (n < 0 && errno == ENODEV) {
					t->online = 0;
				} else {
					if (++t->usb.error_count > 8) {
						t->online = 0;
					}
				}
			}
		} else {
			//printf("output_thread, no data\n");
			pthread_mutex_lock(&t->output_mutex);
			if (t->output_head == t->output_tail) {
				clock_gettime(CLOCK_REALTIME, &ts);
				ts.tv_sec += 1;
				t->output_thread_waiting = 1;
				//gettimeofday(&tv, NULL);
				//ts.tv_sec = tv.tv_sec;
				//ts.tv_nsec = tv.tv_usec*1000;
				pthread_cond_timedwait(&t->output_event,
					&t->output_mutex, &ts);
				t->output_thread_waiting = 0;
				//printf("output_thread, r: %d, errno: %d\n", r, errno);
			}
			pthread_mutex_unlock(&t->output_mutex);
		}
	}
	t->output_thread_quit = 1;
	printf("output_thread end\n");
}




// using libudev to monitor for device changes
// http://www.signal11.us/oss/udev/

static struct udev *udev=NULL;
static struct udev_monitor *mon=NULL;
static int monfd=-1;

//static void new_usb_device(struct udev_device *dev) __attribute__((noinline));
static void new_usb_device(struct udev_device *dev)
{
	struct udev_device *usb;
	const char *str, *devname;
	int vid=0, pid=0, is_teensy=0;
	teensy_t *t;
	int r, len, fd=-1;
	const uint8_t signature[6]={0x06,0x1C,0xFF,0x0A,0x39,0xA7};
	struct hidraw_devinfo info;
	struct hidraw_report_descriptor desc;

	devname = udev_device_get_devnode(dev);
	if (!devname) goto fail;
	usb = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
	if (!usb) goto fail;
	str = udev_device_get_sysattr_value(usb, "idVendor");
	if (!str || sscanf(str, "%x", &vid) != 1) vid = 0;
	str = udev_device_get_sysattr_value(usb, "idProduct");
	if (!str || sscanf(str, "%x", &pid) != 1) pid = 0;
	str = udev_device_get_sysattr_value(usb, "product");
	if (str && strstr(str, "Teensy")) is_teensy = 1;
	//udev_device_unref(usb); // this does NOT need to be unref'd
	if (!is_teensy) goto fail;
	if (vid != 0x16C0) goto fail;
	if (pid != 0x0488) goto fail;
	printf("usb device: %s, vid:pid = %04X:%04X\n", devname, vid, pid);

	fd = open(devname, O_RDWR);
	if (fd < 0) {
		printf("unable to open\n");
		if (errno == EACCES) {
			display("Teensy permission denied, please install udev rules");
		} else {
			display("Teensy unable to open, errno=%d", errno);
		}
		goto fail;
	}
	printf("device %s opened\n", devname);
	r = ioctl(fd, HIDIOCGRAWINFO, &info);
	if (r < 0) goto fail;

	r = ioctl(fd, HIDIOCGRDESCSIZE, &len);
	if (r < 0) goto fail;
	if (len < sizeof(signature)) goto fail;
	printf("device %s, descriptor len = %d, max = %d\n", devname, len, HID_MAX_DESCRIPTOR_SIZE);
	desc.size = len;
	r = ioctl(fd, HIDIOCGRDESC, &desc);
	printf("Teensy descriptors fetched, r = %d\n", r);
	if (r < 0) goto fail;
	if (memcmp(desc.value, signature, sizeof(signature)) != 0) goto fail;
	printf("Teensy descriptors confirmed\n");
	t = TeensyControls_new_teensy();
	if (!t) goto fail;
	printf("Teensy success\n");
	t->usb.fd = fd;
	t->usb.error_count = 0;
	if (!thread_start(input_thread, t)) t->input_thread_quit = 1;
	if (!thread_start(output_thread, t)) t->output_thread_quit = 1;
	return;
fail:
	printf("Teensy fail\n");
	if (fd >= 0) close(fd);
	return;
}


void TeensyControls_find_new_usb_devices(void)
{
	fd_set fds;
	struct timeval tv;
	struct udev_device *dev;
	const char *name, *action;
	static unsigned int count=0, first=1;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	const char *path;
	int r;

	if (first) {
		// set up monitoring
		printf("TeensyControls_usb_init: set up udev monitoring\n");
		mon = udev_monitor_new_from_netlink(udev, "udev");
		udev_monitor_filter_add_match_subsystem_devtype(mon, "hidraw", NULL);
		udev_monitor_enable_receiving(mon);
		monfd = udev_monitor_get_fd(mon);
		// enumerate all currently attached devices
		printf("TeensyControls_usb_init: udev enumerate devices\n");
		enumerate = udev_enumerate_new(udev);
		udev_enumerate_add_match_subsystem(enumerate, "hidraw");
		udev_enumerate_scan_devices(enumerate);
		devices = udev_enumerate_get_list_entry(enumerate);
		udev_list_entry_foreach(dev_list_entry, devices) {
			path = udev_list_entry_get_name(dev_list_entry);
			//printf("path: %s\n", path);
			dev = udev_device_new_from_syspath(udev, path);
			if (dev) {
				new_usb_device(dev);
				udev_device_unref(dev);
			}
		}
		udev_enumerate_unref(enumerate);
		first = 0;
	}
	if (++count < 8) return; // only check every 8 frames
	count = 0;
	FD_ZERO(&fds);
	FD_SET(monfd, &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	r = select(monfd+1, &fds, NULL, NULL, &tv);
	if (r > 0 && FD_ISSET(monfd, &fds)) {
		dev = udev_monitor_receive_device(mon);
		if (dev) {
			name = udev_device_get_devnode(dev);
			action = udev_device_get_action(dev);
			printf("%s device %s\n", action, name);
			if (strcmp(action, "add") == 0) {
				new_usb_device(dev);
			}
			udev_device_unref(dev);
		}
	}
}


int TeensyControls_usb_init(void)
{
	printf("TeensyControls_usb_init\n");
	udev = udev_new();
	if (!udev) return 0;
	return 1;
}


void TeensyControls_usb_close(void)
{
	num_thread_alive();
	teensy_t *t;
	int wait=0;

	for (t = TeensyControls_first_teensy; t; t = t->next) {
		if (t->online) {
			printf("attempt to end any pending USB device I/O\n");
			t->online = 0;
			close(t->usb.fd);
			pthread_cond_signal(&t->output_event);
		}
	}
	// hopefully the threads will gracefully exit on their own?
	while (++wait < 8 && num_thread_alive() > 0) {
		usleep(10000);
		printf("wait #%d for thread exit\n", wait);
	}
	if (mon) {
		udev_monitor_unref(mon);
		mon = NULL;
	}
	if (udev) {
		udev_unref(udev);
		udev = NULL;
	}
	// TODO: violently kill any hung threads?
}







/*************************************************************************/
/**                                                                     **/
/**                               Windows                               **/
/**                                                                     **/
/*************************************************************************/

#elif defined(WINDOWS) || defined(WINDOWS64)

#ifdef USE_PRINTF_DEBUG
static const char * mesg(DWORD err)
{
	static char buf[128];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, buf, sizeof(buf), NULL);
	return buf;
}
#endif

static void input_thread(void *arg);
static void output_thread(void *arg);

static int device_scan_needed=1;

// DispatchMessage ultimately causes this callback to be called
LRESULT CALLBACK window_callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
        printf("callback\n");
        if (uMsg == WM_CREATE || uMsg == WM_SETFOCUS || uMsg == WM_SIZE) return 1;
        if (uMsg == WM_CLOSE || uMsg == WM_DESTROY) return 1;
        if (uMsg != WM_DEVICECHANGE) {
                return DefWindowProc(hWnd, uMsg, wParam, lParam);
        }
        printf("device change event\n");
        if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVNODES_CHANGED
          || wParam == DBT_DEVICEREMOVECOMPLETE) {
		printf("device scan needed\n");
                device_scan_needed = 1;
        }
        return 1;
}

static int is_already_open(const char *path)
{
	teensy_t *t;

	for (t = TeensyControls_first_teensy; t; t = t->next) {
		if (strncmp(t->usb.devpath, path, sizeof(t->usb.devpath)-1) == 0) {
			if (t->online) return 1;
		}
	}
	return 0;
}


void TeensyControls_find_new_usb_devices(void)
{
	GUID guid;
	HDEVINFO devset;
	DWORD index=0, reqd_size, bufsize=0;
	SP_DEVICE_INTERFACE_DATA iface;
	void *buf=NULL, *newbuf;
	SP_DEVICE_INTERFACE_DETAIL_DATA *details=NULL;
	HIDD_ATTRIBUTES attrib;
	PHIDP_PREPARSED_DATA hid_data;
	HIDP_CAPS capabilities;
	teensy_t *t;
	int len;
	HANDLE h;
	BOOL ret;

	if (!device_scan_needed) return;
	printf("begin device scan\n");
	device_scan_needed = 0;
	HidD_GetHidGuid(&guid);
	devset = SetupDiGetClassDevs(&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (devset == INVALID_HANDLE_VALUE) return;
	// maybe use SetupDiEnumDeviceInfo to traverse the set,
	// and SetupDiGetDeviceInstanceId to get the instance ID for each?
	// http://msdn.microsoft.com/en-us/library/windows/hardware/ff541247%28v=VS.85%29.aspx
	while (1) {
		//printf("begin scan, index = %ld\n", index);
		iface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		ret = SetupDiEnumDeviceInterfaces(devset, NULL, &guid, index, &iface);
		if (!ret) break;
		index++;

		// get the device path name
		SetupDiGetInterfaceDeviceDetail(devset, &iface, NULL, 0, &reqd_size, NULL);
		if (reqd_size > bufsize) {
			bufsize = reqd_size + 2000;
			newbuf = realloc(buf, bufsize);
			if (!newbuf) break;
			buf = newbuf;
		}
		details = (SP_DEVICE_INTERFACE_DETAIL_DATA *)buf;
		details->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		ret = SetupDiGetDeviceInterfaceDetail(devset, &iface, details,
			reqd_size, NULL, NULL);
		if (!ret) continue;
		if (is_already_open(details->DevicePath)) continue;
		//printf("path=%s\n", details->DevicePath);

		h = CreateFile(details->DevicePath, GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
		if (h == INVALID_HANDLE_VALUE) continue;
		attrib.Size = sizeof(HIDD_ATTRIBUTES);
		ret = HidD_GetAttributes(h, &attrib);
		if (!ret) goto next;
		if (attrib.VendorID != 0x16C0) goto next;
		if (attrib.ProductID != 0x0488) goto next;
		if (!HidD_GetPreparsedData(h, &hid_data)) goto next;
		if (!HidP_GetCaps(hid_data, &capabilities)) goto next2;
		if (capabilities.UsagePage != 0xFF1C) goto next2;
		if (capabilities.Usage != 0xA739) goto next2;
		printf(" vid = 0x%04X,", (int)(attrib.VendorID));
		printf(" pid = 0x%04X,",  (int)(attrib.ProductID));
		printf("page = 0x%04X,",  (int)(capabilities.UsagePage));
		printf(" use = 0x%04X\n",  (int)(capabilities.Usage));
		printf("path=%s\n\n", details->DevicePath);
		t = TeensyControls_new_teensy();
		if (t) {
			t->usb.handle = h;
			len = strlen(details->DevicePath);
			if (len >= sizeof(t->usb.devpath))
				len = sizeof(t->usb.devpath)-1;
			strncpy(t->usb.devpath, details->DevicePath, len);
			details->DevicePath[len] = 0;
			t->usb.rx_event = CreateEvent(NULL, TRUE, FALSE, NULL);
			t->usb.tx_event = CreateEvent(NULL, TRUE, FALSE, NULL);
			HidD_FreePreparsedData(hid_data);
			if (!thread_start(input_thread, t)) t->input_thread_quit = 1;
			if (!thread_start(output_thread, t)) t->output_thread_quit = 1;
			continue;
		}
		next2:
			HidD_FreePreparsedData(hid_data);
		next:
			CloseHandle(h);
	}
	if (buf) free(buf);
	SetupDiDestroyDeviceInfoList(devset);
}




static void input_thread(void *arg)
{
	teensy_t *t = (teensy_t *)arg;
	DWORD n;
	BOOL ret;
	uint8_t buf[65];

	printf("input_thread begin\n");
	while (t->online) {
		ResetEvent(&(t->usb.rx_event));
		memset(&(t->usb.rx_ov), 0, sizeof(t->usb.rx_ov));
		t->usb.rx_ov.hEvent = t->usb.rx_event;
		ret = ReadFile(t->usb.handle, buf, 65, &n, &(t->usb.rx_ov));
		if (ret) {
			if (n > 0) {
				t->usb.error_count = 0;
				TeensyControls_input_store(t, buf + 1);
			}
		} else {
			n = GetLastError();
			if (n == ERROR_IO_PENDING) {
				ret = GetOverlappedResult(t->usb.handle,
					&(t->usb.rx_ov), &n, TRUE);
				if (ret) {
					if (n > 0) {
						TeensyControls_input_store(t, buf + 1);
					}
				} else {
					if (n == ERROR_DEVICE_NOT_CONNECTED) {
						t->online = 0;
					}
				}
			} else {
				printf("ReadFile error %ld, %s\n", n, mesg(n));
				if (n == ERROR_DEVICE_NOT_CONNECTED) {
					t->online = 0;
				} else {
					if (++t->usb.error_count > 8) t->online = 0;
				}
			}
		}
	}
	t->input_thread_quit = 1;
	printf("input_thread end\n");
}


static void output_thread(void *arg)
{
	teensy_t *t = (teensy_t *)arg;
	uint8_t buf[65];
	int r;
	DWORD n;
	BOOL ret;

	printf("output_thread begin\n");
	while (1) {
		//printf("output_thread\n");
		if (t->online == 0) break;
		if (TeensyControls_output_fetch(t, buf + 1) && t->online) {
			//printf("output_thread: send\n");
			buf[0] = 0;
			ResetEvent(&(t->usb.tx_event));
			memset(&(t->usb.tx_ov), 0, sizeof(t->usb.tx_ov));
			t->usb.tx_ov.hEvent = t->usb.tx_event;
			ret = WriteFile(t->usb.handle, buf, 65, &n, &(t->usb.tx_ov));
			if (ret) {
				printf("WriteFile success\n");
				t->usb.error_count = 0;
			} else {
				n = GetLastError();
				if (n == ERROR_IO_PENDING) {
					ret = GetOverlappedResult(t->usb.handle,
						&(t->usb.tx_ov), &n, TRUE);
					if (ret) {
						t->usb.error_count = 0;
						//printf("WriteFile: GetOverlappedResult success, n=%ld\n", n);
					} else {
						printf("WriteFile: GetOverlappedResult failed: %s\n",
							mesg(GetLastError()));
					}
				} else if (n == ERROR_DEVICE_NOT_CONNECTED) {
					t->online = 0;
				} else {
					if (++t->usb.error_count > 8) t->online = 0;
				}
			}
		} else {
			//printf("output_thread, no data\n");
			pthread_mutex_lock(&t->output_mutex);
			if (t->output_head == t->output_tail) {
				//printf("output_thread, begin wait\n");
				t->output_thread_waiting = 1;
				r = pthread_cond_wait_timeout(&t->output_event,
					&t->output_mutex, 1000);
				t->output_thread_waiting = 0;
				//printf("output_thread, r: %d, errno: %d\n", r, errno);
			}
			pthread_mutex_unlock(&t->output_mutex);
		}
	}
	t->output_thread_quit = 1;
	printf("output_thread end\n");
}


// called from main thread
int TeensyControls_usb_init(void)
{
	GUID hid_guid;
	WNDCLASS window;
	DEV_BROADCAST_DEVICEINTERFACE notification_filter;
	HWND hWnd;
	HDEVNOTIFY newdev;
	LPCTSTR name = TEXT("TeensyControls Plugin");

	// windows will only deliver device change notifications to windows
	// or services... so this yucky code creates a dummy window.
	memset(&window, 0, sizeof(window));
	window.style = CS_NOCLOSE;
	window.lpfnWndProc = window_callback;
	window.cbClsExtra = 0;
	window.cbWndExtra = 0;
	window.hInstance = GetModuleHandle(NULL);
	window.hIcon = NULL;
	window.hCursor = LoadCursor(0, IDC_ARROW);
	window.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	window.lpszMenuName = NULL;
	window.lpszClassName = name;
	if (!RegisterClass(&window)) return 0; // unable to register notification window
	hWnd = CreateWindow(name, NULL, 0, 50, 50, 50, 50, NULL, NULL, NULL, NULL);
	printf("hWnd = %ld\n", (long)hWnd);
	if (!hWnd) return 0; // unable to create notification window
	//ShowWindow(hWnd, SW_SHOWNORMAL);
	//UpdateWindow(hWnd);

        // now we can ask for notifications
        HidD_GetHidGuid(&hid_guid);
        memset(&notification_filter, 0, sizeof(notification_filter));
        notification_filter.dbcc_size = sizeof(notification_filter);
        notification_filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        notification_filter.dbcc_classguid = hid_guid;
        newdev = RegisterDeviceNotification(hWnd, &notification_filter, 0);
        if (!newdev) return 0;  // unable to register device notification
	return 1;
}


void TeensyControls_usb_close(void)
{
	teensy_t *t;
	int wait=0;

	for (t = TeensyControls_first_teensy; t; t = t->next) {
		if (t->online) {
			printf("attempt to end any pending USB device I/O\n");
			t->online = 0;
			pthread_cond_signal(&t->output_event);
			//Sadly, CancelIoEx only exists in Vista and later
			//CancelIoEx(t->usb.handle, NULL);
			//instead, let's try something incredibly ugly.
			//set both events, which hopefully will cause any
			//GetOverlappedResult to end with an error
			SetEvent(t->usb.rx_event);
			SetEvent(t->usb.tx_event);
		}
	}
	// TODO: destroy notification window, unregister notification?
	// hopefully the threads will gracefully exit on their own?
	while (++wait < 5 && num_thread_alive() > 0) {
		Sleep(10);
		printf("wait #%d for thread exit\n", wait);
	}
	// forcibly close everything
	for (t = TeensyControls_first_teensy; t; t = t->next) {
		printf("close USB device\n");
		CloseHandle(t->usb.rx_event);
		CloseHandle(t->usb.tx_event);
		CloseHandle(t->usb.handle);
	}
	// if the threads didn't exit, maybe they will now?
	wait = 0;
	while (++wait < 10 && num_thread_alive() > 0) {
		Sleep(10);
		printf("wait #%d for thread exit\n", wait);
	}
	// TODO: violently kill any hung threads?
}








#endif



