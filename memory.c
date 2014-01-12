#include "TeensyControls.h"

// list of all Teensy boards
teensy_t * TeensyControls_first_teensy = NULL;

// orphaned commands (if Teensy removal while command begin without end)
//static int orphaned_count = 0;
//static XPLMCommandRef orphaned_list[256];

// called from any thread
teensy_t * TeensyControls_new_teensy(void)
{
	teensy_t *n, *p;

	n = (teensy_t *)malloc(sizeof(teensy_t));
	if (!n) return NULL;
	memset(n, 0, sizeof(teensy_t));
	display("Teensy Detected");
	printf("new teensy\n");
	n->online = 1;
	n->unknown_id_heard = 1;
	n->next = NULL;
	pthread_mutex_init(&n->input_mutex, NULL);
	pthread_mutex_init(&n->output_mutex, NULL);
	pthread_cond_init(&n->output_event, NULL);
	if (TeensyControls_first_teensy == NULL) {
		TeensyControls_first_teensy = n;
	} else {
		for (p = TeensyControls_first_teensy; p->next; p = p->next) ;
		p->next = n;
	}
	return n;
}

// always called from main thread
static void delete_teensy(teensy_t *t)
{
	teensy_t *p, *q=NULL;
	item_t *item, *nitem;

	display("Teensy Removed");
	for (p = TeensyControls_first_teensy; p; p = p->next) {
		if (p == t) {
			if (q) {
				q->next = p->next;
			} else {
				TeensyControls_first_teensy = p->next;
			}
			for (item = p->items; item; item = nitem) {
				if (item->type == 0 && item->command_began) {
					XPLMCommandEnd(item->cmdref);
					item->command_began = 0;
				}
				nitem = item->next;
				free(item);
			}
			pthread_mutex_destroy(&p->input_mutex);
			pthread_mutex_destroy(&p->output_mutex);
			pthread_cond_destroy(&p->output_event);
			free(p);
			return;
		}
		q = p;
	}
}

// always called from main thread
void TeensyControls_delete_offline_teensy(void)
{
	int anydeleted;
	teensy_t *t;

	// delete any Teensy that are not online
	do {
		anydeleted = 0;
		for (t = TeensyControls_first_teensy; t; t = t->next) {
			if (!t->online && t->input_thread_quit && t->output_thread_quit) {
				delete_teensy(t);
				anydeleted = 1;
				break;
			}
		}
	} while (anydeleted);
}



void TeensyControls_input_store(teensy_t *t, const uint8_t *packet)
{
	int head;
	pthread_mutex_lock(&t->input_mutex);
	head = t->input_head;
	if (++head >= INPUT_BUFSIZE) head = 0;
	if (head != t->input_tail) {
		memcpy(t->input_buffer + head * 64, packet, 64);
		t->input_head = head;
	}
	pthread_mutex_unlock(&t->input_mutex);
}

int  TeensyControls_input_fetch(teensy_t *t, uint8_t *packet)
{
	int tail;
	pthread_mutex_lock(&t->input_mutex);
	tail = t->input_tail;
	if (tail == t->input_head) {
		pthread_mutex_unlock(&t->input_mutex);
		return 0;
	}
	if (++tail >= INPUT_BUFSIZE) tail = 0;
	memcpy(packet, t->input_buffer + tail * 64, 64);
	t->input_tail = tail;
	pthread_mutex_unlock(&t->input_mutex);
	return 1;
}

void TeensyControls_output_store(teensy_t *t, const uint8_t *packet)
{
	int head;
	pthread_mutex_lock(&t->output_mutex);
	head = t->output_head;
	if (++head >= OUTPUT_BUFSIZE) head = 0;
	if (head != t->output_tail) {
		memcpy(t->output_buffer + head * 64, packet, 64);
		t->output_head = head;
	}
	if (t->output_thread_waiting) {
		pthread_cond_signal(&t->output_event);
	}
	pthread_mutex_unlock(&t->output_mutex);
}

int  TeensyControls_output_fetch(teensy_t *t, uint8_t *packet)
{
	int tail;
	pthread_mutex_lock(&t->output_mutex);
	tail = t->output_tail;
	if (tail == t->output_head) {
		pthread_mutex_unlock(&t->output_mutex);
		return 0;
	}
	if (++tail >= OUTPUT_BUFSIZE) tail = 0;
	memcpy(packet, t->output_buffer + tail * 64, 64);
	t->output_tail = tail;
	pthread_mutex_unlock(&t->output_mutex);
	return 1;
}







// finds any "[##]" suffix on the string, returns the index, and removes it
// from the string.  If none is found, -1 is returns and the string unchanged.
//
static int parse_array_index(char *str)
{
	int len, index=0;
	char *p;

	if (!str) return 0;
	len = strlen(str);
	if (len < 3) return 0;
	p = str + len - 1;
	if (*p != ']') return 0;
	p--;
	while (*p >= '0' && *p <= '9' && p > str) p--;
	if (p == str) return 0;
	if (*p != '[') return 0;
	if (sscanf(p + 1, "%d", &index) != 1) return 0;
	if (index < 0) return 0;
	*p = 0;
	return index;
}

void TeensyControls_new_item(teensy_t *t, int id, int type, const char *name, int namelen)
{
	item_t *item;
	char str[64];
	XPLMCommandRef cmdref = NULL;
	XPLMDataRef dataref = NULL;
	XPLMDataTypeID datatype = 0;
	int index, datawritable = 0;

	if (!t || !name || namelen >= 64) return;
	memcpy(str, name, namelen);
	str[namelen] = 0;
	index = parse_array_index(str);
	if (type == 0) {
		cmdref = XPLMFindCommand(str);
		if (!cmdref) {
			display("Teensy requested command %s does not exist", str);
			return;
		}
	} else {
		dataref = XPLMFindDataRef(str);
		if (!dataref) {
			display("Teensy request data %s does not exist", str);
			return;
		}
		datatype = XPLMGetDataRefTypes(dataref);
		datawritable = XPLMCanWriteDataRef(dataref);
	}
	item = TeensyControls_find_item(t, id);
	if (item) {
		// TODO: item exists - check if our data is correct
	} else {
		item = (item_t *)malloc(sizeof(item_t));
		if (!item) return;
		printf("new item, id = %d, type = %d, name = %s\n", id, type, name);
		memset(item, 0, sizeof(item_t));
		item->id = id;
		item->type = type;
		item->index = index;
		item->cmdref = cmdref;
		item->dataref = dataref;
		item->datatype = datatype;
		item->datawritable = datawritable;
		strcpy(item->name, str);
		item->next = t->items;
		t->items = item;
	}
}

item_t * TeensyControls_find_item(teensy_t *t, int id)
{
	item_t *p;

	if (!t) return NULL;
	for (p = t->items; p; p = p->next) {
		if (p->id == id) return p;
	}
	return NULL;
}

