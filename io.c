#include "TeensyControls.h"


static void input_packet(teensy_t *t, const uint8_t *packet);
static int  output_data(teensy_t *t, const uint8_t *data, int datalen);
static void output_flush(teensy_t *t);


// process all buffered input
// elapsed is time in seconds since previous input
// flags = 1 upon enable event
// flags = 2 upon disable event
//
void TeensyControls_input(float elapsed, int flags)
{
	teensy_t *t;
	uint8_t packet[64];

	for (t = TeensyControls_first_teensy; t; t = t->next) {
		while (TeensyControls_input_fetch(t, packet)) {
			input_packet(t, packet);
		}
	}
}


static float bytes2float(const void *ptr)
{
	union {
		uint8_t b[4];
		float f;
	} u;
	memcpy(u.b, ptr, 4);
	return u.f;
}

static int32_t bytes2in32(const void *ptr)
{
	union {
		uint8_t b[4];
		int32_t i;
	} u;
	memcpy(u.b, ptr, 4);
	return u.i;
}


static void input_packet(teensy_t *t, const uint8_t *packet)
{
	int i, len, cmd, id, type;
	item_t *item;
	int32_t intval;
	float floatval;
	char *name;
#if 1
	printf("Input:");
	for (i=0; i<24; i++) {
		printf(" %02X", packet[i]);
	}
	printf("\n");
#endif
	i = 0;
	do {
		len = packet[i];
		if (len < 2 || len > 64 - i) return;
		cmd = packet[i + 1];
		switch (cmd) {

		  case 0x01: // register command or data
			if (len < 7) break;
			id = packet[i + 2] | (packet[i + 3] << 8);
			type = packet[i + 4];
			name = (char *)(packet + i + 6);
			TeensyControls_new_item(t, id, type, name, len - 6);
			break;

		  case 0x02: // write data data
			if (len < 10) break;
			id = packet[i + 2] | (packet[i + 3] << 8);
			type = packet[i + 4];
			item = TeensyControls_find_item(t, id);
			if (!item) {
				t->unknown_id_heard = 1;
				break;
			}
			if (item->type != type || item->datawritable == 0) break;
			intval = packet[i + 6] | (packet[i + 7] << 8)
			  | (packet[i + 8] << 16) | (packet[i + 9] << 24);
			if (type == 1) { // integer
				item->intval = intval;
				item->intval_remote = intval;
				item->changed_by_teensy = 1;
			} else if (type == 2) { // float
				//floatval = *(float *)((char *)(&intval));
				floatval = bytes2float(&intval);
				item->floatval = floatval;
				item->floatval_remote = floatval;
				item->changed_by_teensy = 1;
			}
			break;

		  case 0x04: // command begin
			if (len < 4) break;
			id = packet[i + 2] | (packet[i + 3] << 8);
			item = TeensyControls_find_item(t, id);
			if (!item) {
				t->unknown_id_heard = 1;
				break;
			}
			if (item->type != 0 || item->command_count >= 128) break;
			item->command_queue[item->command_count++] = cmd;
			printf("Command Begin: id=%d, name=%s\n", id, item->name);
			break;

		  case 0x05: // command end
			if (len < 4) break;
			id = packet[i + 2] | (packet[i + 3] << 8);
			item = TeensyControls_find_item(t, id);
			if (!item) {
				t->unknown_id_heard = 1;
				break;
			}
			if (item->type != 0 || item->command_count >= 128) break;
			item->command_queue[item->command_count++] = cmd;
			printf("Command End: id=%d, name=%s\n", id, item->name);
			break;

		  case 0x06: // command once
			if (len < 4) break;
			id = packet[i + 2] | (packet[i + 3] << 8);
			item = TeensyControls_find_item(t, id);
			if (!item) {
				t->unknown_id_heard = 1;
				break;
			}
			if (item->type != 0 || item->command_count >= 128) break;
			item->command_queue[item->command_count++] = cmd;
			printf("Command Once: id=%d, name=%s\n", id, item->name);
			break;
		}
		i += len;
	} while (i < 64);
}


void TeensyControls_update_xplane(float elapsed)
{
	teensy_t *t;
	item_t *item;
	int i, count;
	float f;

	// step 1: do all commands
	for (t = TeensyControls_first_teensy; t; t = t->next) {
		for (item = t->items; item; item = item->next) {
			if (item->type != 0) continue;
			count = item->command_count;
			for (i = 0; i < count; i++) {
				switch (item->command_queue[i]) {
				  case 0x04: // command begin
					XPLMCommandBegin(item->cmdref);
					display("Command %s Begin", item->name);
					item->command_began = 1;
					break;
				  case 0x05: // command end
					XPLMCommandEnd(item->cmdref);
					display("Command %s End", item->name);
					item->command_began = 0;
					break;
				  case 0x06: // command once
					XPLMCommandOnce(item->cmdref);
					display("Command %s Once", item->name);
				}
			}
			item->command_count = 0;
		}
	}
	// step 2: write any data Teensy changed
	for (t = TeensyControls_first_teensy; t; t = t->next) {
		for (item = t->items; item; item = item->next) {
			if (item->type == 1 && item->changed_by_teensy) {
				item->changed_by_teensy = 0;
				display("Write %s = %d", item->name, item->intval);
				if (item->datatype & xplmType_Int) {
					XPLMSetDatai(item->dataref, item->intval);
				} else if (item->datatype & xplmType_Float) {
					XPLMSetDataf(item->dataref, (float)(item->intval));
				} else if (item->datatype & xplmType_Double) {
					XPLMSetDatad(item->dataref, (double)(item->intval));
				} else if (item->datatype & xplmType_IntArray) {
					i = item->intval;
					XPLMSetDatavi(item->dataref, &i, item->index, 1);
				} else if (item->datatype & xplmType_FloatArray) {
					f = (float)(item->intval);
					XPLMSetDatavf(item->dataref, &f, item->index, 1);
				}
			} else if (item->type == 2 && item->changed_by_teensy) {
				item->changed_by_teensy = 0;
				display("Write %s = %f", item->name, item->floatval);
				printf("float, write %s = %f\n", item->name, item->floatval);
				if (item->datatype & xplmType_Float) {
					XPLMSetDataf(item->dataref, item->floatval);
				} else if (item->datatype & xplmType_Double) {
					XPLMSetDatad(item->dataref, (double)(item->floatval));
				} else if (item->datatype & xplmType_Int) {
					XPLMSetDatai(item->dataref, (int)(item->floatval));
				} else if (item->datatype & xplmType_FloatArray ) {
					f = item->floatval;
					XPLMSetDatavf(item->dataref, &f, item->index, 1);
				} else if (item->datatype & xplmType_IntArray) {
					i = (int)(item->floatval);
					XPLMSetDatavi(item->dataref, &i, item->index, 1);
				}
			}
		}
	}
	// step 3: read all data from simulator
	for (t = TeensyControls_first_teensy; t; t = t->next) {
		for (item = t->items; item; item = item->next) {
			// TODO: perhaps keep record of elapsed time since last update
			// so we don't call the XPLMGetDataX functions so quickly?
			// Then again, these functions seem to be very efficient, so
			// perhaps rapid lowest latency update to Teensy is best?
			switch (item->type) {
			  case 0x01: // integer
				if (item->datatype & xplmType_Int) {
					item->intval = XPLMGetDatai(item->dataref);
				} else if (item->datatype & xplmType_Double) {
					item->intval = (int32_t)XPLMGetDatad(item->dataref);
				} else if (item->datatype & xplmType_Float) {
					item->intval = (int32_t)XPLMGetDataf(item->dataref);
				} else if (item->datatype & xplmType_IntArray) {
					XPLMGetDatavi(item->dataref, &i, item->index, 1);
					item->intval = (int32_t)i;
				} else if (item->datatype & xplmType_FloatArray) {
					XPLMGetDatavf(item->dataref, &f, item->index, 1);
					item->intval = (int32_t)f;
				} else break;
				if (item->intval != item->intval_remote) {
					printf("change: %s = %d\n", item->name, item->intval);
					display("Update %s = %d", item->name, item->intval);
				}
				break;
			  case 0x02: // float
				if (item->datatype & xplmType_Float) {
					item->floatval = XPLMGetDataf(item->dataref);
					//printf("float as float, type=0x%X\n", item->datatype);
				} else if (item->datatype & xplmType_Double) {
					item->floatval = (float)XPLMGetDatad(item->dataref);
					//printf("float as double\n");
				} else if (item->datatype & xplmType_Int) {
					item->floatval = (float)XPLMGetDatai(item->dataref);
					//printf("float as int\n");
				} else if (item->datatype & xplmType_FloatArray) {
					XPLMGetDatavf(item->dataref, &f, item->index, 1);
					item->floatval = f;
					//printf("float as float array\n");
				} else if (item->datatype & xplmType_IntArray) {
					XPLMGetDatavi(item->dataref, &i, item->index, 1);
					item->floatval = (float)i;
					//printf("float as int array\n");
				} else break;
				//printf("float: %s = %f\n", item->name, item->floatval);
				if (item->floatval != item->floatval_remote) {
					printf("change: %s = %f\n", item->name, item->floatval);
					display("Update %s = %f", item->name, item->floatval);
				}
			}
		}
	}
}


// output any items where our copy is different than Teensy's remote copy
// elapsed is time in seconds since previous output
// flags = 1 upon enable event
// flags = 2 upon disable event
//
void TeensyControls_output(float elapsed, int flags)
{
	teensy_t *t;
	item_t *item;
	uint8_t buf[10], enable_state=2, en;
	int32_t i32;

	if (flags == 1) {
		enable_state = 1;
	} else if (flags == 2) {
		enable_state = 3;
	}
	for (t = TeensyControls_first_teensy; t; t = t->next) {
		en = enable_state;
		if (en == 2 && t->unknown_id_heard) {
			en = 1;
			t->unknown_id_heard = 0;
		}
		buf[0] = 4;
		buf[1] = 3;
		buf[2] = en;
		buf[3] = 0;
		if (en == 1) printf("Output: enable and send IDs\n");
		if (en == 3) printf("Output: disable\n");
		if (!output_data(t, buf, 4)) break;

		for (item = t->items; item; item = item->next) {
			if (item->type == 1 && item->intval != item->intval_remote) {
				printf("output: %s = %d\n", item->name, item->intval);
				i32 = item->intval;
				buf[0] = 10;			// length
				buf[1] = 2;			// 2 = write data
				buf[2] = item->id & 255;	// ID
				buf[3] = item->id >> 8;		// ID
				buf[4] = 1;			// type, 1 = integer
				buf[5] = 0;			// reserved
				buf[6] = i32 & 255;
				buf[7] = (i32 >> 8) & 255;
				buf[8] = (i32 >> 16) & 255;
				buf[9] = (i32 >> 24) & 255;
				if (!output_data(t, buf, 10)) break;
				item->intval_remote = item->intval;
			} else if (item->type == 2 && item->floatval != item->floatval_remote) {
				printf("output: %s = %f\n", item->name, item->floatval);
				//i32 = *(int32_t *)((char *)(&(item->floatval)));
				i32 = bytes2in32(&(item->floatval));
				buf[0] = 10;			// length
				buf[1] = 2;			// 2 = write data
				buf[2] = item->id & 255;	// ID
				buf[3] = item->id >> 8;		// ID
				buf[4] = 2;			// type, 2 = float
				buf[5] = 0;			// reserved
				buf[6] = i32 & 255;
				buf[7] = (i32 >> 8) & 255;
				buf[8] = (i32 >> 16) & 255;
				buf[9] = (i32 >> 24) & 255;
				if (!output_data(t, buf, 10)) break;
				item->floatval_remote = item->floatval;
			}
		}
		output_flush(t);
	}
}

static void output_packet(teensy_t *t)
{
	int len = t->output_packet_len;
	if (len < 64) memset(t->output_packet + len, 0, 64 - len);
	TeensyControls_output_store(t, t->output_packet);
	t->output_packet_len = 0;
}

static int output_data(teensy_t *t, const uint8_t *data, int datalen)
{
	if (!data || datalen <= 0 || datalen > 64) return 0;
	if (t->output_packet_len + datalen > 64) output_packet(t);
	memcpy(t->output_packet + t->output_packet_len, data, datalen);
	t->output_packet_len += datalen;
	return 1;
}

static void output_flush(teensy_t *t)
{
	if (t->output_packet_len > 0) output_packet(t);
}











