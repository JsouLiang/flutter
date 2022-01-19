//
//  move_mod_inits.hpp
//  ld
//
//  Created by yuyang.1024 on 15/03/2021.
//  Copyright © 2021 Bytedance. All rights reserved.
//

#ifndef compress_const_section_meta_h
#define compress_const_section_meta_h

#include <stdint.h>

#define LTC_DATA_SEGMENT "__DATA"
#define LTC_COMPRESS_DATA_SEGMENT "__LTC_DATA"
#define LTC_META_SECTION_NAME "__meta"
#define LTC_DECOMPRESSOR_INFO_SECTION_NAME "__deco"
#define LTC_FIX_DATA_SECTION_NAME "__fix"
#define LTC_SECTION_NAME_COMPRESSED_PREFIX "_C_"
#define LTC_SECTION_NAME_DECOMPRESSED_PREFIX "_D_"
#define LTC_TEXT_CONST_NAME "tconst"
#define LTC_DATA_CONST_NAME "dconst"
#define LTC_DATA_DATA_NAME "ddata"
#define LTC_TEXT_GCC_EXCEPT_TAB_NAME "gcc_except_tab"
#define LTC_OBJC_CONST_NAME "objc_const"
#define LTC_OBJC_IVAR_NAME "objc_ivar"
#define LTC_OBJC_SELREF_NAME "objc_selrefs"
#define LTC_OBJC_CFSTRING_NAME "cfstring"
#define LTC_DEFAULT_DECOMPRESSOR BDLDecompressor
#define LTC_DEFAULT_DECOMPRESSOR_NAME "BDLDecompressor"
#define LTC_MAIN_IMAGE_DECOMPRESSOR_SUFFIX "MainImage"
#define LTC_MAIN_IMAGE_DECOMPRESSOR_NAME "BDLDecompressor_MainImage"
#define LTC_MAIN_IMAGE_DECOMPRESSOR_DYLIB_NAME "BDLRepairer"

// Whether a CFString is referencing a cstring data or ustring data.
#define LTC_CFSTRING_CSTRING_TYPE 1
#define LTC_CFSTRING_USTRING_TYPE 2

#define LTC_MODULE_NAME_MAX 16
#define LTC_SEGMENT_NAME_MAX 16
#define LTC_SECTION_NAME_MAX 23

#define LTC_COMPRESSED_SECTION_MAX 24

#define LTC_META_VERSION 1

typedef enum {
  LTC_LZ4 = 0,
  LTC_ZLIB,
  LTC_LZMA,
  LTC_LZFSE,
  LTC_ZSTD,
  LTC_ALG_MAX,
} LTCAlgorithm;

typedef enum {
  LTC_REBASE = 0,
  LTC_OBJC_PS,
  LTC_OBJC_PT,
  LTC_CFSTRING,
  LTC_SEL_REF,
  LTC_FIX_MAX,
} LTCFixData;

typedef enum {
  LTC_SEL_DEFAULT = 0,  // Compress and register with lock.
  LTC_SEL_FAST,         // Compress and register without lock.
  LTC_SEL_NO_COMPRESS,  // Do not compress ObjC selectors.
} LTCSelTreatment;

typedef enum {
  LTC_DECOMPRESSOR_NONE = 0,   // No embedded decompressor.
  LTC_DECOMPRESSOR_NL_OBJC,    // Use an ObjC non-lazy class load function.
  LTC_DECOMPRESSOR_INIT_FUNC,  // Use a mod-init function.
} LTCDecompressorType;

typedef struct {
  uint32_t start;
  uint32_t size;
} EncodedFixData;

typedef struct {
  char segment_name[LTC_SEGMENT_NAME_MAX];  // dest segment name
  char section_name[LTC_SECTION_NAME_MAX];  // section name without prefix
  uint8_t algorithm : 4;
  uint8_t is_merged : 4;
  uint32_t compressed_data_size;
  uint32_t decompressed_data_size;
  uintptr_t compressed_buffer_addr;
  uintptr_t decompressed_buffer_addr;
} LTCSectionMeta;

typedef struct {
  uint8_t version;
  uint8_t compressed_section_count;
  uint8_t objc_sel_treatment;
  char module_name[LTC_MODULE_NAME_MAX];

  uintptr_t cfstring_min_address;
  uintptr_t objc_sel_ref_min_address;
  uintptr_t binding_start_address;
  uintptr_t binding_end_address;
  uintptr_t cf_decompressed_buffer_address;
  uintptr_t objc_sel_ref_decompressed_buffer_address;
  uint32_t binding_count;
  uint32_t objc_pointer_target_min;
  uint32_t cfstring_count;
  uint32_t objc_sel_ref_count;
  uint32_t total_compressed_size;
  uint32_t objc_missing_pointers_count;

  EncodedFixData fix_data[LTC_FIX_MAX];
  LTCSectionMeta sections[];
} LTCMeta;

typedef struct {
  LTCDecompressorType type;
  uintptr_t func_vm_address;
} LTCDecompressorInfo;

#endif /* compress_const_section_meta_h */
