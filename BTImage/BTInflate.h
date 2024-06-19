#pragma once
#include "BTCore/defines.h"
#include <stdlib.h>

#define BT_INFLATE_CHUNK_SIZE 1024

typedef struct BT_BitStream {
  const u8* data;
  size_t size;
  size_t bitpos;
} BT_BitStream;

void BT_BitStream_Align_To_Byte_Boundary(BT_BitStream* stream);
u32 BT_Read_BitStream(BT_BitStream* stream, u32 numBits);
i32 BT_INFLATE(u8* compressed_data, size_t compressed_size, u8** decompressed_data, size_t* decompressed_size);
void BT_INFLATE_Write_To_Output(u8** output, size_t* output_size, size_t* output_pos, u8 val);

i32 BT_INFLATE_Read_Uncompressed_Block(BT_BitStream* stream, u8** output, size_t* output_size, size_t* output_pos);

void BT_INFLATE_Build_Fixed_Huffman_Tables(u16* literal_lengths, u16* distance_lengths);
void BT_INFLATE_Build_Huffman_Tree(u16* lengths, i32 num_codes, u16* codes);
i32 BT_INFLATE_Decode_Huffman_Fixed(BT_BitStream* stream, u16* lengths, int max_code);
i32 BT_INFLATE_Decode_Huffman_Dynamic(BT_BitStream* stream, u16* codes, u16* lengths, int max_code);
i32 BT_INFLATE_Read_Fixed_Huffman_Block(BT_BitStream* stream, u8** output, size_t* output_size, size_t* output_pos);
i32 BT_INFLATE_Read_Dynamic_Huffman_Block(BT_BitStream* stream, u8** output, size_t* output_size, size_t* output_pos);