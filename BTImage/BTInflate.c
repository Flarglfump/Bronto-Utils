#include "BTInflate.h"
#include <stdio.h>

void BT_BitStream_Align_To_Byte_Boundary(BT_BitStream* stream) {
  stream->bitpos = (stream->bitpos + 7) & ~7;
}

u32 BT_Read_BitStream(BT_BitStream* stream, u32 numBits) {
  u32 result = 0;
  for (u32 i = 0; i < numBits; i++) {
    if (stream->bitpos >= stream->size * 8) {
      fprintf(stderr, "Bitstream overflow!\n");
      break;
    }
    result |= ((stream->data[stream->bitpos / 8] >> (stream->bitpos % 8)) & 1) << i;
    stream->bitpos++;
  }
  return result;
}

i32 BT_INFLATE(u8* compressed_data, size_t compressed_size, u8** decompressed_data, size_t* decompressed_size) {
  BT_BitStream stream = {
    .data = compressed_data,
    .size = compressed_size,
    .bitpos = 0
  };

  *decompressed_data = (u8*)malloc(BT_INFLATE_CHUNK_SIZE);
  if (!*decompressed_data) {
    fprintf(stderr, "Failed to allocate memory for decompressed data\n");
    return -1;
  }

  size_t outpos = 0;
  i32 last_block = 0;
  while (!last_block) {
    last_block = read_bits(&stream, 1);
    i32 block_type = read_bits(&stream, 2);

    if (block_type == 0) {
      // Uncompressed block
      if ((BT_INFLATE_Read_Uncompressed_Block(&stream, decompressed_data, decompressed_size, &outpos)) != TRUE) {
        free(*decompressed_data);
        return -1;
      }
    } else if (block_type == 1) {
      // Fixed Huffman block
      if (BT_INFLATE_Read_Fixed_Huffman_Block(&stream, decompressed_data, decompressed_size, &outpos) != 0) {
        free(*decompressed_data);
        return -1;
      }
    } else if (block_type == 2) {
      // Dynamic Huffman block
      if (BT_INFLATE_Read_Dynamic_Huffman_Block(&stream, decompressed_data, decompressed_size, &outpos) != 0) {
        free(*decompressed_data);
        return -1;
      }
    } else {
      fprintf(stderr, "Unsupported block type: %d\n", block_type);
      free(*decompressed_data);
      return -1;
    }

    // Reallocate decompressed data buffer
    if (outpos + BT_INFLATE_CHUNK_SIZE > *decompressed_size) {
      *decompressed_size += BT_INFLATE_CHUNK_SIZE;
      *decompressed_data = (u8*)realloc(*decompressed_data, *decompressed_size);
      if (!*decompressed_data) {
        fprintf(stderr, "Failed to reallocate memory for decompressed data\n");
        return -1;
      }
    }
  }
  
  *decompressed_size = outpos;
  return 0;
}

b8 BT_INFLATE_Write_To_Output(u8** output, size_t* output_size, size_t* output_pos, u8 val) {
  if (*output_pos >= *output_size) {
    *output_size += BT_INFLATE_CHUNK_SIZE;
    *output = (unsigned char*)realloc(*output, *output_size);
    if (!*output) {
      fprintf(stderr, "Failed to reallocate memory for output\n");
      return FALSE;
    }
  }
  (*output)[(*output_pos)++] = val;
  return TRUE;
}

b8 BT_INFLATE_Read_Uncompressed_Block(BT_BitStream* stream, u8** output, size_t* output_size, size_t* output_pos) {
  BT_BitStream_Align_To_Byte_Boundary(stream);
  if (stream->bitpos + 32 > stream->size * 8) {
    fprintf(stderr, "Unexpected end of stream\n");
    return FALSE;
  }

  u16 len = read_bits(stream, 16);
  u16 nlen = read_bits(stream, 16);

  if (len != (u16)~nlen) {
    fprintf(stderr, "Length check failed\n");
    return FALSE;
  }

  // Reallocate output buffer
  if (*output_pos + len > *output_size) {
    *output_size = *output_pos + len;
    *output = (u8*)realloc(*output, *output_size);
    if (!*output) {
      fprintf(stderr, "Failed to reallocate memory for output\n");
      return -1;
    }
  }

  for (u16 i = 0; i < len; i++) {
    if (stream->bitpos / 8 >= stream->size) {
      fprintf(stderr, "Unexpected end of stream\n");
      return  FALSE;
    }
    (*output)[(*output_pos)++] = stream->data[stream->bitpos / 8];
    stream->bitpos += 8;
  }

  return TRUE;
}

void BT_INFLATE_Build_Fixed_Huffman_Tables(u16* literal_lengths, u16* distance_lengths) {
  for (int i = 0; i <= 143; i++) {
    literal_lengths[i] = 8;
  }
  for (int i = 144; i <= 255; i++) {
    literal_lengths[i] = 9;
  }
  for (int i = 256; i <= 279; i++) {
    literal_lengths[i] = 7;
  }
  for (int i = 280; i <= 287; i++) {
    literal_lengths[i] = 8;
  }
  for (int i = 0; i < 30; i++) {
    distance_lengths[i] = 5;
  }
}

void BT_INFLATE_Build_Huffman_Tree(u16* lengths, i32 num_codes, u16* codes) {
  i32 bl_count[16] = {0};
  i32 next_code[16] = {0};
  for (i32 i = 0; i < num_codes; i++) {
    bl_count[lengths[i]]++;
  }
  i32 code = 0;
  bl_count[0] = 0;
  for (i32 bits =1; bits <= 15; bits++) {
    code = (code + bl_count[bits - 1]) << 1;
    next_code[bits] = code;
  }
  for (i32 n = 0; n < num_codes; n++) {
    i32 len = lengths[n];
    if (len != 0) {
      codes[n] = next_code[len];
      next_code[len]++;
    }
  }
}

i32 BT_INFLATE_Decode_Huffman_Fixed(BT_BitStream* stream, u16* lengths, int max_code) {
  i32 code = 0;
  i32 first = 0;
  i32 index = 0;
  for (i32 len = 1; len <= 15; len++) {
    code |= BT_Read_BitStream(stream, 1);
    i32 count = 0;
    for (u32 n = first; n <= max_code; n++) {
      if (lengths[n] == len) {
        if (code == index) {
          return n;
        }
        index++;
        count++;
      }
      first += count;
      code <<= 1;
      index <<= 1;
    }
  }
  return -1;
}

i32 BT_INFLATE_Decode_Huffman_Dynamic(BT_BitStream* stream, u16* codes, u16* lengths, int max_code) {
  i32 code = 0;
  i32 first = 0;
  for (i32 len = 1; len <= 15; len++) {
    code |= BT_Read_BitStream(stream, 1);
    i32 count = 0;
    for (u32 n = first; n <= max_code; n++) {
      if (lengths[n] == len) {
        if (code == codes[n]) {
          return n;
        }
        count++;
      }
      first += count;
      code <<= 1;
    }
  }
  return -1;
}
i32 BT_INFLATE_Read_Fixed_Huffman_Block(BT_BitStream* stream, u8** output, size_t* output_size, size_t* output_pos) {
  u16 literal_lengths[288];
  u16 distance_lengths[32];
  BT_INFLATE_Build_Fixed_Huffman_Tables(literal_lengths, distance_lengths);

  while (1) {
    i32 symbol = BT_INFLATE_Decode_Huffman_Fixed(stream, literal_lengths, 287);
    if (symbol < 256) {
      if (BT_INFLATE_Write_To_Output(output, output_size, output_pos, (u8)symbol) != TRUE) {
        return -1;
      }
    } else if (symbol == 256) {
      break;
    } else {
      symbol -= 257;
      i32 length;
      if (symbol < 8) {
        length = symbol + 3;
      } else if (symbol == 28) {
        length = 258;
      } else {
        int extra_bits = (symbol - 8) / 4 + 1;
        length = ((1 << (extra_bits + 2)) + ((symbol - 8) % 4) * (1 << extra_bits) + read_bits(stream, extra_bits) + 3);
      }

      i32 distance_symbol = BT_INFLATE_Decode_Huffman_Fixed(stream, distance_lengths, 31);
      i32 distance;
      if (distance_symbol < 4) {
        distance = distance_symbol + 1;
      } else {
        i32 extra_bits = (distance_symbol / 2) - 1;
        distance = ((1 << (extra_bits + 1)) + (distance_symbol % 2) * (1 << extra_bits) + read_bits(stream, extra_bits) + 1);
      }

      for (i32 i = 0; i < length; i++) {
        if (BT_INFLATE_Write_To_Output(output, output_size, output_pos, (*output)[*output_pos - distance]) != TRUE) {
          return -1;
        }
      }
    }
  }
  return 0;
}

i32 BT_INFLATE_Read_Dynamic_Huffman_Block(BT_BitStream* stream, u8** output, size_t* output_size, size_t* output_pos) {
  i32 hlit = BT_Read_BitStream(stream, 5) + 257;
  i32 hdist = read_bits(stream, 5) + 1;
  i32 hclen = read_bits(stream, 4) + 4;

  u16 code_lengths_order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
  u16 code_lengths[19] = {0};

  for (i32 i = 0; i < hclen; i++) {
    code_lengths[code_lengths_order[i]] = BT_Read_BitStream(stream, 3);
  }

  u16 code_lengths_codes[19] = {0};
  BT_INFLATE_Build_Huffman_Tree(code_lengths, 19, code_lengths_codes);

  u16 literal_lengths[288] = {0};
  u16 distance_lengths[32] = {0};

  i32 i = 0;
  while (i < hlit + hdist) {
    i32 symbol = BT_INFLATE_Decode_Huffman_Dynamic(stream, code_lengths_codes, code_lengths, 19);
    if (symbol <= 15) {
      if (i < hlit) {
        literal_lengths[i++] = symbol;
      } else {
        distance_lengths[i++ - hlit] = symbol;
      }
    } else if (symbol == 16) {
      i32 repeat = 3 + BT_Read_BitStream(stream, 2);
      i32 prev_code = i > 0 ? (i < hlit ? literal_lengths[i - 1] : distance_lengths[i - hlit - 1]) : 0;
      while (repeat-- > 0) {
        if (i < hlit) {
          literal_lengths[i++] = prev_code;
        } else {
          distance_lengths[i++ - hlit] = prev_code;
        }
      }
    } else if (symbol == 17) {
      i32 repeat = 3 + BT_Read_BitStream(stream, 3);
      while (repeat-- > 0) {
        if (i < hlit) {
          literal_lengths[i++] = 0;
        } else {
          distance_lengths[i++ - hlit] = 0;
        }
      }
    } else if (symbol == 18) {
      i32 repeat = 11 + BT_Read_BitStream(stream, 7);
      while (repeat-- > 0) {
        if (i < hlit) {
          literal_lengths[i++] = 0;
        } else {
          distance_lengths[i++ - hlit] = 0;
        }
      }
    }
  }

  u16 literal_codes[288] = {0};
  u16 distance_codes[32] = {0};
  BT_INFLATE_Build_Huffman_Tree(literal_lengths, hlit, literal_codes);
  BT_INFLATE_Build_Huffman_Tree(distance_lengths, hdist, distance_codes);

  while (1) {
    i32 symbol = BT_INFLATE_Decode_Huffman_Dynamic(stream, literal_codes, literal_lengths, hlit);
    if (symbol < 256) {
      if (*output_pos >= *output_size) {
        *output_size += BT_INFLATE_CHUNK_SIZE;
        *output = (u8*)realloc(*output, *output_size);
        if (!output) {
          fprintf(stderr, "Failed tyo reallocate memory for output\n");
          return -1;
        }
      }
      (*output)[(*output_pos)++] = (u8)symbol;
    } else if (symbol == 256) {
      break;
    } else {
      symbol -= 257;
      i32 length;
      if (symbol < 8) {
        length = symbol + 3;
      } else if (symbol == 28) {
        length = 258;
      } else {
        i32 extra_bits = (symbol - 8) / 4 + 1;
        length = ((1 << (extra_bits + 2)) + ((symbol - 8) % 4) * (1 << extra_bits) + read_bits(stream, extra_bits) + 3);
      }

      i32 distance_symbol = BT_INFLATE_Decode_Huffman_Dynamic(stream, distance_codes, distance_lengths, hdist);
      i32 distance;
      if (distance_symbol < 4) {
        distance = distance_symbol + 1;
      } else {
        i32 extra_bits = (distance_symbol / 2) - 1;
        distance = ((1 << (extra_bits + 1)) + (distance_symbol % 2) * (1 << extra_bits) + read_bits(stream, extra_bits) + 1);
      }

      for (i32 i = 0; i < length; i++) {
        if (*output_pos >= *output_size) {
          *output_size += BT_INFLATE_CHUNK_SIZE;
          *output = (unsigned char*)realloc(*output, *output_size);
          if (!*output) {
            fprintf(stderr, "Failed ot reallocate memory for output\n");
            return -1;
          }
        }
        (*output)[(*output_pos)++] = (*output)[*output_pos - distance];
      }
    }
  }
  return 0;
}