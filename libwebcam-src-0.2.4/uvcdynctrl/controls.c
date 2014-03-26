/*
 * uvcdynctrl - Manage dynamic controls in uvcvideo
 *
 *
 * Copyright (c) 2006-2007 Logitech.
 *
 * This file is part of uvcdynctrl.
 * 
 * uvcdynctrl is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * uvcdynctrl is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with uvcdynctrl.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>


#include <webcam.h>

#include "controls.h"

/*
 * Convert a array of bytes from big endian to little endian and vice versa by inverting it
 */
static
uint8_t *raw_inv(uint8_t *data, int size) {

	int ai = 0;
	int bi = size - 1;
	uint8_t a = 0;
	uint8_t b = 0;
	
	while (ai < bi) {
		a = data[ai];
		b = data[bi];
		
		data[ai] = b;
		data[bi] = a;
		ai++;
		bi--;
	}
	
	return data;
}

/*
 * Convert char array to 1 byte
 */
static 
uint8_t convert_byte(char str[2], int base) {
	uint8_t val = 0;
	int i = 0;
	uint8_t mult[2] = {1, base};
	
	if (base == 16)
	{
		for(i=0; i<2; i++) {
			if (isdigit(str[i])) {
				val += (str[i] - '0') * mult[i];
			}
			else if (isalpha(str[i]) && (toupper(str[i]) >= 'A') && (toupper(str[i]) <= 'F')) {
				val += (10 + (toupper(str[i]) - 'A')) * mult[i];
			}
			else
				break;
		}
	}
	else //base 10
	{
		printf("base value not supported (only base 16)\n");
	}
	
	return val;
}

static 
int convert_raw_string(void *raw_data, int max_size, char raw_str[]) {
	int i = 0;
	int endian = 0; //0=le; 1=be
	int base = 16;
	int start_i = 0;
	int data_index = 0;
	uint8_t *data = (uint8_t *) raw_data;
	//convert raw_data string
	int length = strlen(raw_str);
	
	//check endianess
	if((length > 4) && raw_str[start_i] == '(' && raw_str[start_i+3] == ')') {
		if (isalpha(raw_str[start_i+1]) && isalpha(raw_str[start_i+2]) &&
		    toupper(raw_str[start_i+1]) == 'B' && toupper(raw_str[start_i+2]) == 'E') {
				endian = 1;
		}
		
		start_i = 4;
	}	
	
	if((length > start_i + 1) && raw_str[start_i] == '0' && isalpha(raw_str[start_i+1]) && (toupper(raw_str[start_i+1]) == 'X')) { //hex
		base = 16;
		start_i += 2;
	}
	else { //we assume value data is in hex format
		printf("Assuming hex value (base 16)\n");
		base = 16;
	}
	
	char str[2];
	
	for(i=start_i; i<length; i++) {
		if(i+1 < length)
		{
			str[1] = raw_str[i];
			str[0] = raw_str[i+1];
			i++;
		}
		else
		{
			str[1] = 0;
			str[0] = raw_str[i];
		}
		
		data[data_index] = convert_byte(str, base);
		data_index++;
	}
	
	if(endian > 0) {
		//convert from big endian to little endian
		data = raw_inv(data, data_index);
	}
	
	// size in bytes
	return (data_index);
}


CResult
get_control_list (CHandle handle, CControl **controls, unsigned int *count)
{
	CResult ret;
	unsigned int req_size = 0, buffer_size = 0, local_count = 0;

	assert(*controls == NULL);

	do {
		// Allocate the required memory
		if(*controls) free(*controls);
		if(req_size) {		// No allocation the first time
			*controls = (CControl *)malloc(req_size);
			if(*controls == NULL) {
				ret = C_NO_MEMORY;
				goto done;
			}
			buffer_size = req_size;
		}

		// Try to enumerate. If the buffer is not large enough, the required
		// size is returned.
		ret = c_enum_controls(handle, *controls, &req_size, &local_count);
		if(ret != C_SUCCESS && ret != C_BUFFER_TOO_SMALL)
			goto done;
	}
	while(buffer_size < req_size);

	if(count)
		*count = local_count;

done:
	if(ret) {
		if(*controls) free(*controls);
		*controls = NULL;
	}
	return ret;
}


CControlId
get_control_id (CHandle handle, const char *name)
{
	CControlId id = 0;
	CResult res;
	unsigned int count = 0;
	CControl *controls = NULL;
	int i;

	assert(name);

	// Retrieve the control list
	res = get_control_list(handle, &controls, &count);
	if(res) goto done;

	// Look for a control with the given name and return its ID
	for(i = 0; i < count; i++) {
		CControl *control = &controls[i];
		if(strcasecmp(name, control->name) == 0) {
			id = control->id;
			goto done;
		}
	}

done:
	if(controls) free(controls);
	return id;
}


int
parse_control_value (const char *string, CControlValue *value)
{
	assert(string);
	assert(value);
	
	if(strcasecmp(string, "true") == 0 ||
	   strcasecmp(string, "on") == 0 ||
	   strcasecmp(string, "yes") == 0) {
		value->value = 1;
		return 0;
	}
	else if(strcasecmp(string, "false") == 0 ||
	   strcasecmp(string, "off") == 0 ||
	   strcasecmp(string, "no") == 0) {
		value->value = 0;
		return 0;
	}

	value->value = atoi(string);
	return 0;
}

int
parse_raw_control_value (char *string, CControlValue *value)
{
	assert(string);
	assert(value);
	
	value->type = CC_TYPE_RAW;
	/* hex format:
	 * 1/2 chars => 1 byte e.g. 0xF = 0x0F
	 * 3/4 chars => 2 bytes e.g. 0xFFF = 0x0FFF
	 */
	value->raw.size = strlen(string)/2 + 1; //at most length/2 + 1
	value->raw.data = malloc(value->raw.size);
	//get the real value
	value->raw.size = convert_raw_string(value->raw.data, value->raw.size, string);
	
	return (value->raw.size);
}
