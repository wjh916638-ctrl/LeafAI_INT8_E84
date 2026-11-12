#ifndef LEAF_PROTOCOL_H
#define LEAF_PROTOCOL_H

#include <stdint.h>

#define LEAF_PROTOCOL_VERSION        1u
#define LEAF_HEADER_SIZE             24u
#define LEAF_MAX_PAYLOAD             1048576u

#define LEAF_TYPE_IMAGE_RGB888        1u
#define LEAF_TYPE_RESULT_JSON         2u
#define LEAF_TYPE_ACK                 3u

#define LEAF_MAGIC_0                  'L'
#define LEAF_MAGIC_1                  'E'
#define LEAF_MAGIC_2                  'A'
#define LEAF_MAGIC_3                  'F'

uint32_t leaf_crc32(const uint8_t *data, uint32_t length);
uint16_t leaf_get_u16_le(const uint8_t *data);
uint32_t leaf_get_u32_le(const uint8_t *data);

#endif

