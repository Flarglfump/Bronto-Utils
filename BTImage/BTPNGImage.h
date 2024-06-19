#pragma once
#include "BTCore/defines.h"
#include <stdio.h>

typedef enum BT_PNGColorType {
  BT_PNG_COLOR_TYPE_GRAY = 0,
  BT_PNG_COLOR_TYPE_RGB = 2,
  BT_PNG_COLOR_TYPE_PALETTE = 3,
  BT_PNG_COLOR_TYPE_GRAY_ALPHA = 4,
  BT_PNG_COLOR_TYPE_RGBA = 6
} BT_PNGColorType;

typedef struct BT_PNGImage {
  u32 width;
  u32 height;
  u8 color_type; // PNGColorType
  u8 bit_depth;
  u8* pixel_data;
} BT_PNGImage;

typedef struct BT_PNGChunk {
  u32 length;
  u8 type[5];
  u8* data;
  u32 crc;
} BT_PNGChunk;

/* Basic functions for PNG Image Struct */
BT_PNGImage* BT_PNGImage_Create(u32 width, u32 height, u8 color_type, u8 bit_depth);
void BT_PNGImage_Free(BT_PNGImage* image);

/* Functions for PNG file handling */
BT_PNGImage* BT_PNGImage_LoadFromFile(const char* filename);
b8 BT_Validate_PNG_File_Signature(FILE* fp);
BT_PNGChunk BT_PNG_Read_Chunk(FILE* fp);
void BT_PNG_Handle_IHDR_Chunk(BT_PNGImage* image, BT_PNGChunk* chunk);
void BT_PNG_Handle_IDAT_Chunk(BT_PNGImage* image, BT_PNGChunk* chunk, u8** compressed_data, size_t* compressed_size);
i32 BT_PNG_Decompress_IDAT_Data(u8* compressed_data, size_t compressed_size, u8** decompressed_data, size_t decompressed_size);