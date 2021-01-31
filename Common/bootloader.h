/*
 * bootloader.h
 *
 * Created: 1/18/2021 1:41:25 PM
 *  Author: robbytong
 */ 


#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#define SK_APP_PAGE			(400)

#define SK_APP_ADDR			(SK_APP_PAGE * 64)


#define SK_HEADER_SIZE			(64)

#define SK_IMAGE_TYPE_APP		(0)
#define SK_IMAGE_TYPE_DFU		(1)

#define SK_IMAGE_MAGIC			(0xcafefeed)

union SKAppHeader
{
	struct
	{
		uint32_t	magic;
		uint32_t	header_version;
		uint8_t   image_type;
		uint8_t		fw_major_version;
		uint8_t		fw_minor_version;
		uint8_t		fw_patch_version;
		uint32_t	vector_table_addr;
		char			git_sha[8];
	}fields;
	
	uint8_t data[SK_HEADER_SIZE];
};


#endif /* BOOTLOADER_H_ */