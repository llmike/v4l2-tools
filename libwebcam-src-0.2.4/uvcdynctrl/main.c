/*
 * uvcdynctrl - Manage dynamic controls in uvcvideo
 *
 *
 * Copyright (c) 2006-2010 Logitech.
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
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <fnmatch.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/types.h>

#define __user
#include <linux/media.h>

#include "config.h"
#include "cmdline.h"
#include "controls.h"


static struct gengetopt_args_info args_info;


#define ARRAY_SIZE(a)		(sizeof(a) / sizeof((a)[0]))
#define HAS_VERBOSE()		(args_info.verbose_given)


static void
print_handle_error (CHandle hDevice, const char *error, CResult res)
{
	if((int)res < 0) {
		printf("ERROR: %s.\n", error);
	}
	else {
		char *text = c_get_handle_error_text(hDevice, res);
		if(text) {
			printf("ERROR: %s: %s. (Code: %d)\n", error, text, res);
			free(text);
		}
		else {
			printf("ERROR: %s: Unknown error (Code: %d)\n", error, res);
		}
	}
}


static void
print_error (const char *error, CResult res)
{
	print_handle_error(0, error, res);
}


static
const char *get_control_type (CControlType type)
{
	switch(type) {
		case CC_TYPE_RAW:		return "Raw";
		case CC_TYPE_BOOLEAN:	return "Boolean";
		case CC_TYPE_CHOICE:	return "Choice";
		case CC_TYPE_BYTE:		return "Byte";
		case CC_TYPE_WORD:		return "Word";
		case CC_TYPE_DWORD:		return "Dword";
		case CC_TYPE_BUTTON:	return "Button";
		case CC_TYPE_CLASS:	return "Class";
		default:
			return "<Unknown>";
	}
}


// Note: The caller must free the buffer obtained from this function.
static char *
get_control_flags (CControlFlags flags)
{
	int i;

	const char *delimiter = ", ";
	const char *names[] = {
		"CAN_READ",
		"CAN_WRITE",
		"CAN_NOTIFY",
		"<Unknown>",
		"<Unknown>",
		"<Unknown>",
		"<Unknown>",
		"<Unknown>",
		"IS_CUSTOM",
		"IS_RELATIVE",
		"IS_ACTION",
	};
	char buffer[14 * ARRAY_SIZE(names)];		// 14 = maximum name length + strlen(delimiter)
	memset(buffer, 0, sizeof(buffer));

	for(i = 0; i < ARRAY_SIZE(names); i++) {
		if(flags & (1 << i)) {
			strcat(buffer, names[i]);
			if(flags >= (1 << (i + 1)))
				strcat(buffer, delimiter);
		}
	}

	return strdup(buffer);
}


// Note: The caller must free the buffer obtained from this function.
static char *
get_control_choices (CControl *control)
{
	const char *delimiter = ", ";
	char *buffer;
	int i;

	unsigned int req_size = 0;
	unsigned int count = control->choices.count;
	for(i = 0; i < count; i++) {
		req_size += 1 + strlen(control->choices.list[i].name) + 1 + 12;
		if(i < count - 1)
			req_size += strlen(delimiter);
	}

	buffer = (char *)malloc(req_size + 1);
	memset(buffer, 0, req_size + 1);

	for(i = 0; i < control->choices.count; i++) {
		char index_buffer[13];
		sprintf(index_buffer, "[%u]", control->choices.list[i].index);

		strcat(buffer, "'");
		strcat(buffer, control->choices.list[i].name);
		strcat(buffer, "'");
		strcat(buffer, index_buffer);

		if(i < count - 1)
			strcat(buffer, delimiter);
	}

	return buffer;
}


static void
print_control (CControl *control)
{
	if(HAS_VERBOSE()) {
		char *flags = get_control_flags(control->flags);

		printf("  %s\n    ID      : 0x%08x,\n    Type    : %s,\n    Flags   : { %s },\n", control->name,
				control->id, get_control_type(control->type), flags);

		if(control->type == CC_TYPE_CHOICE) {
			char *choices = get_control_choices(control);
			printf("    Values  : { %s },\n    Default : %d\n", choices, control->def.value);
			free(choices);
		}
		else {
			printf("    Values  : [ %d .. %d, step size: %d ],\n    Default : %d\n",
					control->min.value, control->max.value, control->step.value,
					control->def.value);
		}

		free(flags);
	}
	else {
		printf("  %s\n", control->name);
	}

}


static void
print_device (CDevice *device)
{
	if(HAS_VERBOSE()) {
		printf("  %s   %s [%s, %s]\n",
				device->shortName, device->name, device->driver, device->location);
	}
	else {
		printf("  %s   %s\n", device->shortName, device->name);
	}
}

static int file_exists (char * fileName)
{
	int result = access (fileName, F_OK); 
   
	/* File found */
	if ( result == 0 )
    {
    	return 1;
    }
    return 0;   
}

static CResult
save_controls (CHandle hDevice, const char* filename)
{
    int ret = C_SUCCESS;
    ret = c_save_controls (hDevice, filename);
    return ret;
}

static CResult
load_controls (CHandle hDevice, const char* filename)
{
    int ret = C_SUCCESS;
	ret = c_load_controls (hDevice, filename);
    return ret;
}

static CResult
list_controls (CHandle hDevice)
{
	CResult ret;
	unsigned int count = 0;
	CControl *controls = NULL;
	int i;

	// Retrieve the control list
	ret = get_control_list(hDevice, &controls, &count);
	if(ret) goto done;

	if(count == 0) {
		printf("No controls found.\n");
		goto done;
	}

	// Print a list of all available controls
	for(i = 0; i < count; i++) {
		CControl *control = &controls[i];
		print_control(control);
	}

done:
	if(controls) free(controls);
	if(ret)
		print_handle_error(hDevice, "Unable to retrieve device list", ret);
	return ret;
}


static CResult
list_frame_intervals (CHandle hDevice, CPixelFormat *pixelformat, CFrameSize *framesize)
{
	CResult ret = C_SUCCESS;
	CFrameInterval *intervals = NULL;
	unsigned int size = 0, count = 0;
	unsigned int i;

	ret = c_enum_frame_intervals(hDevice, pixelformat, framesize, NULL, &size, &count);
	if(ret == C_BUFFER_TOO_SMALL) {
		intervals = (CFrameInterval *)malloc(size);
		ret = c_enum_frame_intervals(hDevice, pixelformat, framesize, intervals, &size, &count);
		if(ret) {
			print_handle_error(hDevice, "Unable to enumerate frame intervals", ret);
			goto done;
		}

		// Display all frame intervals
		if(HAS_VERBOSE()) {
			// Verbose: one line per interval
			for(i = 0; i < count; i++) {
				CFrameInterval *fival = &intervals[i];

				if(fival->type == CF_INTERVAL_DISCRETE) {
					printf("    Frame interval: %u/%u [s]\n", fival->n, fival->d);
				}
				else if(fival->type == CF_INTERVAL_CONTINUOUS) {
					printf("    Frame intervals: %u/%u - %u/%u [s] (continuous)\n",
						fival->min_n, fival->min_d,
						fival->max_n, fival->max_d
					);
				}
				else if(fival->type == CF_INTERVAL_STEPWISE) {
					printf("    Frame intervals: %u/%u - %u/%u [s] (in steps of %u/%u [s])\n",
						fival->min_n, fival->min_d,
						fival->max_n, fival->max_d,
						fival->step_n, fival->step_d
					);
				}
				else {
					print_handle_error(hDevice, "Unrecognized frame interval type", -1);
				}
			}
		}
		else {
			// Determine how concise we can be
			int simple = 1;
			for(i = 0; i < count; i++) {
				if(intervals[i].type != CF_INTERVAL_DISCRETE || intervals[i].n != 1) {
					simple = 0;
					break;
				}
			}

			// Not verbose
			if(simple) {
				printf("    Frame rates: ");
				for(i = 0; i < count; i++) {
					printf("%u%s", intervals[i].d, i < count - 1 ? ", " : "");
				}
				printf("\n");
			}
			else {
				printf("    Frame intervals: ");
				for(i = 0; i < count; i++) {
					CFrameInterval *fival = &intervals[i];

					if(fival->type == CF_INTERVAL_DISCRETE) {
						printf("%u/%u", fival->n, fival->d);
					}
					else if(fival->type == CF_INTERVAL_CONTINUOUS) {
						printf("%u/%u - %u/%u",
							fival->min_n, fival->min_d,
							fival->max_n, fival->max_d
						);
					}
					else if(fival->type == CF_INTERVAL_STEPWISE) {
						printf("%u/%u - %u/%u (%u/%u)",
							fival->min_n, fival->min_d,
							fival->max_n, fival->max_d,
							fival->step_n, fival->step_d
						);
					}
					else {
						printf("<?>");
					}
					if(i < count - 1)
						printf(", ");
				}
				printf("\n");
			}
		}
	}
	else if(ret == C_SUCCESS) {
		printf("No frame intervals found.\n");
	}
	else {
		print_handle_error(hDevice, "No frame intervals found", ret);
	}

done:
	if(intervals) free(intervals);
	return ret;
}


static CResult
list_frame_sizes (CHandle hDevice, CPixelFormat *pixelformat)
{
	CResult ret = C_SUCCESS;
	CFrameSize *sizes = NULL;
	unsigned int size = 0, count = 0;
	unsigned int i;

	ret = c_enum_frame_sizes(hDevice, pixelformat, NULL, &size, &count);
	if(ret == C_BUFFER_TOO_SMALL) {
		sizes = (CFrameSize *)malloc(size);
		ret = c_enum_frame_sizes(hDevice, pixelformat, sizes, &size, &count);
		if(ret) {
			print_handle_error(hDevice, "Unable to enumerate frame sizes", ret);
			goto done;
		}

		for(i = 0; i < count; i++) {
			CFrameSize *fsize = &sizes[i];

			if(fsize->type == CF_SIZE_DISCRETE) {
				printf("  Frame size: %ux%u\n", fsize->width, fsize->height);
				list_frame_intervals(hDevice, pixelformat, fsize);
			}
			else if(fsize->type == CF_SIZE_CONTINUOUS) {
				printf("  Frame sizes: %ux%u - %ux%u (continuous)\n"
					"  Will not display frame intervals.\n",
					fsize->min_width, fsize->min_height,
					fsize->max_width, fsize->max_height
				);
			}
			else if(fsize->type == CF_SIZE_STEPWISE) {
				printf("  Frame sizes: %ux%u - %ux%u (in steps of width = %u, height = %u)\n"
					"  Will not display frame intervals.\n",
					fsize->min_width, fsize->min_height,
					fsize->max_width, fsize->max_height,
					fsize->step_width, fsize->step_height
				);
			}
			else {
				print_handle_error(hDevice, "Unrecognized frame size type", -1);
			}
		}
	}
	else if(ret == C_SUCCESS) {
		printf("No frame sizes found.\n");
	}
	else {
		print_handle_error(hDevice, "No frame sizes found", ret);
	}

done:
	if(sizes) free(sizes);
	return ret;
}


static CResult
list_frame_formats (CHandle hDevice)
{
	CResult ret = C_SUCCESS;
	CPixelFormat *formats = NULL;
	unsigned int size = 0, count = 0;
	unsigned int i;

	ret = c_enum_pixel_formats(hDevice, NULL, &size, &count);
	if(ret == C_BUFFER_TOO_SMALL) {
		formats = (CPixelFormat *)malloc(size);
		ret = c_enum_pixel_formats(hDevice, formats, &size, &count);
		if(ret) {
			print_handle_error(hDevice, "Unable to enumerate pixel formats", ret);
			goto done;
		}

		for(i = 0; i < count; i++) {
			CPixelFormat *format = &formats[i];
			printf("Pixel format: %s (%s%s%s)\n",
				format->fourcc, format->name,
				format->mimeType ? "; MIME type: " : "",
				format->mimeType ? format->mimeType : ""
			);
			list_frame_sizes(hDevice, format);
		}
	}
	else if(ret == C_SUCCESS) {
		printf("No pixel formats found.\n");
	}
	else {
		print_handle_error(hDevice, "No pixel formats found", ret);
	}

done:
	if(formats) free(formats);
	return ret;
}

static CResult
list_entities (CDevice *device)
{
	int i;

	assert(device != NULL);

	int device_index = -1;
	if(sscanf(device->shortName, "video%u", &device_index) != 1)
		return C_INVALID_ARG;

	char media_path[32] = { 0 };
	sprintf(media_path, "/dev/media%u", device_index);
	//check for device node
	if(file_exists(media_path))
		printf("    Media controller device: %s\n", media_path);
	else
	{
		printf("    Media controller device %s doesn't exist\n", media_path);
		return C_INVALID_DEVICE;
	}
	
	int mcdev = open(media_path, O_RDWR);
	if(mcdev == -1) {
		printf("ERROR: Unable to open media controller device '%s': %s (Error: %d)\n", media_path, strerror(errno), errno);
		return C_INVALID_DEVICE;
	}

	int r = 0;
	struct media_entity_desc entity;
	memset(&entity, 0, sizeof(entity));
	entity.id = 1;
	while((r = ioctl(mcdev, MEDIA_IOC_ENUM_ENTITIES, &entity)) == 0) {
		printf(
			"    Entity %u: %s. Type: %u, Revision: %u, Flags: %u, Group-id: %u, Pads: %u, Links: %u\n",
			entity.id, entity.name, entity.type, entity.revision, entity.flags, entity.group_id, entity.pads, entity.links
		);

		switch(entity.type & MEDIA_ENT_TYPE_MASK) {
			case MEDIA_ENT_T_DEVNODE:
				printf("      Device node\n");
			
				switch(entity.type & MEDIA_ENT_SUBTYPE_MASK) {
					case MEDIA_ENT_T_DEVNODE_V4L:
						printf("      Node: v4l: { major: %u, minor: %u }\n", entity.v4l.major, entity.v4l.minor);
						break;
					case MEDIA_ENT_T_DEVNODE_FB:
						printf("      Node: fb: { major: %u, minor: %u }\n", entity.fb.major, entity.fb.minor);
						break;
					case MEDIA_ENT_T_DEVNODE_ALSA:
						printf("      Node: alsa: {card: %u, device: %u, subdevice: %u}\n", entity.alsa.card, entity.alsa.device, entity.alsa.subdevice);
						break;
					case MEDIA_ENT_T_DEVNODE_DVB:
						printf("      Node: dvb: %u\n", entity.dvb);
						break;
				}
				break;
				
			case MEDIA_ENT_T_V4L2_SUBDEV:
				printf("      Subdevice: ");
			
				switch(entity.type & MEDIA_ENT_SUBTYPE_MASK) {
					case MEDIA_ENT_T_V4L2_SUBDEV_SENSOR:
						printf(" Sensor\n");
						break;
					case MEDIA_ENT_T_V4L2_SUBDEV_FLASH:
						printf(" Flash\n");
						break;
					case MEDIA_ENT_T_V4L2_SUBDEV_LENS:
						printf(" Lens\n");
						break;
				}
				break;
		}

		struct media_links_enum links;
		memset(&links, 0, sizeof(links));
		links.entity = entity.id;
		links.pads = (struct media_pad_desc *)calloc(entity.pads, sizeof(struct media_pad_desc));
		links.links = (struct media_link_desc *)calloc(entity.links, sizeof(struct media_link_desc));

		r = ioctl(mcdev, MEDIA_IOC_ENUM_LINKS, &links);
		if(r == 0) {
			for(i = 0; i < entity.pads; i++) {
				struct media_pad_desc *pelem = &links.pads[i];
				printf(
					"      Entity: %u, Pad %u, Flags: %u\n",
					pelem->entity, pelem->index, pelem->flags
				);
			}

			for(i = 0; i < entity.links; i++) {
				struct media_link_desc *lelem = &links.links[i];
				printf(
					"      Out link: Source pad { Entity: %u, Index: %u, Flags: %u } => "
					"Sink pad { Entity: %u, Index: %u, Flags: %u }\n",
					lelem->source.entity, lelem->source.index, lelem->source.flags,
					lelem->sink.entity, lelem->sink.index, lelem->sink.flags
				);
			}
		}
		else {
			printf("ERROR: Unable to enumerate links for entity %u: %s (Error: %d)\n", entity.id, strerror(errno), errno);
		}

		free(links.links);
		links.links = NULL;
		free(links.pads);
		links.pads = NULL;

		entity.id++;
	}

	close(mcdev);
	return C_SUCCESS;
}


static CResult
list_devices ()
{
	CResult ret;
	unsigned int req_size = 0, buffer_size = 0, count = 0;
	CDevice *devices = NULL;
	int i;

	printf("Listing available devices:\n");

	do {
		// Allocate the required memory
		if(devices) free(devices);
		if(req_size) {		// No allocation the first time
			devices = (CDevice *)malloc(req_size);
			if(devices == NULL)
				return C_NO_MEMORY;
			buffer_size = req_size;
		}

		// Try to enumerate. If the buffer is not large enough, the required
		// size is returned.
		ret = c_enum_devices(devices, &req_size, &count);
		if(ret != C_SUCCESS && ret != C_BUFFER_TOO_SMALL)
			goto done;
	}
	while(buffer_size < req_size);

	if(count == 0) {
		printf("No devices found.\n");
		goto done;
	}

	// Print a list of all available devices
	for(i = 0; i < count; i++) {
		CDevice *device = &devices[i];
		print_device(device);

		ret = list_entities(device);
		if(ret != C_SUCCESS && ret != C_INVALID_ARG) {
			print_error("Unable to list device entities", ret);
			ret = C_SUCCESS;
		}
	}

done:
	if(devices) free(devices);
	if(ret)
		print_error("Unable to retrieve device list", ret);
	return ret;
}

/*
 * join path strings
 */
static char* 
path_cat (const char * str1, char * str2) 
{ 
	size_t str1_len = strlen (str1); 
	size_t str2_len = strlen (str2);
	int nc;
	
	if ((str1[str1_len] != '/') && (str2[0] != '/')) nc = 2;
	else nc = 1;
	 
	char * result; 
	result = malloc ((str1_len + str2_len + nc) * sizeof * result); 
	strcpy (result, str1); 
	int i, j;
	i = str1_len;
	if (nc > 1) result[i] = '/';
	i++; 
	for (j = 0; ((i <(str1_len + str2_len + (nc-1))) && (j <str2_len)); j++) 
	{ 
		result [i] = str2 [j];
		i++; 
	}
	result [str1_len + str2_len + (nc-1)] = '\0'; 
	return result; 
}


/*
 * get a xml filename NULL terminated list from data dir 
 */
static char**
get_filename (const char *dir_path, const char *vid)
{
	struct dirent * dp;
	struct dirent * sdp;
	char** file_list;
	int nf=1;
	DIR * dir = opendir(dir_path);
	
	file_list = calloc(1, sizeof(file_list));
	file_list[0] = NULL;
	printf ( "checking dir: %s \n", dir_path);
	while ((dp = readdir(dir)) != NULL) 
	{
#ifndef __QNXNTO__
		if((dp->d_type == DT_DIR) && (fnmatch("[[:xdigit:]][[:xdigit:]][[:xdigit:]][[:xdigit:]]", dp->d_name, 0) == 0))
#else
		struct stat st;

		stat(dp->d_name, &st);
		if ((st.st_mode & _S_IFDIR) && (fnmatch("[[:xdigit:]][[:xdigit:]][[:xdigit:]][[:xdigit:]]", dp->d_name, 0) == 0))
#endif /* __QNXNTO__ */
		{
			if( strcasecmp(vid, dp->d_name) != 0)
			{
				/*doesn't match - clean up and move to the next entry*/
				continue;
			}
			
			char *tmp = path_cat (dir_path, dp->d_name);
			printf("found dir: %s \n", dp->d_name);
			DIR * subdir = opendir(tmp);
			while ((sdp = readdir(subdir)) != NULL) 
			{
				if( fnmatch("*.xml", sdp->d_name, 0) == 0 )
				{
					file_list[nf-1] = path_cat (tmp, sdp->d_name);
					printf("found: %s \n", file_list[nf-1]);
					nf++;
					file_list = realloc(file_list,nf*sizeof(file_list));
					file_list[nf-1] = NULL;   
				} 
			}
			closedir(subdir);
			free (tmp);
			tmp = NULL;
		}
	} 
	if(!file_list[0]) printf("Could not find %s entry\n", vid);
	closedir (dir);
	return(file_list);
}

static CResult
add_control_mappings(CHandle hDevice, const char *filename)
{
	CResult res;
	CDynctrlInfo info = { 0 };
	int i;

	info.flags = CD_REPORT_ERRORS;
	if(HAS_VERBOSE())
		info.flags |= CD_RETRIEVE_META_INFO;

	printf("Importing dynamic controls from file %s.\n", filename);
	if (hDevice)
		res = c_add_control_mappings(hDevice, filename, &info);
	else
		res = c_add_control_mappings_from_file(filename, &info);
	if(res)
		print_error("Unable to import dynamic controls", res);

	// Print meta information if we're in verbose mode
	if(res == C_SUCCESS && HAS_VERBOSE()) {
		printf(
			"Available meta information:\n"
			"  File format: %d.%d\n"
			"  Author:      %s\n"
			"  Contact:     %s\n"
			"  Copyright:   %s\n"
			"  Revision:    %d.%d\n",
			info.meta.version.major, info.meta.version.minor,
			info.meta.author    ? info.meta.author    : "(unknown)",
			info.meta.contact   ? info.meta.contact   : "(unknown)",
			info.meta.copyright ? info.meta.copyright : "(unknown)",
			info.meta.revision.major, info.meta.revision.minor
		);
	}

	// Print errors
	if(info.message_count) {
		for(i = 0; i < info.message_count; i++) {
			CDynctrlMessage *msg = &info.messages[i];
			const char *severity = "message";
			switch(msg->severity) {
				case CD_SEVERITY_ERROR:		severity = "error";		break;
				case CD_SEVERITY_WARNING:	severity = "warning";	break;
				case CD_SEVERITY_INFO:		severity = "info";		break;
			}
			if(msg->line && msg->col) {
				printf("%s:%d:%d: %s: %s\n", filename, msg->line, msg->col, severity, msg->text);
			}
			else if(msg->line) {
				printf("%s:%d: %s: %s\n", filename, msg->line, severity, msg->text);
			}
			else {
				printf("%s: %s: %s\n", filename, severity, msg->text);
			}
		}
	}

	// Print processing statistics if we're in verbose mode
	if(HAS_VERBOSE()) {
		printf(
			"Processing statistics:\n"
			"  %u constants processed (%u failed, %u successful)\n"
			"  %u controls processed (%u failed, %u successful)\n"
			"  %u mappings processed (%u failed, %u successful)\n",
			info.stats.constants.successful + info.stats.constants.failed,
			info.stats.constants.successful, info.stats.constants.failed,
			info.stats.controls.successful + info.stats.controls.failed,
			info.stats.controls.successful, info.stats.controls.failed,
			info.stats.mappings.successful + info.stats.mappings.failed,
			info.stats.mappings.successful, info.stats.mappings.failed
		);
	}

	if(info.messages)
		free(info.messages);
	if(info.meta.author)
		free(info.meta.author);
	if(info.meta.contact)
		free(info.meta.contact);
	if(info.meta.author)
		free(info.meta.copyright);
	return res;
}

int
main (int argc, char **argv)
{
	CHandle handle = 0;
	CResult res = C_SUCCESS;

	// Parse the command line
	if(cmdline_parser(argc, argv, &args_info) != 0)
		exit(1);
	
	// Display help if no arguments were specified
	if(argc == 1) {
		cmdline_parser_print_help();
		exit(0);
	}

	res = c_init();
	if(res) goto done;
	
	// Open the device
	if (!args_info.list_given && (!args_info.import_given || args_info.device_given)) {
		handle = c_open_device(args_info.device_arg);
		if(!handle) {
			print_error("Unable to open device", -1);
			res = C_INVALID_DEVICE;
			goto done;
		}
	}

	// List devices
	if(args_info.list_given) {
		res = list_devices();
		goto done;
	}
	// Import dynamic controls from XML file
	else if(args_info.import_given) {
		res = add_control_mappings(handle, args_info.import_arg);
		goto done;
	}
	// Import dynamic controls from XML files at default location
	if(args_info.addctrl_given) {
		// list all xml files at default data/vid dir
		int nf=0;
		char vid[5];
		char pid[5];
		short pid_set = 0;
		if(fnmatch("[[:xdigit:]][[:xdigit:]][[:xdigit:]][[:xdigit:]]:[[:xdigit:]][[:xdigit:]][[:xdigit:]][[:xdigit:]]", args_info.addctrl_arg, 0))
		{
			if(fnmatch("[[:xdigit:]][[:xdigit:]][[:xdigit:]][[:xdigit:]]", args_info.addctrl_arg, 0))
			{
				printf("%s invalid: set at least a valid vid value, :pid is optional\n", args_info.addctrl_arg);
				goto done;
			}
			else
			{
				/*extract vid and reset pid*/
				int c = 0;
				for (c = 0; c < 4; c++)
				{
					vid[c] = args_info.addctrl_arg[c];
					pid[c] = 0;
				}
				vid[4] = '\0';
				pid[4] = '\0';
			}
		}
		else 
		{
			/*extract vid and pid*/
			int c = 0;
			for (c = 0; c < 4; c++)
			{
				vid[c] = args_info.addctrl_arg[c];
				pid[c] = args_info.addctrl_arg[c+5];
			}
			vid[4] = '\0';
			pid[4] = '\0';
			pid_set = 1; /*flag pid.xml check*/
			//printf("vid:%s pid:%s\n", vid, pid);
		}
		
		/* get xml file list from DATA_DIR/vid/ */ 
		char **xml_files = get_filename (DATA_DIR, vid);
 
		/*check for pid.xml*/
		char fname[9];
		strcpy(fname, pid);
		strcat(fname,".xml");
		if(pid_set)
		{
			pid_set = 0; /*reset*/
			nf=0;
			while (xml_files[nf] != NULL)
			{
				if ( strcasecmp(fname, xml_files[nf]) == 0)
					pid_set = 1; /*file exists so flag it*/
				nf++;
			}
		}
		
		/*parse xml files*/
		nf = 0;
		while (xml_files[nf] != NULL)
		{
			/* if pid was set and pid.xml exists parse it*/
			if(pid_set)
			{
				if ((strcasecmp(fname, xml_files[nf]) == 0))
				{
					printf ( "Parsing: %s \n", xml_files[nf]);
					res = add_control_mappings(handle, xml_files[nf]);
				}
			}
			else /* parse all xml files inside vid dir */
			{
				printf ( "Parsing: %s \n", xml_files[nf]);
				res = add_control_mappings(handle, xml_files[nf]);
			}
			free(xml_files[nf]);
			xml_files[nf]=NULL;
			nf++;
		}
		free(xml_files);
		goto done;
	}

	// List frame formats
	if(args_info.formats_given) {
		printf("Listing available frame formats for device %s:\n", args_info.device_arg);
		res = list_frame_formats(handle);
	}
	// List controls
	else if(args_info.clist_given) {
		printf("Listing available controls for device %s:\n", args_info.device_arg);
		res = list_controls(handle);
	}
	// Retrieve control value
	else if(args_info.get_given) {
		CControlValue value;
		
		// Resolve the control Id
		CControlId controlId = get_control_id(handle, args_info.get_arg);
		if(!controlId) {
			res = 1;
			print_handle_error(handle, "Unknown control specified", -1);
			goto done;
		}

		// Retrieve the control value
		res = c_get_control(handle, controlId, &value);
		if(res) {
			print_handle_error(handle, "Unable to retrieve control value", res);
			goto done;
		}
		printf("%d\n", value.value);
	}
	// Retrieve raw control value
	else if(args_info.get_raw_given) {
		//scan input
		uint16_t unit_id;
		unsigned char selector;
		sscanf(args_info.get_raw_arg, "%hu:%hhu", &unit_id, &selector);
		CControlValue value;
		value.type = CC_TYPE_RAW;
		// the entity is only used for the generating a control name
		//TODO: pass the guid through cmdline (optional)
		unsigned char entity[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
		res = c_read_xu_control(handle, entity, unit_id, selector, &value);
		if(res) {
			print_handle_error(handle, "Unable to retrieve control value", res);
			goto done;
		}
		
		//print the raw value
		uint8_t * val = value.raw.data;
		int i=0;
		printf("query current value of: (LE)0x");
		for(i=0; i<value.raw.size; i++)	{
			printf("%.2x", val[i]);
		}
		printf("  (BE)0x");
		for(i=value.raw.size-1; i >=0; i--) {
			printf("%.2x", val[i]);
		}
		printf("\n");
		//free the raw value alocation
		if(value.raw.data) free(value.raw.data);
	}
	else if(args_info.set_given) {
		CControlValue value;

		// Parse the control value
		if(args_info.inputs_num < 1) {
			res = 3;
			print_error("No control value specified", -1);
			goto done;
		}
		if(parse_control_value(args_info.inputs[0], &value)) {
			res = 2;
			print_error("Invalid control value specified", -1);
			goto done;
		}

		// Resolve the control Id
		CControlId controlId = get_control_id(handle, args_info.set_arg);
		if(!controlId) {
			res = 1;
			print_handle_error(handle, "Unknown control specified", -1);
			goto done;
		}

		// Set the new control value
		res = c_set_control(handle, controlId, &value);
		if(res) {
			print_handle_error(handle, "Unable to set new control value", res);
			goto done;
		}
	}
	// Set the raw control value
	else if(args_info.set_raw_given) {
		CControlValue value;
		
		// Parse the control value
		if(args_info.inputs_num < 1) {
			res = 3;
			print_error("No raw control value specified", -1);
			goto done;
		}
		uint16_t unit_id;
		unsigned char selector;

		sscanf(args_info.set_raw_arg, "%hu:%hhu", &unit_id, &selector);
		
		parse_raw_control_value (args_info.inputs[0], &value);
		
		// the entity is only used for the generating a control name
		//TODO: pass the guid through cmdline (optional)
		unsigned char entity[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
		res = c_write_xu_control(handle, entity, unit_id, selector, &value);
		if(res) {
			print_handle_error(handle, "Unable to set the control value", res);
			goto done;
		}
		
		//print the raw value le and be format
		uint8_t * val = value.raw.data;
		int i=0;
		printf("set value of          : (LE)0x");
		for(i=0; i<value.raw.size; i++)	{
			printf("%.2x", val[i]);
		}
		printf("  (BE)0x");
		for(i=value.raw.size-1; i >=0; i--) {
			printf("%.2x", val[i]);
		}
		printf("\n");
		//free the raw value alocation
		if(value.raw.data) free(value.raw.data);
	}
	else if(args_info.save_ctrl_given) {
		res = save_controls( handle, args_info.save_ctrl_arg);
	}
	else if(args_info.load_ctrl_given) {
		res = load_controls( handle, args_info.load_ctrl_arg);
	}

	// Clean up
done:
	if(handle) c_close_device(handle);
	c_cleanup();
	cmdline_parser_free(&args_info);

	return res;
}
