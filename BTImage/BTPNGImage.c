#include "BTPNGImage.h"
#include "BTInflate.h"
#include <stdlib.h>
#include <string.h>

#ifdef BT_PLATFORM_WINDOWS
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif

BT_PNGImage* BT_PNGImage_Create(u32 width, u32 height, u8 color_type, u8 bit_depth) {
  BT_PNGImage* image = (BT_PNGImage*)malloc(sizeof(BT_PNGImage));
  if (image == NULL) {
    fprintf(stderr, "Failed to allocate memory for PNG image\n");
    return NULL;
  }

  image->width = width;
  image->height = height;
  image->color_type = color_type;
  image->bit_depth = bit_depth;

  // Calculate the size of the pixel data
  u32 bytes_per_pixel = 0;
  switch(color_type) {
    case BT_PNG_COLOR_TYPE_GRAY:
      bytes_per_pixel = bit_depth / 8;
      break;
    case BT_PNG_COLOR_TYPE_RGB:
      bytes_per_pixel = 3 * (bit_depth / 8);
      break;
    case BT_PNG_COLOR_TYPE_PALETTE:
      bytes_per_pixel = bit_depth / 8;
      break;
    case BT_PNG_COLOR_TYPE_GRAY_ALPHA:
      bytes_per_pixel = 2 * (bit_depth / 8);
      break;
    case BT_PNG_COLOR_TYPE_RGBA:
      bytes_per_pixel = 4 * (bit_depth / 8);
      break;
    default:
      fprintf(stderr, "Invalid color type for PNG image\n");
      free(image);
      return NULL;
  }

  // Allocate memory for the pixel_datab8 BT_Validate_PNG_File_Signature(FILE* fp);
  return image;
}

void BT_PNGImage_Free(BT_PNGImage* image) {
  if (image == NULL) {
    return;
  }

  free(image->pixel_data);
  free(image);
}

BT_PNGImage* BT_PNGImage_LoadFromFile(const char* filename) {
  FILE* fp = fopen(filename, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open PNG file: %s\n", filename);
    return NULL;
  }

  if (!BT_Validate_PNG_File_Signature(fp)) {
    fclose(fp);
    return NULL;
  }

  BT_PNGImage* image = (BT_PNGImage*)calloc(1,sizeof(BT_PNGImage));
  if (image == NULL) {
    fprintf(stderr, "Failed to allocate memory for PNG image\n");
    fclose(fp);
    return NULL;
  }

  u8* compressed_data = NULL;
  size_t compressed_size = 0;

  while (1) {
    BT_PNGChunk chunk = BT_PNG_Read_Chunk(fp);
    if (strcmp(chunk.type, "IHDR") == 0) {
      // Handle IHDR chunk
      BT_PNG_Handle_IHDR_Chunk(image, &chunk);
    } else if (strcmp(chunk.type, "IDAT") == 0) {
      // Handle IDAT chunk
      BT_PNG_Handle_IDAT_Chunk(image, &chunk, &compressed_data, &compressed_size);
    } else if (strcmp(chunk.type, "IEND") == 0) {
      // Handle IEND chunk
      free(chunk.data);
      break;
    }
    free(chunk.data);
  }

  fclose(fp);

  size_t rowbytes = (image->width * image->bit_depth * ((image->color_type == 2 ? 3 : 1) + (image->color_type == 6 ? 1 : 0)) + 7) / 8;
  size_t decompressed_size = rowbytes * image->height;
  if (BT_PNG_Decompress_IDAT_Data(compressed_data, compressed_size, &image->pixel_data, &decompressed_size) != 0) {
    fprintf(stderr, "Failed to decompress image data\n");
    free(compressed_data);
    free(image);
    return NULL;
  }

  free(compressed_data);
  return image;
}

b8 BT_Validate_PNG_File_Signature(FILE* fp) {
  u8 png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  u8 buffer[8] = {0};
  size_t bytes_read = 0;
  if ((bytes_read = fread(buffer, 1, 8, fp)) != 8) {
    fprintf(stderr, "Failed to read PNG file signature\n");
    return FALSE;
  }
 
  for (int i = 0; i < 8; i++) {
    if (buffer[i] != png_signature[i]) {
      fprintf(stderr, "Invalid PNG file signature\n");
      return FALSE;
    }
  }

  return TRUE;
}

BT_PNGChunk BT_PNG_Read_Chunk(FILE* fp) {
  BT_PNGChunk chunk;
  size_t bytes_read = 0;
  bytes_read = fread(&chunk.length, 4, 1, fp);
  if (bytes_read != 1) {
    fprintf(stderr, "Failed to read chunk length\n");
    return chunk;
  } 
  chunk.length = ntohl(chunk.length);

  bytes_read = fread(chunk.type, 1, 4, fp);
  if (bytes_read != 4) {
    fprintf(stderr, "Failed to read chunk type\n");
    return chunk;
  }
  chunk.type[4] = '\0';
  chunk.data = (u8*)malloc(chunk.length);
  if (chunk.data == NULL) {
    fprintf(stderr, "Failed to allocate memory for chunk data\n");
    return chunk;
  }

  bytes_read = fread(chunk.data, 1, chunk.length, fp);
  if (bytes_read != chunk.length) {
    fprintf(stderr, "Failed to read chunk data\n");
    free(chunk.data);
    return chunk;
  }

  bytes_read = fread(&chunk.crc, 4, 1, fp);
  if (bytes_read != 1) {
    fprintf(stderr, "Failed to read chunk CRC\n");
    free(chunk.data);
    return chunk;
  }
  chunk.crc = ntohl(chunk.crc);

  return chunk;
}

void BT_PNG_Handle_IHDR_Chunk(BT_PNGImage* image, BT_PNGChunk* chunk) {
  image->width = ntohl(*(u32*)chunk->data);
  image->height = ntohl(*(u32*)(chunk->data + 4));
  image->bit_depth = *(chunk->data + 8);
  image->color_type = *(chunk->data + 9);
}

void BT_PNG_Handle_IDAT_Chunk(BT_PNGImage* image, BT_PNGChunk* chunk, u8** compressed_data, size_t* compressed_size) {
  *compressed_data = realloc(*compressed_data, *compressed_size + chunk->length);
  if (*compressed_data == NULL) {
    fprintf(stderr, "Failed to allocate memory for compressed data\n");
    return;
  }
  memcpy(*compressed_data + *compressed_size, chunk->data, chunk->length);
  *compressed_size += chunk->length;
}

i32 BT_PNG_Decompress_IDAT_Data(u8* compressed_data, size_t compressed_size, u8** decompressed_data, size_t decompressed_size) {
  return BT_INFLATE(compressed_data, compressed_size, decompressed_data, &decompressed_size);
}