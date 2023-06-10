// SPDX-FileCopyrightText: 2012-2018 pancake <pancake@nopcode.org>
// SPDX-FileCopyrightText: 2012-2018 Fedor Sakharov <fedor.sakharov@gmail.com>
// SPDX-License-Identifier: LGPL-3.0-only

#include <rz_util/rz_buf.h>
#include <errno.h>
#include <rz_bin.h>
#include <rz_bin_dwarf.h>
#include <rz_core.h>

#define RET_VAL_IF_FAIL(x, val) \
	do { \
		if (!(x)) { \
			return (val); \
		} \
	} while (0)

#define READ8(buf) \
	(((buf) + 1 < buf_end) ? *((ut8 *)(buf)) : 0); \
	(buf)++
#define READI8(buf) \
	(((buf) + 1 < buf_end) ? *((st8 *)(buf)) : 0); \
	(buf)++
#define READ16(buf) \
	(((buf) + sizeof(ut16) < buf_end) ? rz_read_ble16(buf, big_endian) : 0); \
	(buf) += sizeof(ut16)
#define READ32(buf) \
	(((buf) + sizeof(ut32) < buf_end) ? rz_read_ble32(buf, big_endian) : 0); \
	(buf) += sizeof(ut32)
#define READ64(buf) \
	(((buf) + sizeof(ut64) < buf_end) ? rz_read_ble64(buf, big_endian) : 0); \
	(buf) += sizeof(ut64)

#define RET_FALSE_IF_FAIL(x) RET_VAL_IF_FAIL(x, false)
#define RET_NULL_IF_FAIL(x)  RET_VAL_IF_FAIL(x, NULL)
#define GOTO_IF_FAIL(x, label) \
	do { \
		if (!(x)) { \
			goto label; \
		} \
	} while (0)

#define UX_WRAP(out, x, wrap) \
	switch ((x)) { \
	case 1: \
		wrap(rz_buf_read8(buffer, (ut8 *)&(out))); \
		break; \
	case 2: \
		wrap(rz_buf_read_ble16(buffer, (ut16 *)&(out), big_endian)); \
		break; \
	case 4: \
		wrap(rz_buf_read_ble32(buffer, (ut32 *)&(out), big_endian)); \
		break; \
	case 8: \
		wrap(rz_buf_read_ble64(buffer, (ut64 *)&(out), big_endian)); \
		break; \
	default: \
		RZ_LOG_ERROR("DWARF: Unexpected pointer size: %u\n", (unsigned)(x)); \
		return false; \
	}

#define UX_WRAP1(out, x, wrap, ...) \
	switch ((x)) { \
	case 1: \
		wrap(rz_buf_read8(buffer, (ut8 *)&(out)), __VA_ARGS__); \
		break; \
	case 2: \
		wrap(rz_buf_read_ble16(buffer, (ut16 *)&(out), big_endian), __VA_ARGS__); \
		break; \
	case 4: \
		wrap(rz_buf_read_ble32(buffer, (ut32 *)&(out), big_endian), __VA_ARGS__); \
		break; \
	case 8: \
		wrap(rz_buf_read_ble64(buffer, (ut64 *)&(out), big_endian), __VA_ARGS__); \
		break; \
	default: \
		RZ_LOG_ERROR("DWARF: Unexpected pointer size: %u\n", (unsigned)(x)); \
		return false; \
	}

static char *buf_get_string(RzBuffer *buffer) {
	st64 offset = (st64)rz_buf_tell(buffer);
	RET_NULL_IF_FAIL(offset != -1);
	char *x = rz_buf_get_string(buffer, offset);
	RET_NULL_IF_FAIL(x);
	rz_buf_seek(buffer, (st64)strlen(x) + 1, SEEK_CUR);
	return x;
}

#define U8_WARP(out, warp)     warp(rz_buf_read8(buffer, (ut8 *)&(out)))
#define U16_WARP(out, warp)    warp(rz_buf_read_ble16(buffer, (ut16 *)&(out), big_endian))
#define U32_WARP(out, warp)    warp(rz_buf_read_ble32(buffer, (ut32 *)&(out), big_endian))
#define U64_WARP(out, warp)    warp(rz_buf_read_ble64(buffer, (ut64 *)&(out), big_endian))
#define ULE128_WARP(out, warp) warp(rz_buf_uleb128(buffer, (ut64 *)&(out)) > 0)
#define SLE128_WARP(out, warp) warp(rz_buf_sleb128(buffer, (st64 *)&(out)) > 0)

#define U8_WARP1(out, warp, ...)     warp(rz_buf_read8(buffer, (ut8 *)&(out)), __VA_ARGS__)
#define U16_WARP1(out, warp, ...)    warp(rz_buf_read_ble16(buffer, (ut16 *)&(out), big_endian), __VA_ARGS__)
#define U32_WARP1(out, warp, ...)    warp(rz_buf_read_ble32(buffer, (ut32 *)&(out), big_endian), __VA_ARGS__)
#define U64_WARP1(out, warp, ...)    warp(rz_buf_read_ble64(buffer, (ut64 *)&(out), big_endian), __VA_ARGS__)
#define ULE128_WARP1(out, warp, ...) warp(rz_buf_uleb128(buffer, (ut64 *)&(out) > 0), __VA_ARGS__)
#define SLE128_WARP1(out, warp, ...) warp(rz_buf_sleb128(buffer, (st64 *)&(out) > 0), __VA_ARGS__)

#define U8_OR_RET_NULL(out)     U8_WARP(out, RET_NULL_IF_FAIL)
#define U16_OR_RET_NULL(out)    U16_WARP(out, RET_NULL_IF_FAIL)
#define U32_OR_RET_NULL(out)    U32_WARP(out, RET_NULL_IF_FAIL)
#define U64_OR_RET_NULL(out)    U64_WARP(out, RET_NULL_IF_FAIL)
#define UX_OR_RET_NULL(out, x)  UX_WRAP(out, x, RET_NULL_IF_FAIL)
#define ULE128_OR_RET_NULL(out) ULE128_WARP(out, RET_NULL_IF_FAIL)
#define SLE128_OR_RET_NULL(out) SLE128_WARP(out, RET_NULL_IF_FAIL)

#define U8_OR_RET_FALSE(out)     U8_WARP(out, RET_FALSE_IF_FAIL)
#define U16_OR_RET_FALSE(out)    U16_WARP(out, RET_FALSE_IF_FAIL)
#define U32_OR_RET_FALSE(out)    U32_WARP(out, RET_FALSE_IF_FAIL)
#define U64_OR_RET_FALSE(out)    U64_WARP(out, RET_FALSE_IF_FAIL)
#define UX_OR_RET_FALSE(out, x)  UX_WRAP(out, x, RET_FALSE_IF_FAIL)
#define ULE128_OR_RET_FALSE(out) ULE128_WARP(out, RET_FALSE_IF_FAIL)
#define SLE128_OR_RET_FALSE(out) SLE128_WARP(out, RET_FALSE_IF_FAIL)

#define U8_OR_GOTO(out, label)     U8_WARP1(out, GOTO_IF_FAIL, label)
#define U16_OR_GOTO(out, label)    U8_WARP1(out, GOTO_IF_FAIL, label)
#define U32_OR_GOTO(out, label)    U8_WARP1(out, GOTO_IF_FAIL, label)
#define U64_OR_GOTO(out, label)    U8_WARP1(out, GOTO_IF_FAIL, label)
#define UX_OR_GOTO(out, x, label)  UX_WRAP1(out, x, GOTO_IF_FAIL, label)
#define ULE128_OR_GOTO(out, label) U8_WARP1(out, GOTO_IF_FAIL, label)
#define SLE128_OR_GOTO(out, label) U8_WARP1(out, GOTO_IF_FAIL, label)

static const char *dwarf_tag_name_encodings[] = {
	[DW_TAG_null_entry] = "DW_TAG_null_entry",
	[DW_TAG_array_type] = "DW_TAG_array_type",
	[DW_TAG_class_type] = "DW_TAG_class_type",
	[DW_TAG_entry_point] = "DW_TAG_entry_point",
	[DW_TAG_enumeration_type] = "DW_TAG_enumeration_type",
	[DW_TAG_formal_parameter] = "DW_TAG_formal_parameter",
	[DW_TAG_imported_declaration] = "DW_TAG_imported_declaration",
	[DW_TAG_label] = "DW_TAG_label",
	[DW_TAG_lexical_block] = "DW_TAG_lexical_block",
	[DW_TAG_member] = "DW_TAG_member",
	[DW_TAG_pointer_type] = "DW_TAG_pointer_type",
	[DW_TAG_reference_type] = "DW_TAG_reference_type",
	[DW_TAG_compile_unit] = "DW_TAG_compile_unit",
	[DW_TAG_string_type] = "DW_TAG_string_type",
	[DW_TAG_structure_type] = "DW_TAG_structure_type",
	[DW_TAG_subroutine_type] = "DW_TAG_subroutine_type",
	[DW_TAG_typedef] = "DW_TAG_typedef",
	[DW_TAG_union_type] = "DW_TAG_union_type",
	[DW_TAG_unspecified_parameters] = "DW_TAG_unspecified_parameters",
	[DW_TAG_variant] = "DW_TAG_variant",
	[DW_TAG_common_block] = "DW_TAG_common_block",
	[DW_TAG_common_inclusion] = "DW_TAG_common_inclusion",
	[DW_TAG_inheritance] = "DW_TAG_inheritance",
	[DW_TAG_inlined_subroutine] = "DW_TAG_inlined_subroutine",
	[DW_TAG_module] = "DW_TAG_module",
	[DW_TAG_ptr_to_member_type] = "DW_TAG_ptr_to_member_type",
	[DW_TAG_set_type] = "DW_TAG_set_type",
	[DW_TAG_subrange_type] = "DW_TAG_subrange_type",
	[DW_TAG_with_stmt] = "DW_TAG_with_stmt",
	[DW_TAG_access_declaration] = "DW_TAG_access_declaration",
	[DW_TAG_base_type] = "DW_TAG_base_type",
	[DW_TAG_catch_block] = "DW_TAG_catch_block",
	[DW_TAG_const_type] = "DW_TAG_const_type",
	[DW_TAG_constant] = "DW_TAG_constant",
	[DW_TAG_enumerator] = "DW_TAG_enumerator",
	[DW_TAG_file_type] = "DW_TAG_file_type",
	[DW_TAG_friend] = "DW_TAG_friend",
	[DW_TAG_namelist] = "DW_TAG_namelist",
	[DW_TAG_namelist_item] = "DW_TAG_namelist_item",
	[DW_TAG_packed_type] = "DW_TAG_packed_type",
	[DW_TAG_subprogram] = "DW_TAG_subprogram",
	[DW_TAG_template_type_param] = "DW_TAG_template_type_param",
	[DW_TAG_template_value_param] = "DW_TAG_template_value_param",
	[DW_TAG_thrown_type] = "DW_TAG_thrown_type",
	[DW_TAG_try_block] = "DW_TAG_try_block",
	[DW_TAG_variant_part] = "DW_TAG_variant_part",
	[DW_TAG_variable] = "DW_TAG_variable",
	[DW_TAG_volatile_type] = "DW_TAG_volatile_type",
	[DW_TAG_dwarf_procedure] = "DW_TAG_dwarf_procedure",
	[DW_TAG_restrict_type] = "DW_TAG_restrict_type",
	[DW_TAG_interface_type] = "DW_TAG_interface_type",
	[DW_TAG_namespace] = "DW_TAG_namespace",
	[DW_TAG_imported_module] = "DW_TAG_imported_module",
	[DW_TAG_unspecified_type] = "DW_TAG_unspecified_type",
	[DW_TAG_partial_unit] = "DW_TAG_partial_unit",
	[DW_TAG_imported_unit] = "DW_TAG_imported_unit",
	[DW_TAG_mutable_type] = "DW_TAG_mutable_type",
	[DW_TAG_condition] = "DW_TAG_condition",
	[DW_TAG_shared_type] = "DW_TAG_shared_type",
	[DW_TAG_type_unit] = "DW_TAG_type_unit",
	[DW_TAG_rvalue_reference_type] = "DW_TAG_rvalue_reference_type",
	[DW_TAG_template_alias] = "DW_TAG_template_alias",
	[DW_TAG_LAST] = "DW_TAG_LAST",
};

static const char *dwarf_attr_encodings[] = {
	[DW_AT_sibling] = "DW_AT_siblings",
	[DW_AT_location] = "DW_AT_location",
	[DW_AT_name] = "DW_AT_name",
	[DW_AT_ordering] = "DW_AT_ordering",
	[DW_AT_byte_size] = "DW_AT_byte_size",
	[DW_AT_bit_size] = "DW_AT_bit_size",
	[DW_AT_stmt_list] = "DW_AT_stmt_list",
	[DW_AT_low_pc] = "DW_AT_low_pc",
	[DW_AT_high_pc] = "DW_AT_high_pc",
	[DW_AT_language] = "DW_AT_language",
	[DW_AT_discr] = "DW_AT_discr",
	[DW_AT_discr_value] = "DW_AT_discr_value",
	[DW_AT_visibility] = "DW_AT_visibility",
	[DW_AT_import] = "DW_AT_import",
	[DW_AT_string_length] = "DW_AT_string_length",
	[DW_AT_common_reference] = "DW_AT_common_reference",
	[DW_AT_comp_dir] = "DW_AT_comp_dir",
	[DW_AT_const_value] = "DW_AT_const_value",
	[DW_AT_containing_type] = "DW_AT_containing_type",
	[DW_AT_default_value] = "DW_AT_default_value",
	[DW_AT_inline] = "DW_AT_inline",
	[DW_AT_is_optional] = "DW_AT_is_optional",
	[DW_AT_lower_bound] = "DW_AT_lower_bound",
	[DW_AT_producer] = "DW_AT_producer",
	[DW_AT_prototyped] = "DW_AT_prototyped",
	[DW_AT_return_addr] = "DW_AT_return_addr",
	[DW_AT_start_scope] = "DW_AT_start_scope",
	[DW_AT_stride_size] = "DW_AT_stride_size",
	[DW_AT_upper_bound] = "DW_AT_upper_bound",
	[DW_AT_abstract_origin] = "DW_AT_abstract_origin",
	[DW_AT_accessibility] = "DW_AT_accessibility",
	[DW_AT_address_class] = "DW_AT_address_class",
	[DW_AT_artificial] = "DW_AT_artificial",
	[DW_AT_base_types] = "DW_AT_base_types",
	[DW_AT_calling_convention] = "DW_AT_calling_convention",
	[DW_AT_count] = "DW_AT_count",
	[DW_AT_data_member_location] = "DW_AT_data_member_location",
	[DW_AT_decl_column] = "DW_AT_decl_column",
	[DW_AT_decl_file] = "DW_AT_decl_file",
	[DW_AT_decl_line] = "DW_AT_decl_line",
	[DW_AT_declaration] = "DW_AT_declaration",
	[DW_AT_discr_list] = "DW_AT_discr_list",
	[DW_AT_encoding] = "DW_AT_encoding",
	[DW_AT_external] = "DW_AT_external",
	[DW_AT_frame_base] = "DW_AT_frame_base",
	[DW_AT_friend] = "DW_AT_friend",
	[DW_AT_identifier_case] = "DW_AT_identifier_case",
	[DW_AT_macro_info] = "DW_AT_macro_info",
	[DW_AT_namelist_item] = "DW_AT_namelist_item",
	[DW_AT_priority] = "DW_AT_priority",
	[DW_AT_segment] = "DW_AT_segment",
	[DW_AT_specification] = "DW_AT_specification",
	[DW_AT_static_link] = "DW_AT_static_link",
	[DW_AT_type] = "DW_AT_type",
	[DW_AT_use_location] = "DW_AT_use_location",
	[DW_AT_variable_parameter] = "DW_AT_variable_parameter",
	[DW_AT_virtuality] = "DW_AT_virtuality",
	[DW_AT_vtable_elem_location] = "DW_AT_vtable_elem_location",
	[DW_AT_allocated] = "DW_AT_allocated",
	[DW_AT_associated] = "DW_AT_associated",
	[DW_AT_data_location] = "DW_AT_data_location",
	[DW_AT_byte_stride] = "DW_AT_byte_stride",
	[DW_AT_entry_pc] = "DW_AT_entry_pc",
	[DW_AT_use_UTF8] = "DW_AT_use_UTF8",
	[DW_AT_extension] = "DW_AT_extension",
	[DW_AT_ranges] = "DW_AT_ranges",
	[DW_AT_trampoline] = "DW_AT_trampoline",
	[DW_AT_call_column] = "DW_AT_call_column",
	[DW_AT_call_file] = "DW_AT_call_file",
	[DW_AT_call_line] = "DW_AT_call_line",
	[DW_AT_description] = "DW_AT_description",
	[DW_AT_binary_scale] = "DW_AT_binary_scale",
	[DW_AT_decimal_scale] = "DW_AT_decimal_scale",
	[DW_AT_small] = "DW_AT_small",
	[DW_AT_decimal_sign] = "DW_AT_decimal_sign",
	[DW_AT_digit_count] = "DW_AT_digit_count",
	[DW_AT_picture_string] = "DW_AT_picture_string",
	[DW_AT_mutable] = "DW_AT_mutable",
	[DW_AT_threads_scaled] = "DW_AT_threads_scaled",
	[DW_AT_explicit] = "DW_AT_explicit",
	[DW_AT_object_pointer] = "DW_AT_object_pointer",
	[DW_AT_endianity] = "DW_AT_endianity",
	[DW_AT_elemental] = "DW_AT_elemental",
	[DW_AT_pure] = "DW_AT_pure",
	[DW_AT_recursive] = "DW_AT_recursive",
	[DW_AT_signature] = "DW_AT_signature",
	[DW_AT_main_subprogram] = "DW_AT_main_subprogram",
	[DW_AT_data_bit_offset] = "DW_AT_data_big_offset",
	[DW_AT_const_expr] = "DW_AT_const_expr",
	[DW_AT_enum_class] = "DW_AT_enum_class",
	[DW_AT_linkage_name] = "DW_AT_linkage_name",
	[DW_AT_string_length_bit_size] = "DW_AT_string_length_bit_size",
	[DW_AT_string_length_byte_size] = "DW_AT_string_length_byte_size",
	[DW_AT_rank] = "DW_AT_rank",
	[DW_AT_str_offsets_base] = "DW_AT_str_offsets_base",
	[DW_AT_addr_base] = "DW_AT_addr_base",
	[DW_AT_rnglists_base] = "DW_AT_rnglists_base",
	[DW_AT_dwo_name] = "DW_AT_dwo_name",
	[DW_AT_reference] = "DW_AT_reference",
	[DW_AT_rvalue_reference] = "DW_AT_rvalue_reference",
	[DW_AT_macros] = "DW_AT_macros",
	[DW_AT_call_all_calls] = "DW_AT_call_all_calls",
	[DW_AT_call_all_source_calls] = "DW_AT_call_all_source_calls",
	[DW_AT_call_all_tail_calls] = "DW_AT_call_all_tail_calls",
	[DW_AT_call_return_pc] = "DW_AT_call_return_pc",
	[DW_AT_call_value] = "DW_AT_call_value",
	[DW_AT_call_origin] = "DW_AT_call_origin",
	[DW_AT_call_parameter] = "DW_AT_call_parameter",
	[DW_AT_call_pc] = "DW_AT_call_pc",
	[DW_AT_call_tail_call] = "DW_AT_call_tail_call",
	[DW_AT_call_target] = "DW_AT_call_target",
	[DW_AT_call_target_clobbered] = "DW_AT_call_target_clobbered",
	[DW_AT_call_data_location] = "DW_AT_call_data_location",
	[DW_AT_call_data_value] = "DW_AT_call_data_value",
	[DW_AT_noreturn] = "DW_AT_noreturn",
	[DW_AT_alignment] = "DW_AT_alignment",
	[DW_AT_export_symbols] = "DW_AT_export_symbols",
	[DW_AT_deleted] = "DW_AT_deleted",
	[DW_AT_defaulted] = "DW_AT_defaulted",
	[DW_AT_loclists_base] = "DW_AT_loclists_base"
};

static const char *dwarf_attr_form_encodings[] = {
	[DW_FORM_addr] = "DW_FORM_addr",
	[DW_FORM_block2] = "DW_FORM_block2",
	[DW_FORM_block4] = "DW_FORM_block4",
	[DW_FORM_data2] = "DW_FORM_data2",
	[DW_FORM_data4] = "DW_FORM_data4",
	[DW_FORM_data8] = "DW_FORM_data8",
	[DW_FORM_string] = "DW_FORM_string",
	[DW_FORM_block] = "DW_FORM_block",
	[DW_FORM_block1] = "DW_FORM_block1",
	[DW_FORM_data1] = "DW_FORM_data1",
	[DW_FORM_flag] = "DW_FORM_flag",
	[DW_FORM_sdata] = "DW_FORM_sdata",
	[DW_FORM_strp] = "DW_FORM_strp",
	[DW_FORM_udata] = "DW_FORM_udata",
	[DW_FORM_ref_addr] = "DW_FORM_ref_addr",
	[DW_FORM_ref1] = "DW_FORM_ref1",
	[DW_FORM_ref2] = "DW_FORM_ref2",
	[DW_FORM_ref4] = "DW_FORM_ref4",
	[DW_FORM_ref8] = "DW_FORM_ref8",
	[DW_FORM_ref_udata] = "DW_FORM_ref_udata",
	[DW_FORM_indirect] = "DW_FORM_indirect",
	[DW_FORM_sec_offset] = "DW_FORM_sec_offset",
	[DW_FORM_exprloc] = "DW_FORM_exprloc",
	[DW_FORM_flag_present] = "DW_FORM_flag_present",
	[DW_FORM_strx] = "DW_FORM_strx",
	[DW_FORM_addrx] = "DW_FORM_addrx",
	[DW_FORM_ref_sup4] = "DW_FORM_ref_sup4",
	[DW_FORM_strp_sup] = "DW_FORM_strp_sup",
	[DW_FORM_data16] = "DW_FORM_data16",
	[DW_FORM_line_ptr] = "DW_FORM_line_ptr",
	[DW_FORM_ref_sig8] = "DW_FORM_ref_sig8",
	[DW_FORM_implicit_const] = "DW_FORM_implicit_const",
	[DW_FORM_loclistx] = "DW_FORM_loclistx",
	[DW_FORM_rnglistx] = "DW_FORM_rnglistx",
	[DW_FORM_ref_sup8] = "DW_FORM_ref_sup8",
	[DW_FORM_strx1] = "DW_FORM_strx1",
	[DW_FORM_strx2] = "DW_FORM_strx2",
	[DW_FORM_strx3] = "DW_FORM_strx3",
	[DW_FORM_strx4] = "DW_FORM_strx4",
	[DW_FORM_addrx1] = "DW_FORM_addrx1",
	[DW_FORM_addrx2] = "DW_FORM_addrx2",
	[DW_FORM_addrx3] = "DW_FORM_addrx3",
	[DW_FORM_addrx4] = "DW_FORM_addrx4",
};

static const char *dwarf_langs[] = {
	[DW_LANG_C89] = "C89",
	[DW_LANG_C] = "C",
	[DW_LANG_Ada83] = "Ada83",
	[DW_LANG_C_plus_plus] = "C++",
	[DW_LANG_Cobol74] = "Cobol74",
	[DW_LANG_Cobol85] = "Cobol85",
	[DW_LANG_Fortran77] = "Fortran77",
	[DW_LANG_Fortran90] = "Fortran90",
	[DW_LANG_Pascal83] = "Pascal83",
	[DW_LANG_Modula2] = "Modula2",
	[DW_LANG_Java] = "Java",
	[DW_LANG_C99] = "C99",
	[DW_LANG_Ada95] = "Ada95",
	[DW_LANG_Fortran95] = "Fortran95",
	[DW_LANG_PLI] = "PLI",
	[DW_LANG_ObjC] = "ObjC",
	[DW_LANG_ObjC_plus_plus] = "ObjC_plus_plus",
	[DW_LANG_UPC] = "UPC",
	[DW_LANG_D] = "D",
	[DW_LANG_Python] = "Python",
	[DW_LANG_Rust] = "Rust",
	[DW_LANG_C11] = "C11",
	[DW_LANG_Swift] = "Swift",
	[DW_LANG_Julia] = "Julia",
	[DW_LANG_Dylan] = "Dylan",
	[DW_LANG_C_plus_plus_14] = "C++14",
	[DW_LANG_Fortran03] = "Fortran03",
	[DW_LANG_Fortran08] = "Fortran08",
	[DW_LANG_Mips_Assembler] = "DW_LANG_Mips_Assembler",
	[DW_LANG_GOOGLE_RenderScript] = "DW_LANG_GOOGLE_RenderScript",
	[DW_LANG_SUN_Assembler] = "DW_LANG_SUN_Assembler",
	[DW_LANG_ALTIUM_Assembler] = "DW_LANG_ALTIUM_Assembler",
	[DW_LANG_BORLAND_Delphi] = "DW_LANG_BORLAND_Delphi",
};

static const char *dwarf_unit_types[] = {
	[DW_UT_compile] = "DW_UT_compile",
	[DW_UT_type] = "DW_UT_type",
	[DW_UT_partial] = "DW_UT_partial",
	[DW_UT_skeleton] = "DW_UT_skeleton",
	[DW_UT_split_compile] = "DW_UT_split_compile",
	[DW_UT_split_type] = "DW_UT_split_type",
	[DW_UT_lo_user] = "DW_UT_lo_user",
	[DW_UT_hi_user] = "DW_UT_hi_user",
};

RZ_API const char *rz_bin_dwarf_tag(enum DW_TAG tag) {
	if (tag >= DW_TAG_LAST) {
		return NULL;
	}
	return dwarf_tag_name_encodings[tag];
}

RZ_API const char *rz_bin_dwarf_attr(enum DW_AT attr_code) {
	if (attr_code < RZ_ARRAY_SIZE(dwarf_attr_encodings)) {
		return dwarf_attr_encodings[attr_code];
	}
	// the below codes are much sparser, so putting them in an array would require a lot of
	// unused memory
	switch (attr_code) {
	case DW_AT_lo_user:
		return "DW_AT_lo_user";
	case DW_AT_MIPS_linkage_name:
		return "DW_AT_MIPS_linkage_name";
	case DW_AT_GNU_call_site_value:
		return "DW_AT_GNU_call_site_value";
	case DW_AT_GNU_call_site_data_value:
		return "DW_AT_GNU_call_site_data_value";
	case DW_AT_GNU_call_site_target:
		return "DW_AT_GNU_call_site_target";
	case DW_AT_GNU_call_site_target_clobbered:
		return "DW_AT_GNU_call_site_target_clobbered";
	case DW_AT_GNU_tail_call:
		return "DW_AT_GNU_tail_call";
	case DW_AT_GNU_all_tail_call_sites:
		return "DW_AT_GNU_all_tail_call_sites";
	case DW_AT_GNU_all_call_sites:
		return "DW_AT_GNU_all_call_sites";
	case DW_AT_GNU_all_source_call_sites:
		return "DW_AT_GNU_all_source_call_sites";
	case DW_AT_GNU_macros:
		return "DW_AT_GNU_macros";
	case DW_AT_GNU_deleted:
		return "DW_AT_GNU_deleted";
	case DW_AT_GNU_dwo_name:
		return "DW_AT_GNU_dwo_name";
	case DW_AT_GNU_dwo_id:
		return "DW_AT_GNU_dwo_id";
	case DW_AT_GNU_ranges_base:
		return "DW_AT_GNU_ranges_base";
	case DW_AT_GNU_addr_base:
		return "DW_AT_GNU_addr_base";
	case DW_AT_GNU_pubnames:
		return "DW_AT_GNU_pubnames";
	case DW_AT_GNU_pubtypes:
		return "DW_AT_GNU_pubtypes";
	case DW_AT_hi_user:
		return "DW_AT_hi_user";
	default:
		return NULL;
	}
}

RZ_API const char *rz_bin_dwarf_form(enum DW_FORM form_code) {
	if (form_code < DW_FORM_addr || form_code > DW_FORM_addrx4) {
		return NULL;
	}
	return dwarf_attr_form_encodings[form_code];
}

RZ_API const char *rz_bin_dwarf_unit_type(enum DW_UT unit_type) {
	if (!unit_type || unit_type > DW_UT_split_type) {
		return NULL;
	}
	return dwarf_unit_types[unit_type];
}

RZ_API const char *rz_bin_dwarf_lang(enum DW_LANG lang) {
	if (lang >= RZ_ARRAY_SIZE(dwarf_langs)) {
		return NULL;
	}
	return dwarf_langs[lang];
}

static const char *dwarf_children[] = {
	[DW_CHILDREN_yes] = "DW_CHILDREN_yes",
	[DW_CHILDREN_no] = "DW_CHILDREN_no",
};

RZ_API const char *rz_bin_dwarf_children(enum DW_CHILDREN lang) {
	if (lang >= RZ_ARRAY_SIZE(dwarf_children)) {
		return NULL;
	}
	return dwarf_children[lang];
}

/**
 * \brief Read an "initial length" value, as specified by dwarf.
 * This also determines whether it is 64bit or 32bit and reads 4 or 12 bytes respectively.
 */
static inline ut64 dwarf_read_initial_length(RZ_OUT bool *is_64bit, bool big_endian, const ut8 **buf, const ut8 *buf_end) {
	static const ut64 DWARF32_UNIT_LENGTH_MAX = 0xfffffff0;
	static const ut64 DWARF64_UNIT_LENGTH_INI = 0xffffffff;
	ut64 r = READ32(*buf);
	if (r <= DWARF32_UNIT_LENGTH_MAX) {
		*is_64bit = false;
		return r;
	} else if (r == DWARF64_UNIT_LENGTH_INI) {
		*is_64bit = true;
		return READ64(*buf);
	} else {
		RZ_LOG_ERROR("Invalid initial length: 0x%" PFMT64x "\n", r);
	}
	return r;
}

/**
 * \brief Read an "initial length" value, as specified by dwarf.
 * This also determines whether it is 64bit or 32bit and reads 4 or 12 bytes respectively.
 */
static bool read_initial_length(RzBuffer *buffer, RZ_OUT bool *is_64bit, ut64 *out, bool big_endian) {
	static const ut64 DWARF32_UNIT_LENGTH_MAX = 0xfffffff0;
	static const ut64 DWARF64_UNIT_LENGTH_INI = 0xffffffff;
	ut32 x32;
	if (!rz_buf_read_ble32(buffer, &x32, big_endian)) {
		return false;
	}
	if (x32 <= DWARF32_UNIT_LENGTH_MAX) {
		*is_64bit = false;
		*out = x32;
	} else if (x32 == DWARF64_UNIT_LENGTH_INI) {
		ut64 x64;
		if (!rz_buf_read_ble64(buffer, &x64, big_endian)) {
			return false;
		}
		*is_64bit = true;
		*out = x64;
	} else {
		RZ_LOG_ERROR("Invalid initial length: 0x%" PFMT32x "\n", x32);
	}
	return true;
}

/**
 * @brief Reads 64/32 bit unsigned based on format
 *
 * @param is_64bit Format of the comp unit
 * @param buf Pointer to the buffer to read from, to update after read
 * @param buf_end To check the boundary /for READ macro/
 * @return ut64 Read value
 */
static inline ut64 dwarf_read_offset(bool is_64bit, bool big_endian, const ut8 **buf, const ut8 *buf_end) {
	ut64 result;
	if (is_64bit) {
		result = READ64(*buf);
	} else {
		result = READ32(*buf);
	}
	return result;
}

static inline bool read_offset(RzBuffer *buffer, ut64 *out, bool is_64bit, bool big_endian) {
	if (is_64bit) {
		ut64 result;
		U64_OR_RET_FALSE(result);
		*out = result;
	} else {
		ut32 result;
		U32_OR_RET_FALSE(result);
		*out = result;
	}
	return true;
}

static inline ut64 dwarf_read_address(size_t size, bool big_endian, const ut8 **buf, const ut8 *buf_end) {
	ut64 result;
	switch (size) {
	case 2:
		result = READ16(*buf);
		break;
	case 4:
		result = READ32(*buf);
		break;
	case 8:
		result = READ64(*buf);
		break;
	default:
		result = 0;
		*buf += size;
		RZ_LOG_WARN("Weird dwarf address size: %zu.", size);
	}
	return result;
}

void rz_bin_dwarf_line_file_entry_fini(RzBinDwarfLineFileEntry *x, void *user) {
	if (!x) {
		return;
	}
	free(x->name);
	free(x->include_dir);
}

static void line_header_init(RzBinDwarfLineHeader *hdr) {
	if (!hdr) {
		return;
	}
	memset(hdr, 0, sizeof(*hdr));
	rz_vector_init(&hdr->file_name_entry_formats, sizeof(RzBinDwarfFileEntryFormat), NULL, NULL);
	rz_vector_init(&hdr->file_names, sizeof(RzBinDwarfLineFileEntry), (RzVectorFree)rz_bin_dwarf_line_file_entry_fini, NULL);
	rz_vector_init(&hdr->directory_entry_formats, sizeof(RzBinDwarfFileEntryFormat), NULL, NULL);
	rz_pvector_init(&hdr->directories, free);
}

static void line_header_fini(RzBinDwarfLineHeader *hdr) {
	if (!hdr) {
		return;
	}
	rz_vector_fini(&hdr->file_name_entry_formats);
	rz_vector_fini(&hdr->file_names);
	rz_vector_fini(&hdr->directory_entry_formats);
	rz_pvector_fini(&hdr->directories);
}

static bool file_entry_format_parse(RzBinDwarfLineHeader *hdr, RzBuffer *buffer) {
	ut8 count = 0;
	RET_FALSE_IF_FAIL(rz_buf_read8(buffer, &count));
	rz_vector_reserve(&hdr->file_name_entry_formats, count);
	ut32 path_count = 0;
	for (ut8 i = 0; i < count; ++i) {
		ut64 content_type = 0;
		ut64 form = 0;
		RET_FALSE_IF_FAIL(rz_buf_uleb128(buffer, (unsigned long long int *)&content_type));
		RET_FALSE_IF_FAIL(rz_buf_uleb128(buffer, &form));
		if (form > UT16_MAX) {
			RZ_LOG_ERROR("invalid file entry format form %" PFMT64x "\n", form);
			return false;
		}

		if (content_type == DW_LNCT_path) {
			path_count += 1;
		}

		RzBinDwarfFileEntryFormat format = {
			.content_type = RZ_MIN(UT16_MAX, content_type),
			.form = form,
		};
		rz_vector_push(&hdr->file_name_entry_formats, &format);
	}
	if (path_count != 1) {
		RZ_LOG_ERROR("Missing file entry format path");
		return false;
	}
	return true;
}

// static char *parse_directory_v5(RzBinDwarfLineHeader *hdr, RzBuffer *buffer) {
//	char *path_name = NULL;
//	void *it;
//	rz_vector_foreach(&hdr->file_name_entry_formats, it) {
//		RzBinDwarfFileEntryFormat *format = it;
//		//		parse_attr_value(buffer, format->form);
//		//		if (format->content_type == DW_LNCT_path) {
//		//			path_name = value;
//		//		}
//	}
//	return path_name;
// }

/**
 * 6.2.4 The Line Number Program Header: https://dwarfstd.org/doc/DWARF5.pdf#page=172
 */
static const ut8 *parse_line_header_source(RzBinFile *bf, const ut8 *buf, const ut8 *buf_end, RzBinDwarfLineHeader *hdr) {
	if (hdr->version <= 4) {
		while (buf + 1 < buf_end) {
			size_t maxlen = RZ_MIN((size_t)(buf_end - buf) - 1, 0xfff);
			size_t len = rz_str_nlen((const char *)buf, maxlen);
			char *str = rz_str_ndup((const char *)buf, len);
			if (len < 1 || len >= 0xfff || !str) {
				buf += 1;
				free(str);
				break;
			}
			rz_pvector_push(&hdr->directories, str);
			buf += len + 1;
		}
	} else {
		RzBuffer *buffer = rz_buf_new_with_bytes(buf, buf_end - buf);
		file_entry_format_parse(hdr, buffer);
		ut64 count = 0;
		RET_NULL_IF_FAIL(rz_buf_uleb128(buffer, &count));
		rz_buf_free(buffer);
		//		for (ut64 i = 0; i < count; ++i) {
		//		}
	}

	rz_vector_init(&hdr->file_names, sizeof(RzBinDwarfLineFileEntry), NULL, NULL);
	while (buf + 1 < buf_end) {
		const char *filename = (const char *)buf;
		size_t maxlen = RZ_MIN((size_t)(buf_end - buf - 1), 0xfff);
		ut64 id_idx, mod_time, file_len;
		int len = rz_str_nlen(filename, maxlen);

		if (!len) {
			buf++;
			break;
		}
		buf += len + 1;
		if (buf >= buf_end) {
			buf = NULL;
			goto beach;
		}
		buf = rz_uleb128(buf, buf_end - buf, &id_idx, NULL);
		if (buf >= buf_end) {
			buf = NULL;
			goto beach;
		}
		buf = rz_uleb128(buf, buf_end - buf, &mod_time, NULL);
		if (buf >= buf_end) {
			buf = NULL;
			goto beach;
		}
		buf = rz_uleb128(buf, buf_end - buf, &file_len, NULL);
		if (buf >= buf_end) {
			buf = NULL;
			goto beach;
		}
		RzBinDwarfLineFileEntry *entry = rz_vector_push(&hdr->file_names, NULL);
		entry->name = rz_str_ndup(filename, len);
		entry->id_idx = id_idx;
		entry->mod_time = mod_time;
		entry->file_len = file_len;
	}
beach:
	return buf;
}

/**
 * \param info if not NULL, filenames can get resolved to absolute paths using the compilation unit dirs from it
 */
RZ_API char *rz_bin_dwarf_line_header_get_full_file_path(RZ_NULLABLE const RzBinDwarfDebugInfo *info, const RzBinDwarfLineHeader *hdr, ut64 file_index) {
	rz_return_val_if_fail(hdr, NULL);
	if (file_index >= rz_vector_len(&hdr->file_names)) {
		return NULL;
	}
	RzBinDwarfLineFileEntry *file = rz_vector_index_ptr(&hdr->file_names, file_index);
	if (!file->name) {
		return NULL;
	}

	/*
	 * Dwarf standard does not seem to specify the exact separator (slash/backslash) of paths
	 * so apparently it is target-dependent. However we have yet to see a Windows binary that
	 * also contains dwarf and contains backslashes. The ones we have seen from MinGW have regular
	 * slashes.
	 * And since there seems to be no way to reliable check whether the target uses slashes
	 * or backslashes anyway, we will simply use slashes always here.
	 */

	const char *comp_dir = info ? ht_up_find(info->line_info_offset_comp_dir, hdr->offset, NULL) : NULL;
	const char *include_dir = NULL;
	char *own_str = NULL;
	if (file->id_idx > 0 && file->id_idx - 1 < rz_pvector_len(&hdr->directories)) {
		include_dir = rz_pvector_at(&hdr->directories, file->id_idx - 1);
		if (include_dir && include_dir[0] != '/' && comp_dir) {
			include_dir = own_str = rz_str_newf("%s/%s/", comp_dir, include_dir);
		}
	} else {
		include_dir = comp_dir;
	}
	if (!include_dir) {
		include_dir = "./";
	}
	char *r = rz_str_newf("%s/%s", include_dir, file->name);
	free(own_str);
	return r;
}

static const char *get_full_file_path(const RzBinDwarfDebugInfo *info, const RzBinDwarfLineHeader *hdr,
	RZ_NULLABLE RzBinDwarfLineFileCache *cache, ut64 file_index) {
	if (file_index >= rz_vector_len(&hdr->file_names)) {
		return NULL;
	}
	if (!cache) {
		return ((RzBinDwarfLineFileEntry *)rz_vector_index_ptr(&hdr->file_names, file_index))->name;
	}
	char *path = rz_pvector_at(cache, file_index);
	if (!path) {
		path = rz_bin_dwarf_line_header_get_full_file_path(info, hdr, file_index);
		rz_pvector_set(cache, file_index, path);
	}
	return path;
}

RZ_API ut64 rz_bin_dwarf_line_header_get_adj_opcode(const RzBinDwarfLineHeader *hdr, ut8 opcode) {
	rz_return_val_if_fail(hdr, 0);
	return opcode - hdr->opcode_base;
}

RZ_API ut64 rz_bin_dwarf_line_header_get_spec_op_advance_pc(const RzBinDwarfLineHeader *hdr, ut8 opcode) {
	rz_return_val_if_fail(hdr, 0);
	if (!hdr->line_range) {
		// to dodge division by zero
		return 0;
	}
	ut8 adj_opcode = rz_bin_dwarf_line_header_get_adj_opcode(hdr, opcode);
	int op_advance = adj_opcode / hdr->line_range;
	if (hdr->max_ops_per_inst == 1) {
		return op_advance * hdr->min_inst_len;
	}
	return hdr->min_inst_len * (op_advance / hdr->max_ops_per_inst);
}

RZ_API st64 rz_bin_dwarf_line_header_get_spec_op_advance_line(const RzBinDwarfLineHeader *hdr, ut8 opcode) {
	rz_return_val_if_fail(hdr, 0);
	if (!hdr->line_range) {
		// to dodge division by zero
		return 0;
	}
	ut8 adj_opcode = rz_bin_dwarf_line_header_get_adj_opcode(hdr, opcode);
	return hdr->line_base + (adj_opcode % hdr->line_range);
}

static const ut8 *parse_line_header(
	RzBinFile *bf, const ut8 *buf, const ut8 *buf_end,
	RzBinDwarfLineHeader *hdr, ut64 offset_cur, bool big_endian) {
	rz_return_val_if_fail(hdr && bf && buf && buf_end, NULL);

	line_header_init(hdr);
	hdr->offset = offset_cur;
	hdr->is_64bit = false;
	hdr->unit_length = dwarf_read_initial_length(&hdr->is_64bit, big_endian, &buf, buf_end);

	hdr->version = READ16(buf);
	if (hdr->version < 2 || hdr->version > 5) {
		RZ_LOG_VERBOSE("DWARF line hdr version %d is not supported\n", hdr->version);
		return NULL;
	}
	if (hdr->version == 5) {
		hdr->address_size = READ8(buf);
		hdr->segment_selector_size = READ8(buf);
		if (hdr->segment_selector_size != 0) {
			RZ_LOG_ERROR("DWARF line hdr segment selector size %d is not supported\n", hdr->segment_selector_size);
			return NULL;
		}
	} else if (hdr->version < 5) {
		// Dwarf < 5 needs this size to be supplied from outside
		RzBinObject *o = bf->o;
		hdr->address_size = o && o->info && o->info->bits ? o->info->bits / 8 : 4;
	}

	hdr->header_length = dwarf_read_offset(hdr->is_64bit, big_endian, &buf, buf_end);

	hdr->min_inst_len = READ8(buf);
	if (hdr->min_inst_len == 0) {
		RZ_LOG_VERBOSE("DWARF line hdr min inst len %d is not supported\n", hdr->min_inst_len);
		return NULL;
	}

	if (hdr->version >= 4) {
		hdr->max_ops_per_inst = READ8(buf);
	} else {
		hdr->max_ops_per_inst = 1;
	}
	if (hdr->max_ops_per_inst == 0) {
		RZ_LOG_VERBOSE("DWARF line hdr max ops per inst %d is not supported\n", hdr->max_ops_per_inst);
		return NULL;
	}

	hdr->default_is_stmt = READ8(buf);
	hdr->line_base = READI8(buf);
	hdr->line_range = READ8(buf);
	if (hdr->line_range == 0) {
		RZ_LOG_ERROR("DWARF line hdr line range %d is not supported\n", hdr->line_range);
		return NULL;
	}

	hdr->opcode_base = READ8(buf);
	assert(hdr->opcode_base != 0);
	if (hdr->opcode_base > 1) {
		assert(buf_end - buf >= hdr->opcode_base - 1);
		hdr->std_opcode_lengths = calloc(sizeof(ut8), hdr->opcode_base - 1);
		memcpy(hdr->std_opcode_lengths, buf, hdr->opcode_base - 1);
		buf += hdr->opcode_base - 1;
	} else {
		hdr->std_opcode_lengths = NULL;
	}

	return parse_line_header_source(bf, buf, buf_end, hdr);
}

RZ_API void rz_bin_dwarf_line_op_fini(RzBinDwarfLineOp *op) {
	rz_return_if_fail(op);
	if (op->type == RZ_BIN_DWARF_LINE_OP_TYPE_EXT && op->ext_opcode == DW_LNE_define_file) {
		free(op->args.define_file.filename);
	}
}

static const ut8 *parse_ext_opcode(RzBinDwarfLineOp *op, const RzBinDwarfLineHeader *hdr, const ut8 *obuf, size_t len,
	bool big_endian) {
	rz_return_val_if_fail(op && hdr && obuf, NULL);
	const ut8 *buf = obuf;
	const ut8 *buf_end = obuf + len;

	ut64 op_len;
	buf = rz_uleb128(buf, len, &op_len, NULL);
	// op_len must fit and be at least 1 (for the opcode byte)
	if (!buf || buf >= buf_end || !op_len || buf_end - buf < op_len) {
		return NULL;
	}

	op->ext_opcode = *buf++;
	op->type = RZ_BIN_DWARF_LINE_OP_TYPE_EXT;

	switch (op->ext_opcode) {
	case DW_LNE_set_address: {
		op->args.set_address = dwarf_read_address(hdr->address_size, big_endian, &buf, buf_end);
		break;
	}
	case DW_LNE_define_file: {
		if (hdr->version <= 4) {
			size_t fn_len = rz_str_nlen((const char *)buf, buf_end - buf);
			char *fn = malloc(fn_len + 1);
			if (!fn) {
				return NULL;
			}
			memcpy(fn, buf, fn_len);
			fn[fn_len] = 0;
			op->args.define_file.filename = fn;
			buf += fn_len + 1;
			if (buf + 1 < buf_end) {
				buf = rz_uleb128(buf, buf_end - buf, &op->args.define_file.dir_index, NULL);
			}
			if (buf && buf + 1 < buf_end) {
				buf = rz_uleb128(buf, buf_end - buf, NULL, NULL);
			}
			if (buf && buf + 1 < buf_end) {
				buf = rz_uleb128(buf, buf_end - buf, NULL, NULL);
			}
		} else {
			op->type = RZ_BIN_DWARF_LINE_OP_TYPE_EXT_UNKNOWN;
		}
		break;
	}
	case DW_LNE_set_discriminator:
		buf = rz_uleb128(buf, buf_end - buf, &op->args.set_discriminator, NULL);
		break;
	case DW_LNE_end_sequence:
	default:
		buf += op_len - 1;
		break;
	}
	return buf;
}

/**
 * \return the number of leb128 args the std opcode takes, EXCEPT for DW_LNS_fixed_advance_pc! (see Dwarf spec)
 */
static size_t std_opcode_args_count(const RzBinDwarfLineHeader *hdr, ut8 opcode) {
	if (!opcode || opcode > hdr->opcode_base - 1 || !hdr->std_opcode_lengths) {
		return 0;
	}
	return hdr->std_opcode_lengths[opcode - 1];
}

static const ut8 *parse_std_opcode(RzBinDwarfLineOp *op, const RzBinDwarfLineHeader *hdr, const ut8 *obuf, size_t len, enum DW_LNS opcode, bool big_endian) {
	rz_return_val_if_fail(op && hdr && obuf, NULL);
	const ut8 *buf = obuf;
	const ut8 *buf_end = obuf + len;

	op->type = RZ_BIN_DWARF_LINE_OP_TYPE_STD;
	op->opcode = opcode;
	switch (opcode) {
	case DW_LNS_advance_pc:
		buf = rz_uleb128(buf, buf_end - buf, &op->args.advance_pc, NULL);
		break;
	case DW_LNS_advance_line:
		buf = rz_leb128(buf, buf_end - buf, &op->args.advance_line);
		break;
	case DW_LNS_set_file:
		buf = rz_uleb128(buf, buf_end - buf, &op->args.set_file, NULL);
		break;
	case DW_LNS_set_column:
		buf = rz_uleb128(buf, buf_end - buf, &op->args.set_column, NULL);
		break;
	case DW_LNS_fixed_advance_pc:
		op->args.fixed_advance_pc = READ16(buf);
		break;
	case DW_LNS_set_isa:
		buf = rz_uleb128(buf, buf_end - buf, &op->args.set_isa, NULL);
		break;

	// known opcodes that take no args
	case DW_LNS_copy:
	case DW_LNS_negate_stmt:
	case DW_LNS_set_basic_block:
	case DW_LNS_const_add_pc:
	case DW_LNS_set_prologue_end:
	case DW_LNS_set_epilogue_begin:
		break;

	// unknown operands, skip the number of args given in the header.
	default: {
		size_t args_count = std_opcode_args_count(hdr, opcode);
		for (size_t i = 0; i < args_count; i++) {
			buf = rz_uleb128(buf, buf_end - buf, &op->args.advance_pc, NULL);
			if (!buf) {
				break;
			}
		}
	}
	}
	return buf;
}

RZ_API void rz_bin_dwarf_line_header_reset_regs(const RzBinDwarfLineHeader *hdr, RzBinDwarfSMRegisters *regs) {
	rz_return_if_fail(hdr && regs);
	regs->address = 0;
	regs->file = 1;
	regs->line = 1;
	regs->column = 0;
	regs->is_stmt = hdr->default_is_stmt;
	regs->basic_block = DWARF_FALSE;
	regs->end_sequence = DWARF_FALSE;
	regs->prologue_end = DWARF_FALSE;
	regs->epilogue_begin = DWARF_FALSE;
	regs->isa = 0;
}

static void store_line_sample(RzBinSourceLineInfoBuilder *bob, const RzBinDwarfLineHeader *hdr, RzBinDwarfSMRegisters *regs,
	RZ_NULLABLE RzBinDwarfDebugInfo *info, RZ_NULLABLE RzBinDwarfLineFileCache *fnc) {
	const char *file = NULL;
	if (regs->file) {
		file = get_full_file_path(info, hdr, fnc, regs->file - 1);
	}
	rz_bin_source_line_info_builder_push_sample(bob, regs->address, (ut32)regs->line, (ut32)regs->column, file);
}

/**
 * \brief Execute a single line op on regs and optionally store the resulting line info in bob
 * \param fnc if not null, filenames will be resolved to their full paths using this cache.
 */
RZ_API bool rz_bin_dwarf_line_op_run(const RzBinDwarfLineHeader *hdr, RzBinDwarfSMRegisters *regs, RzBinDwarfLineOp *op,
	RZ_NULLABLE RzBinSourceLineInfoBuilder *bob, RZ_NULLABLE RzBinDwarfDebugInfo *info, RZ_NULLABLE RzBinDwarfLineFileCache *fnc) {
	rz_return_val_if_fail(hdr && regs && op, false);
	switch (op->type) {
	case RZ_BIN_DWARF_LINE_OP_TYPE_STD:
		switch (op->opcode) {
		case DW_LNS_copy:
			if (bob) {
				store_line_sample(bob, hdr, regs, info, fnc);
			}
			regs->basic_block = DWARF_FALSE;
			break;
		case DW_LNS_advance_pc:
			regs->address += op->args.advance_pc * hdr->min_inst_len;
			break;
		case DW_LNS_advance_line:
			regs->line += op->args.advance_line;
			break;
		case DW_LNS_set_file:
			regs->file = op->args.set_file;
			break;
		case DW_LNS_set_column:
			regs->column = op->args.set_column;
			break;
		case DW_LNS_negate_stmt:
			regs->is_stmt = regs->is_stmt ? DWARF_FALSE : DWARF_TRUE;
			break;
		case DW_LNS_set_basic_block:
			regs->basic_block = DWARF_TRUE;
			break;
		case DW_LNS_const_add_pc:
			regs->address += rz_bin_dwarf_line_header_get_spec_op_advance_pc(hdr, 255);
			break;
		case DW_LNS_fixed_advance_pc:
			regs->address += op->args.fixed_advance_pc;
			break;
		case DW_LNS_set_prologue_end:
			regs->prologue_end = ~0;
			break;
		case DW_LNS_set_epilogue_begin:
			regs->epilogue_begin = ~0;
			break;
		case DW_LNS_set_isa:
			regs->isa = op->args.set_isa;
			break;
		default:
			return false;
		}
		break;
	case RZ_BIN_DWARF_LINE_OP_TYPE_EXT:
		switch (op->ext_opcode) {
		case DW_LNE_end_sequence:
			regs->end_sequence = DWARF_TRUE;
			if (bob) {
				// closing entry
				rz_bin_source_line_info_builder_push_sample(bob, regs->address, 0, 0, NULL);
			}
			rz_bin_dwarf_line_header_reset_regs(hdr, regs);
			break;
		case DW_LNE_set_address:
			regs->address = op->args.set_address;
			break;
		case DW_LNE_define_file:
			break;
		case DW_LNE_set_discriminator:
			regs->discriminator = op->args.set_discriminator;
			break;
		default:
			return false;
		}
		break;
	case RZ_BIN_DWARF_LINE_OP_TYPE_SPEC:
		regs->address += rz_bin_dwarf_line_header_get_spec_op_advance_pc(hdr, op->opcode);
		regs->line += rz_bin_dwarf_line_header_get_spec_op_advance_line(hdr, op->opcode);
		if (bob) {
			store_line_sample(bob, hdr, regs, info, fnc);
		}
		regs->basic_block = DWARF_FALSE;
		regs->prologue_end = DWARF_FALSE;
		regs->epilogue_begin = DWARF_FALSE;
		regs->discriminator = 0;
		break;
	default:
		return false;
	}
	return true;
}

static size_t parse_opcodes(const ut8 *obuf,
	size_t len, const RzBinDwarfLineHeader *hdr, RzVector /*<RzBinDwarfLineOp>*/ *ops_out,
	RzBinDwarfSMRegisters *regs, RZ_NULLABLE RzBinSourceLineInfoBuilder *bob, RZ_NULLABLE RzBinDwarfDebugInfo *info,
	RZ_NULLABLE RzBinDwarfLineFileCache *fnc, bool big_endian) {
	const ut8 *buf, *buf_end;

	if (!obuf || !len) {
		return 0;
	}
	buf = obuf;
	buf_end = obuf + len;

	while (buf < buf_end) {
		ut8 opcode = *buf++;
		RzBinDwarfLineOp op = { 0 };
		if (!opcode) {
			buf = parse_ext_opcode(&op, hdr, buf, (buf_end - buf), big_endian);
		} else if (opcode >= hdr->opcode_base) {
			// special opcode without args, no further parsing needed
			op.type = RZ_BIN_DWARF_LINE_OP_TYPE_SPEC;
			op.opcode = opcode;
		} else {
			buf = parse_std_opcode(&op, hdr, buf, (buf_end - buf), opcode, big_endian);
		}
		if (!buf) {
			break;
		}
		if (bob) {
			rz_bin_dwarf_line_op_run(hdr, regs, &op, bob, info, fnc);
		}
		if (ops_out) {
			rz_vector_push(ops_out, &op);
		} else {
			rz_bin_dwarf_line_op_fini(&op);
		}
	}
	if (!buf) {
		return 0;
	}
	return (size_t)(buf - obuf); // number of bytes we've moved by
}

static void line_unit_free(RzBinDwarfLineUnit *unit) {
	if (!unit) {
		return;
	}
	line_header_fini(&unit->header);
	rz_vector_fini(&unit->ops);
	free(unit);
}

static RzBinDwarfLineInfo *parse_line_raw(RzBinFile *binfile, const ut8 *obuf,
	ut64 len, RzBinDwarfLineInfoMask mask, bool big_endian, RZ_NULLABLE RzBinDwarfDebugInfo *info) {
	// Dwarf 3 Standard 6.2 Line Number Information
	rz_return_val_if_fail(binfile && obuf, NULL);

	const ut8 *buf = obuf;
	const ut8 *buf_start = buf;
	const ut8 *buf_end = obuf + len;
	const ut8 *tmpbuf = NULL;
	ut64 buf_size;

	RzBinDwarfLineInfo *li = RZ_NEW0(RzBinDwarfLineInfo);
	if (!li) {
		return NULL;
	}
	li->units = rz_list_newf((RzListFree)line_unit_free);
	if (!li->units) {
		free(li);
		return NULL;
	}

	RzBinSourceLineInfoBuilder bob;
	if (mask & RZ_BIN_DWARF_LINE_INFO_MASK_LINES) {
		rz_bin_source_line_info_builder_init(&bob);
	}

	// each iteration we read one header AKA comp. unit
	while (buf <= buf_end) {
		RzBinDwarfLineUnit *unit = RZ_NEW0(RzBinDwarfLineUnit);
		if (!unit) {
			break;
		}

		// How much did we read from the compilation unit
		size_t bytes_read = 0;
		// calculate how much we've read by parsing header
		// because header unit_length includes itself
		buf_size = buf_end - buf;

		tmpbuf = buf;
		buf = parse_line_header(binfile, buf, buf_end, &unit->header, buf - buf_start, big_endian);
		if (!buf) {
			line_unit_free(unit);
			break;
		}

		bytes_read = buf - tmpbuf;

		RzBinDwarfSMRegisters regs;
		rz_bin_dwarf_line_header_reset_regs(&unit->header, &regs);

		// If there is more bytes in the buffer than size of the header
		// It means that there has to be another header/comp.unit
		buf_size = RZ_MIN(buf_size, unit->header.unit_length + (unit->header.is_64bit * 8 + 4)); // length field + rest of the unit
		if (buf_size <= bytes_read) {
			// no info or truncated
			line_unit_free(unit);
			continue;
		}
		if (buf_size > (buf_end - buf) + bytes_read || buf > buf_end) {
			line_unit_free(unit);
			break;
		}
		size_t tmp_read = 0;

		if (mask & RZ_BIN_DWARF_LINE_INFO_MASK_OPS) {
			rz_vector_init(&unit->ops, sizeof(RzBinDwarfLineOp), NULL, NULL);
		}

		RzBinDwarfLineFileCache *fnc = NULL;
		if (mask & RZ_BIN_DWARF_LINE_INFO_MASK_LINES) {
			fnc = rz_pvector_new_with_len(free, rz_vector_len(&unit->header.file_names));
		}

		// we read the whole compilation unit (that might be composed of more sequences)
		do {
			// reads one whole sequence
			tmp_read = parse_opcodes(buf, buf_size - bytes_read, &unit->header,
				(mask & RZ_BIN_DWARF_LINE_INFO_MASK_OPS) ? &unit->ops : NULL, &regs,
				(mask & RZ_BIN_DWARF_LINE_INFO_MASK_LINES) ? &bob : NULL,
				info, fnc, big_endian);
			bytes_read += tmp_read;
			buf += tmp_read; // Move in the buffer forward
		} while (bytes_read < buf_size && tmp_read != 0); // if nothing is read -> error, exit

		rz_pvector_free(fnc);

		if (!tmp_read) {
			line_unit_free(unit);
			break;
		}
		rz_list_push(li->units, unit);
	}
	if (mask & RZ_BIN_DWARF_LINE_INFO_MASK_LINES) {
		li->lines = rz_bin_source_line_info_builder_build_and_fini(&bob);
	}
	return li;
}

RZ_API void rz_bin_dwarf_arange_set_free(RzBinDwarfARangeSet *set) {
	if (!set) {
		return;
	}
	free(set->aranges);
	free(set);
}

static RzList /*<RzBinDwarfARangeSet *>*/ *parse_aranges_raw(const ut8 *obuf, size_t obuf_sz, bool big_endian) {
	rz_return_val_if_fail(obuf, NULL);
	const ut8 *buf = obuf;
	const ut8 *buf_end = buf + obuf_sz;

	RzList *r = rz_list_newf((RzListFree)rz_bin_dwarf_arange_set_free);
	if (!r) {
		return NULL;
	}

	// DWARF 3 Standard Section 6.1.2 Lookup by Address
	// also useful to grep for display_debug_aranges in binutils
	while (buf < buf_end) {
		const ut8 *start = buf;
		bool is_64bit;
		ut64 unit_length = dwarf_read_initial_length(&is_64bit, big_endian, &buf, buf_end);
		// Sanity check: length must be at least the minimal size of the remaining header fields
		// and at maximum the remaining buffer size.
		size_t header_rest_size = 2 + (is_64bit ? 8 : 4) + 1 + 1;
		if (unit_length < header_rest_size || unit_length > buf_end - buf) {
			break;
		}
		const ut8 *next_set_buf = buf + unit_length;
		RzBinDwarfARangeSet *set = RZ_NEW(RzBinDwarfARangeSet);
		if (!set) {
			break;
		}
		set->unit_length = unit_length;
		set->is_64bit = is_64bit;
		set->version = READ16(buf);
		set->debug_info_offset = dwarf_read_offset(set->is_64bit, big_endian, &buf, buf_end);
		set->address_size = READ8(buf);
		set->segment_size = READ8(buf);
		unit_length -= header_rest_size;
		if (!set->address_size) {
			free(set);
			break;
		}

		// align to 2*addr_size
		size_t off = buf - start;
		size_t pad = rz_num_align_delta(off, 2 * set->address_size);
		if (pad > unit_length || pad > buf_end - buf) {
			free(set);
			break;
		}
		buf += pad;
		unit_length -= pad;

		size_t arange_size = 2 * set->address_size;
		set->aranges_count = unit_length / arange_size;
		if (!set->aranges_count) {
			free(set);
			break;
		}
		set->aranges = RZ_NEWS0(RzBinDwarfARange, set->aranges_count);
		if (!set->aranges) {
			free(set);
			break;
		}
		size_t i;
		for (i = 0; i < set->aranges_count; i++) {
			set->aranges[i].addr = dwarf_read_address(set->address_size, big_endian, &buf, buf_end);
			set->aranges[i].length = dwarf_read_address(set->address_size, big_endian, &buf, buf_end);
			if (!set->aranges[i].addr && !set->aranges[i].length) {
				// last entry has two 0s
				i++; // so i will be the total count of read entries
				break;
			}
		}
		set->aranges_count = i;
		buf = next_set_buf;
		rz_list_push(r, set);
	}

	return r;
}

static void ht_kv_value_free(HtUPKv *kv) {
	free(kv->value);
}

static void attr_fini(RzBinDwarfAttr *val) {
	// TODO adjust to new forms, now we're leaking
	if (!val) {
		return;
	}
	switch (val->form) {
	case DW_FORM_strp:
	case DW_FORM_string:
		RZ_FREE(val->string.content);
		break;
	case DW_FORM_exprloc:
	case DW_FORM_block:
	case DW_FORM_block1:
	case DW_FORM_block2:
	case DW_FORM_block4:
		RZ_FREE(val->block.data);
		break;
	default:
		break;
	};
}

static void die_init(RzBinDwarfDie *die) {
	if (!die) {
		return;
	}
	rz_vector_init(&die->attrs, sizeof(RzBinDwarfAttr), (RzVectorFree)attr_fini, NULL);
}

static void die_fini(RzBinDwarfDie *die) {
	if (!die) {
		return;
	}
	rz_vector_fini(&die->attrs);
}

static int unit_init(RzBinDwarfCompUnit *unit) {
	if (!unit) {
		return -EINVAL;
	}
	rz_vector_init(&unit->dies, sizeof(RzBinDwarfDie), (RzVectorFree)die_fini, NULL);
	return 0;
}

static void unit_fini(RzBinDwarfCompUnit *unit, void *user) {
	if (!unit) {
		return;
	}
	rz_vector_fini(&unit->dies);
}

static bool debug_info_init(RzBinDwarfDebugInfo *info) {
	rz_vector_init(&info->units, sizeof(RzBinDwarfCompUnit), (RzVectorFree)unit_fini, NULL);
	info->line_info_offset_comp_dir = ht_up_new(NULL, ht_kv_value_free, NULL);
	if (!info->line_info_offset_comp_dir) {
		goto beach;
	}
	return true;
beach:
	rz_vector_fini(&info->units);
	return false;
}

static int abbrev_decl_init(RzBinDwarfAbbrevDecl *abbrev) {
	if (!abbrev) {
		return -EINVAL;
	}
	rz_vector_init(&abbrev->defs, sizeof(RzBinDwarfAttrDef), NULL, NULL);
	return 0;
}

static void abbrev_decl_fini(RzBinDwarfAbbrevDecl *abbrevs, void *user) {
	if (!abbrevs) {
		return;
	}
	rz_vector_fini(&abbrevs->defs);
}

static void abbrev_fini(RzBinDwarfDebugAbbrevs *abbrevs) {
	if (!abbrevs) {
		return;
	}
	rz_vector_fini(&abbrevs->decls);
	ht_up_free(abbrevs->decl_tbl);
	ht_uu_free(abbrevs->decl_index_tbl);
}

static int abbrev_init(RzBinDwarfDebugAbbrevs *abbrevs) {
	if (!abbrevs) {
		return -EINVAL;
	}
	rz_vector_init(&abbrevs->decls, sizeof(RzBinDwarfAbbrevDecl), (RzVectorFree)abbrev_decl_fini, NULL);
	abbrevs->decl_tbl = ht_up_new(NULL, NULL, NULL);
	if (!abbrevs->decl_tbl) {
		goto beach;
	}
	abbrevs->decl_index_tbl = ht_uu_new0();
	if (!abbrevs->decl_index_tbl) {
		goto beach;
	}
	return 0;
beach:
	abbrev_fini(abbrevs);
	return -EINVAL;
}

RZ_API void rz_bin_dwarf_abbrev_free(RzBinDwarfDebugAbbrevs *abbrevs) {
	if (!abbrevs) {
		return;
	}
	abbrev_fini(abbrevs);
	free(abbrevs);
}

RZ_API void rz_bin_dwarf_line_info_free(RzBinDwarfLineInfo *li) {
	if (!li) {
		return;
	}
	rz_list_free(li->units);
	rz_bin_source_line_info_free(li->lines);
	free(li);
}

RZ_API void rz_bin_dwarf_info_free(RzBinDwarfDebugInfo *info) {
	if (!info) {
		return;
	}
	rz_vector_fini(&info->units);
	ht_up_free(info->line_info_offset_comp_dir);
	ht_up_free(info->die_tbl);
	ht_up_free(info->unit_tbl);
	free(info);
}

static const ut8 *fill_block_data(const ut8 *buf, const ut8 *buf_end, RzBinDwarfBlock *block) {
	block->data = calloc(sizeof(ut8), block->length);
	if (!block->data) {
		return NULL;
	}
	/* Maybe unroll this as an optimization in future? */
	if (block->data) {
		size_t j = 0;
		for (j = 0; j < block->length; j++) {
			block->data[j] = READ8(buf);
		}
	}
	return buf;
}

static bool buf_read_block(RzBuffer *buffer, RzBinDwarfBlock *block) {
	if (block->length == 0) {
		return true;
	}
	block->data = calloc(sizeof(ut8), block->length);
	RET_FALSE_IF_FAIL(block->data);
	/* Maybe unroll this as an optimization in future? */
	ut16 len = rz_buf_read(buffer, block->data, block->length);
	if (len != block->length) {
		RZ_FREE(block->data);
		return false;
	}
	return true;
}

/**
 * This function is quite incomplete and requires lot of work
 * With parsing various new FORM values
 * \brief Parses attribute value based on its definition
 *        and stores it into `value`
 */
static bool attr_parse(RzBuffer *buffer, RzBinDwarfAttrDef *def, RzBinDwarfAttr *value, const RzBinDwarfCompUnitHdr *hdr, RzBuffer *str_buffer, bool big_endian) {
	rz_return_val_if_fail(def && value && hdr && buffer, NULL);

	value->form = def->form;
	value->name = def->name;
	value->block.data = NULL;
	value->string.content = NULL;
	value->string.offset = 0;

	// http://www.dwarfstd.org/doc/DWARF4.pdf#page=161&zoom=100,0,560
	switch (def->form) {
	case DW_FORM_addr:
		value->kind = DW_AT_KIND_ADDRESS;
		UX_OR_RET_FALSE(value->address, hdr->address_size);
		break;
	case DW_FORM_data1:
		value->kind = DW_AT_KIND_CONSTANT;
		U8_OR_RET_FALSE(value->uconstant);
		break;
	case DW_FORM_data2:
		value->kind = DW_AT_KIND_CONSTANT;
		U16_OR_RET_FALSE(value->uconstant);
		break;
	case DW_FORM_data4:
		value->kind = DW_AT_KIND_CONSTANT;
		U32_OR_RET_FALSE(value->uconstant);
		break;
	case DW_FORM_data8:
		value->kind = DW_AT_KIND_CONSTANT;
		U64_OR_RET_FALSE(value->uconstant);
		break;
	case DW_FORM_data16: // TODO Fix this, right now I just read the data, but I need to make storage for it
		value->kind = DW_AT_KIND_CONSTANT;
		if (big_endian) {
			U64_OR_RET_FALSE(value->uconstant128.High);
			U64_OR_RET_FALSE(value->uconstant128.Low);
		} else {
			U64_OR_RET_FALSE(value->uconstant128.Low);
			U64_OR_RET_FALSE(value->uconstant128.High);
		}
		break;
	case DW_FORM_sdata:
		value->kind = DW_AT_KIND_CONSTANT;
		SLE128_OR_RET_FALSE(value->sconstant);
		break;
	case DW_FORM_udata:
		value->kind = DW_AT_KIND_CONSTANT;
		ULE128_OR_RET_FALSE(value->uconstant);
		break;
	case DW_FORM_string:
		value->kind = DW_AT_KIND_STRING;
		value->string.content = buf_get_string(buffer);
#define CHECK_STRING \
	if (!value->string.content) { \
		RZ_LOG_ERROR("Failed to read string %s [%s]\n", rz_bin_dwarf_attr(def->name), rz_bin_dwarf_form(def->form)); \
		return false; \
	}
		CHECK_STRING;
		break;
	case DW_FORM_block1:
		value->kind = DW_AT_KIND_BLOCK;
		U8_OR_RET_FALSE(value->block.length);
		RET_FALSE_IF_FAIL(buf_read_block(buffer, &value->block));
		break;
	case DW_FORM_block2:
		value->kind = DW_AT_KIND_BLOCK;
		U16_OR_RET_FALSE(value->block.length);
		RET_FALSE_IF_FAIL(buf_read_block(buffer, &value->block));
		break;
	case DW_FORM_block4:
		value->kind = DW_AT_KIND_BLOCK;
		U32_OR_RET_FALSE(value->block.length);
		RET_FALSE_IF_FAIL(buf_read_block(buffer, &value->block));
		break;
	case DW_FORM_block: // variable length ULEB128
		value->kind = DW_AT_KIND_BLOCK;
		ULE128_OR_RET_NULL(value->block.length);
		RET_FALSE_IF_FAIL(buf_read_block(buffer, &value->block));
		break;
	case DW_FORM_flag:
		value->kind = DW_AT_KIND_FLAG;
		U8_OR_RET_FALSE(value->flag);
		break;
		// offset in .debug_str
	case DW_FORM_strp:
		value->kind = DW_AT_KIND_STRING;
		RET_FALSE_IF_FAIL(read_offset(buffer, &value->string.offset, hdr->is_64bit, big_endian));
		if (str_buffer && value->string.offset < rz_buf_size(str_buffer)) {
			value->string.content = rz_buf_get_string(str_buffer, value->string.offset);
		}
		CHECK_STRING;
		break;
		// offset in .debug_info
	case DW_FORM_ref_addr:
		value->kind = DW_AT_KIND_REFERENCE;
		RET_FALSE_IF_FAIL(read_offset(buffer, &value->reference, hdr->is_64bit, big_endian));
		break;
		// This type of reference is an offset from the first byte of the compilation
		// header for the compilation unit containing the reference
	case DW_FORM_ref1:
		value->kind = DW_AT_KIND_REFERENCE;
		U8_OR_RET_FALSE(value->reference);
		value->reference += hdr->unit_offset;
		break;
	case DW_FORM_ref2:
		value->kind = DW_AT_KIND_REFERENCE;
		U16_OR_RET_FALSE(value->reference);
		value->reference += hdr->unit_offset;
		break;
	case DW_FORM_ref4:
		value->kind = DW_AT_KIND_REFERENCE;
		U32_OR_RET_FALSE(value->reference);
		value->reference += hdr->unit_offset;
		break;
	case DW_FORM_ref8:
		value->kind = DW_AT_KIND_REFERENCE;
		U64_OR_RET_FALSE(value->reference);
		value->reference += hdr->unit_offset;
		break;
	case DW_FORM_ref_udata:
		value->kind = DW_AT_KIND_REFERENCE;
		// uleb128 is enough to fit into ut64?
		ULE128_OR_RET_FALSE(value->reference);
		value->reference += hdr->unit_offset;
		break;
		// offset in a section other than .debug_info or .debug_str
	case DW_FORM_sec_offset:
		value->kind = DW_AT_KIND_REFERENCE;
		RET_FALSE_IF_FAIL(read_offset(buffer, &value->reference, hdr->is_64bit, big_endian));
		break;
	case DW_FORM_exprloc:
		value->kind = DW_AT_KIND_BLOCK;
		ULE128_OR_RET_FALSE(value->block.length);
		RET_FALSE_IF_FAIL(buf_read_block(buffer, &value->block));
		break;
		// this means that the flag is present, nothing is read
	case DW_FORM_flag_present:
		value->kind = DW_AT_KIND_FLAG;
		value->flag = true;
		break;
	case DW_FORM_ref_sig8:
		value->kind = DW_AT_KIND_REFERENCE;
		U64_OR_RET_NULL(value->reference);
		break;
		// offset into .debug_line_str section, can't parse the section now, so we just skip
	case DW_FORM_strx:
		value->kind = DW_AT_KIND_STRING;
		// value->string.offset = dwarf_read_offset (hdr->is_64bit, big_endian, &buf, buf_end);
		// if (debug_str && value->string.offset < debug_str_len) {
		// 	value->string.content =
		// 		rz_str_ndup ((const char *)(debug_str + value->string.offset), debug_str_len - value->string.offset);
		// } else {
		// 	value->string.content = NULL; // Means malformed DWARF, should we print error message?
		// }
		break;
	case DW_FORM_strx1:
		value->kind = DW_AT_KIND_STRING;
		U8_OR_RET_FALSE(value->string.offset);
		break;
	case DW_FORM_strx2:
		value->kind = DW_AT_KIND_STRING;
		U16_OR_RET_FALSE(value->string.offset);
		break;
	case DW_FORM_strx3: // TODO Add 3 byte int read
		value->kind = DW_AT_KIND_STRING;
		rz_buf_seek(buffer, 3, RZ_BUF_CUR);
		break;
	case DW_FORM_strx4:
		value->kind = DW_AT_KIND_STRING;
		U32_OR_RET_FALSE(value->string.offset);
		break;
	case DW_FORM_implicit_const:
		value->kind = DW_AT_KIND_CONSTANT;
		value->uconstant = def->special;
		break;
		/*  addrx* forms : The index is relative to the value of the
			DW_AT_addr_base attribute of the associated compilation unit.
		    index into an array of addresses in the .debug_addr section.*/
	case DW_FORM_addrx:
		value->kind = DW_AT_KIND_ADDRESS;
		ULE128_OR_RET_FALSE(value->address);
		break;
	case DW_FORM_addrx1:
		value->kind = DW_AT_KIND_ADDRESS;
		U8_OR_RET_FALSE(value->address);
		break;
	case DW_FORM_addrx2:
		value->kind = DW_AT_KIND_ADDRESS;
		U16_OR_RET_FALSE(value->address);
		break;
	case DW_FORM_addrx3:
		// I need to add 3byte endianess free read here TODO
		value->kind = DW_AT_KIND_ADDRESS;
		rz_buf_seek(buffer, 3, RZ_BUF_CUR);
		break;
	case DW_FORM_addrx4:
		value->kind = DW_AT_KIND_ADDRESS;
		U32_OR_RET_FALSE(value->address);
		break;
	case DW_FORM_line_ptr: // offset in a section .debug_line_str
	case DW_FORM_strp_sup: // offset in a section .debug_line_str
		value->kind = DW_AT_KIND_STRING;
		RET_FALSE_IF_FAIL(read_offset(buffer, &value->string.offset, hdr->is_64bit, big_endian));
		// if (debug_str && value->string.offset < debug_line_str_len) {
		// 	value->string.content =
		// 		rz_str_ndup
		break;
		// offset in the supplementary object file
	case DW_FORM_ref_sup4:
		value->kind = DW_AT_KIND_REFERENCE;
		U32_OR_RET_FALSE(value->reference);
		break;
	case DW_FORM_ref_sup8:
		value->kind = DW_AT_KIND_REFERENCE;
		U64_OR_RET_FALSE(value->reference);
		break;
		// An index into the .debug_loc
	case DW_FORM_loclistx:
		value->kind = DW_AT_KIND_LOCLISTPTR;
		RET_FALSE_IF_FAIL(read_offset(buffer, &value->reference, hdr->is_64bit, big_endian));
		break;
		// An index into the .debug_rnglists
	case DW_FORM_rnglistx:
		value->kind = DW_AT_KIND_ADDRESS;
		ULE128_OR_RET_FALSE(value->address);
		break;
	default:
		RZ_LOG_ERROR("Unknown DW_FORM 0x%02" PFMT32x "\n", def->form);
		value->uconstant = 0;
		return false;
	}
	return true;
}

static char *attr_to_string(RzBinDwarfAttr *attr) {
	switch (attr->name) {
	case DW_AT_language: return rz_str_new(rz_bin_dwarf_lang(attr->uconstant));
	default: break;
	}
	switch (attr->kind) {
	case DW_AT_KIND_ADDRESS: return rz_str_newf("0x%" PFMT64x, attr->address);
	case DW_AT_KIND_BLOCK: return rz_str_newf("0x%" PFMT64x, attr->block.length);
	case DW_AT_KIND_CONSTANT:
		return rz_str_newf("0x%" PFMT64x, attr->uconstant);
	case DW_AT_KIND_FLAG: return rz_str_newf("true");
	case DW_AT_KIND_REFERENCE:
	case DW_AT_KIND_LOCLISTPTR: return rz_str_newf("ref: 0x%" PFMT64x, attr->reference);
	case DW_AT_KIND_STRING: return attr->string.offset > 0 ? rz_str_newf(".debug_str[0x%" PFMT64x "] = \"%s\"", attr->string.offset, attr->string.content) : rz_str_newf("\"%s\"", attr->string.content);
	case DW_AT_KIND_RANGELISTPTR:
	case DW_AT_KIND_MACPTR:
	case DW_AT_KIND_LINEPTR:
	case DW_AT_KIND_EXPRLOC:
	default: return NULL;
	}
}

static bool die_attr_parse(RzBuffer *buffer, RzBinDwarfDie *die, RzBinDwarfDebugInfo *info, RzBinDwarfAbbrevDecl *abbrev,
	RzBinDwarfCompUnitHdr *hdr, RzBuffer *str_buffer, bool big_endian) {
	const char *comp_dir = NULL;
	ut64 line_info_offset = UT64_MAX;

	RZ_LOG_DEBUG("0x%" PFMT64x ":\t%s [%" PFMT64d "] %s\n", die->offset, rz_bin_dwarf_tag(die->tag), die->abbrev_code, rz_bin_dwarf_children(die->has_children));
	void *it;
	rz_vector_foreach(&abbrev->defs, it) {
		RzBinDwarfAttrDef *def = it;
		RzBinDwarfAttr attr = { 0 };
		if (!attr_parse(buffer, def, &attr, hdr, str_buffer, big_endian)) {
			RZ_LOG_ERROR("0x%" PFMT64x ":\tfailed 0x%" PFMT64x "%s [%s]\n ", rz_buf_tell(buffer), die->offset, rz_bin_dwarf_attr(def->name), rz_bin_dwarf_form(def->form));
			continue;
		}

		char *data = attr_to_string(&attr);
		RZ_LOG_DEBUG("0x%" PFMT64x ":\t\t%s [%s]\t(%s)\n", rz_buf_tell(buffer), rz_bin_dwarf_attr(def->name), rz_bin_dwarf_form(def->form), rz_str_get(data));
		free(data);

		enum DW_AT name = attr.name;
		enum DW_FORM form = attr.form;
		RzBinDwarfAttrKind kind = attr.kind;
		if (name == DW_AT_comp_dir && (form == DW_FORM_strp || form == DW_FORM_string) && attr.string.content) {
			comp_dir = attr.string.content;
		}
		if (name == DW_AT_stmt_list) {
			if (kind == DW_AT_KIND_CONSTANT) {
				line_info_offset = attr.uconstant;
			} else if (kind == DW_AT_KIND_REFERENCE) {
				line_info_offset = attr.reference;
			}
		}
		rz_vector_push(&die->attrs, &attr);
	}

	// If this is a compilation unit dir attribute, we want to cache it so the line info parsing
	// which will need this info can quickly look it up.
	if (comp_dir && line_info_offset != UT64_MAX) {
		char *name = strdup(comp_dir);
		if (name) {
			if (!ht_up_insert(info->line_info_offset_comp_dir, line_info_offset, name)) {
				free(name);
			}
		}
	}
	return true;
}

static RzBinDwarfAbbrevDecl *abbrev_get(const RzBinDwarfDebugAbbrevs *abbrevs, ut64 idx) {
	idx -= 1;
	if (idx >= rz_bin_dwarf_abbrev_count(abbrevs)) {
		return NULL;
	}
	return rz_bin_dwarf_abbrev_get(abbrevs, idx);
}

/**
 * \brief Reads throught comp_unit buffer and parses all its DIEntries*
 */
static bool comp_unit_die_parse(RzBuffer *buffer, RzBinDwarfCompUnit *unit, RzBinDwarfDebugInfo *info, const RzBinDwarfDebugAbbrevs *abbrevs,
	size_t first_abbr_idx, RzBuffer *str_buffer, bool big_endian) {

	size_t index = 0;
	st64 depth = 0;
	while (true) {
		ut64 offset = rz_buf_tell(buffer);
		RzBinDwarfDie die = {
			.offset = offset,
			.unit_offset = unit->offset,
			.index = index,
		};
		die_init(&die);
		// DIE starts with ULEB128 with the abbreviation code
		// we wanna store this entry too, usually the last one is null_entry
		// return the buffer to parse next compilation units
		ULE128_OR_GOTO(die.abbrev_code, ok);

		// there can be "null" entries that have abbr_code == 0
		if (!die.abbrev_code) {
			RZ_LOG_DEBUG("0x%" PFMT64x ":\tNULL\n", offset);
			rz_vector_push(&unit->dies, &die);
			depth--;
			if (depth <= 0) {
				break;
			} else {
				continue;
			}
		}

		RzBinDwarfAbbrevDecl *abbrev = abbrev_get(abbrevs, first_abbr_idx + die.abbrev_code);
		if (!abbrev) {
			break;
		}

		ut64 attr_count = rz_bin_dwarf_abbrev_decl_count(abbrev);
		if (attr_count) {
			rz_vector_reserve(&die.attrs, attr_count);
		}
		if (abbrev->code != 0) {
			die.tag = abbrev->tag;
			die.has_children = abbrev->has_children;
			if (die.has_children) {
				depth++;
			}
			GOTO_IF_FAIL(die_attr_parse(buffer, &die, info, abbrev, &unit->hdr, str_buffer, big_endian), err);
		}
		index++;
		rz_vector_push(&unit->dies, &die);
	}
ok:
	return true;
err:
	return false;
}

/**
 * \brief Reads all information about compilation unit header
 */
static bool comp_unit_hdr_parse(RzBuffer *buffer, RzBinDwarfCompUnitHdr *hdr, bool big_endian) {
	RET_FALSE_IF_FAIL(read_initial_length(buffer, &hdr->is_64bit, &hdr->length, big_endian));
	assert(hdr->length <= rz_buf_size(buffer) - rz_buf_tell(buffer));
	ut64 offset_start = rz_buf_tell(buffer);
	U16_OR_RET_FALSE(hdr->version);

	if (hdr->version == 5) {
		U8_OR_RET_FALSE(hdr->unit_type);
		U8_OR_RET_FALSE(hdr->address_size);
		RET_FALSE_IF_FAIL(read_offset(buffer, &hdr->abbrev_offset, hdr->is_64bit, big_endian));

		if (hdr->unit_type == DW_UT_skeleton || hdr->unit_type == DW_UT_split_compile) {
			U8_OR_RET_FALSE(hdr->dwo_id);
		} else if (hdr->unit_type == DW_UT_type || hdr->unit_type == DW_UT_split_type) {
			U64_OR_RET_FALSE(hdr->type_sig);
			RET_FALSE_IF_FAIL(read_offset(buffer, &hdr->type_offset, hdr->is_64bit, big_endian));
		}
	} else {
		RET_FALSE_IF_FAIL(read_offset(buffer, &hdr->abbrev_offset, hdr->is_64bit, big_endian));
		U8_OR_RET_FALSE(hdr->address_size);
	}
	hdr->header_size = rz_buf_tell(buffer) - offset_start; // header size excluding length field
	return true;
}

/**
 * \brief Parses whole .debug_info section
 */
static bool comp_unit_parse(RzBuffer *buffer, RzBinDwarfDebugInfo *info, RzBinDwarfDebugAbbrevs *da, RzBuffer *str_buffer, bool big_endian) {
	if (!info) {
		return NULL;
	}
	if (!debug_info_init(info)) {
		goto cleanup;
	}

	ut64 next_unit_offset = 0;
	while (true) {
		ut64 offset = rz_buf_tell(buffer);
		if (offset != next_unit_offset) {
			RZ_LOG_ERROR("offset: 0x%" PFMT64x " != next_unit_offset: 0x%" PFMT64x "\n", offset, next_unit_offset);
			rz_buf_seek(buffer, (st64)next_unit_offset, SEEK_SET);
		}

		RzBinDwarfCompUnit unit = {
			.offset = offset,
			.hdr.unit_offset = unit.offset,
		};
		if (unit_init(&unit) < 0) {
			goto cleanup;
		}
		if (!comp_unit_hdr_parse(buffer, &unit.hdr, big_endian)) {
			break;
		}
		if (unit.hdr.length > rz_buf_size(buffer)) {
			goto cleanup;
		}

		next_unit_offset = offset + unit.hdr.length;

		RzBinDwarfAbbrevDecl *decl_head = rz_vector_head(&da->decls);
		size_t count = rz_bin_dwarf_abbrev_count(da);
		if (rz_bin_dwarf_abbrev_decl_count(decl_head) >= count) {
			RZ_LOG_WARN("malformed dwarf have not enough buckets for decls.\n");
		}

		// find abbrev start for current comp unit
		// we could also do naive, ((char *)da->decls) + abbrev_offset,
		// but this is more bulletproof to invalid DWARF
		RzBinDwarfAbbrevDecl *abbrev_start = rz_bin_dwarf_abbrev_by_offet(da, unit.hdr.abbrev_offset);
		if (!abbrev_start) {
			goto cleanup;
		}

		// They point to the same array object, so should be def. behaviour
		size_t first_abbr_idx = rz_bin_dwarf_abbrev_index_by_offet(da, unit.hdr.abbrev_offset);

		RZ_LOG_DEBUG("0x%" PFMT64x ":\tcompile unit length = 0x%" PFMT64x ", abbr_offset: 0x%" PFMT64x "\n", unit.offset, unit.hdr.length, unit.hdr.abbrev_offset);
		comp_unit_die_parse(buffer, &unit, info, da, first_abbr_idx, str_buffer, big_endian);

		info->die_count += rz_vector_len(&unit.dies);
		rz_vector_push(&info->units, &unit);
	}

	return true;
cleanup:
	return false;
}

static bool abbrev_parse(RzBuffer *buffer, RzBinDwarfDebugAbbrevs *abbrevs) {
	abbrev_init(abbrevs);
	RzBinDwarfAbbrevDecl *p = NULL;
	while (true) {
		ut64 offset = rz_buf_tell(buffer);
		ut64 code = 0;
		ULE128_OR_GOTO(code, ok);
		if (code == 0) {
			continue;
		}

		enum DW_TAG tag;
		ULE128_OR_RET_FALSE(tag);
		enum DW_CHILDREN has_children;
		U8_OR_RET_FALSE(has_children);
		if (!(has_children == DW_CHILDREN_yes || has_children == DW_CHILDREN_no)) {
			RZ_LOG_ERROR("invalid DW_CHILDREN value: %d\n", has_children);
			return false;
		}

		RzBinDwarfAbbrevDecl decl = {
			.offset = offset,
			.code = code,
			.tag = tag,
			.has_children = has_children,
		};
		abbrev_decl_init(&decl);
		RZ_LOG_DEBUG("0x%" PFMT64x ":\t[%" PFMT64d "] %s, has_children: %d\n", offset, code, rz_bin_dwarf_tag(tag), has_children);

		do {
			ut64 name = 0;
			ULE128_OR_RET_FALSE(name);
			if (name == 0) {
				st64 form = 0;
				ULE128_OR_RET_FALSE(form);
				if (form == 0) {
					goto abbrev_ok;
				}
				goto err;
			}

			ut64 form = 0;
			ULE128_OR_RET_FALSE(form);

			/**
			 * http://www.dwarfstd.org/doc/DWARF5.pdf#page=225
			 *
			 * The attribute form DW_FORM_implicit_const is another special case. For
			 * attributes with this form, the attribute specification contains a third part, which is
			 * a signed LEB128 number. The value of this number is used as the value of the
			 * attribute, and no value is stored in the .debug_info section.
			 */
			st64 special = 0;
			if (form == DW_FORM_implicit_const) {
				SLE128_OR_RET_FALSE(special);
			}
			RzBinDwarfAttrDef def = {
				.name = name,
				.form = form,
				.special = special,
			};
			RZ_LOG_DEBUG("0x%" PFMT64x ":\t\t%s [%s] special = %" PFMT64d "\n", rz_buf_tell(buffer), rz_bin_dwarf_attr(name), rz_bin_dwarf_form(form), special);
			rz_vector_push(&decl.defs, &def);
		} while (true);
	abbrev_ok:
		p = rz_vector_push(&abbrevs->decls, &decl);
		ht_up_insert(abbrevs->decl_tbl, decl.offset, p);
		ht_uu_insert(abbrevs->decl_index_tbl, decl.offset, rz_vector_len(&abbrevs->decls) - 1);
	}
ok:
	return abbrevs;
err:
	rz_bin_dwarf_abbrev_free(abbrevs);
	return NULL;
}

static RzBinSection *getsection(RzBinFile *binfile, const char *sn) {
	rz_return_val_if_fail(binfile && sn, NULL);
	RzListIter *iter;
	RzBinSection *section = NULL;
	RzBinObject *o = binfile->o;
	if (!o || !o->sections) {
		return NULL;
	}
	rz_list_foreach (o->sections, iter, section) {
		if (!section->name) {
			continue;
		}
		if (strstr(section->name, sn)) {
			return section;
		}
	}
	return NULL;
}

static ut8 *get_section_bytes(RzBinFile *binfile, const char *sect_name, size_t *len) {
	rz_return_val_if_fail(binfile && sect_name && len, NULL);
	RzBinSection *section = getsection(binfile, sect_name);
	if (!section) {
		return NULL;
	}
	if (section->paddr >= binfile->size) {
		return NULL;
	}
	*len = RZ_MIN(section->size, binfile->size - section->paddr);
	ut8 *buf = calloc(1, *len);
	rz_buf_read_at(binfile->buf, section->paddr, buf, *len);
	return buf;
}

static RzBuffer *get_section_buf(RzBinFile *binfile, const char *sect_name) {
	rz_return_val_if_fail(binfile && sect_name, NULL);
	RzBinSection *section = getsection(binfile, sect_name);
	if (!section) {
		return NULL;
	}
	if (section->paddr >= binfile->size) {
		return NULL;
	}
	ut64 len = RZ_MIN(section->size, binfile->size - section->paddr);
	return rz_buf_new_slice(binfile->buf, section->paddr, len);
}

/**
 * @brief Parses .debug_info section
 *
 * @param abbrevs Parsed abbreviations
 * @param bin
 * @return RzBinDwarfDebugInfo* Parsed information, NULL if error
 */
RZ_API RzBinDwarfDebugInfo *rz_bin_dwarf_info_parse(RzBinFile *binfile, RzBinDwarfDebugAbbrevs *abbrevs) {
	rz_return_val_if_fail(binfile && abbrevs, NULL);
	RzBuffer *debug_str_buf = get_section_buf(binfile, "debug_str");
	RzBuffer *buf = get_section_buf(binfile, "debug_info");
	GOTO_IF_FAIL(buf, cave_debug_str_buf);

	RzBinDwarfDebugInfo *info = RZ_NEW0(RzBinDwarfDebugInfo);
	GOTO_IF_FAIL(comp_unit_parse(buf, info, abbrevs, debug_str_buf, binfile->o && binfile->o->info && binfile->o->info->big_endian), cave_buf);

	info->die_tbl = ht_up_new_size(info->die_count, NULL, NULL, NULL);
	GOTO_IF_FAIL(info->die_tbl, cave_info);
	info->unit_tbl = ht_up_new(NULL, NULL, NULL);
	GOTO_IF_FAIL(info->unit_tbl, cave_info);

	// build hashtable after whole parsing because of possible relocations
	void *it;
	rz_vector_foreach(&info->units, it) {
		RzBinDwarfCompUnit *unit = it;
		ht_up_insert(info->unit_tbl, unit->offset, unit);
		void *it_die;
		rz_vector_foreach(&unit->dies, it_die) {
			RzBinDwarfDie *die = it_die;
			ht_up_insert(info->die_tbl, die->offset, die); // optimization for further processing
		}
	}

cave_buf:
	free(buf);
cave_debug_str_buf:
	free(debug_str_buf);
	return info;
cave_info:
	rz_bin_dwarf_info_free(info);
	info = NULL;
	goto cave_buf;
}

/**
 * \param info if not NULL, filenames can get resolved to absolute paths using the compilation unit dirs from it
 */
RZ_API RzBinDwarfLineInfo *rz_bin_dwarf_parse_line(RzBinFile *binfile, RZ_NULLABLE RzBinDwarfDebugInfo *info, RzBinDwarfLineInfoMask mask) {
	rz_return_val_if_fail(binfile, NULL);
	size_t len;
	ut8 *buf = get_section_bytes(binfile, "debug_line", &len);
	if (!buf) {
		return NULL;
	}
	// Actually parse the section
	RzBinDwarfLineInfo *r = parse_line_raw(binfile, buf, len, mask, binfile->o && binfile->o->info && binfile->o->info->big_endian, info);
	free(buf);
	return r;
}

RZ_API RzList /*<RzBinDwarfARangeSet *>*/ *rz_bin_dwarf_aranges_parse(RzBinFile *binfile) {
	rz_return_val_if_fail(binfile, NULL);
	size_t len;
	ut8 *buf = get_section_bytes(binfile, "debug_aranges", &len);
	if (!buf) {
		return NULL;
	}
	RzList *r = parse_aranges_raw(buf, len, binfile->o && binfile->o->info && binfile->o->info->big_endian);
	free(buf);
	return r;
}

RZ_API RzBinDwarfDebugAbbrevs *rz_bin_dwarf_abbrev_parse(RzBinFile *binfile) {
	rz_return_val_if_fail(binfile, NULL);
	RzBinDwarfDebugAbbrevs *abbrevs = NULL;
	RzBuffer *buf = get_section_buf(binfile, "debug_abbrev");
	GOTO_IF_FAIL(buf, ok);
	abbrevs = RZ_NEW0(RzBinDwarfDebugAbbrevs);
	GOTO_IF_FAIL(abbrevs, err);
	GOTO_IF_FAIL(abbrev_parse(buf, abbrevs), err);
ok:
	rz_buf_free(buf);
	return abbrevs;
err:
	rz_bin_dwarf_abbrev_free(abbrevs);
	abbrevs = NULL;
	goto ok;
}

static inline ut64 get_max_offset(size_t addr_size) {
	switch (addr_size) {
	case 2:
		return UT16_MAX;
	case 4:
		return UT32_MAX;
	case 8:
		return UT64_MAX;
	default:
		rz_warn_if_reached();
		break;
	}
	return 0;
}

static inline RzBinDwarfLocList *create_loc_list(ut64 offset) {
	RzBinDwarfLocList *list = RZ_NEW0(RzBinDwarfLocList);
	if (list) {
		list->list = rz_list_new();
		list->offset = offset;
	}
	return list;
}

static inline RzBinDwarfLocRange *create_loc_range(ut64 start, ut64 end, RzBinDwarfBlock *block) {
	RzBinDwarfLocRange *range = RZ_NEW0(RzBinDwarfLocRange);
	if (range) {
		range->start = start;
		range->end = end;
		range->expression = block;
	}
	return range;
}

static void loc_list_tree(RzBinDwarfLocList *loc_list) {
	RzListIter *iter;
	RzBinDwarfLocRange *range;
	rz_list_foreach (loc_list->list, iter, range) {
		free(range->expression->data);
		free(range->expression);
		free(range);
	}
	rz_list_free(loc_list->list);
	free(loc_list);
}

static void block_free(RzBinDwarfBlock *block) {
	if (!block) {
		return;
	}
	free(block->data);
	free(block);
}

static HtUP *parse_loc_raw(HtUP /*<offset, List *<LocListEntry>*/ *loc_table, const ut8 *buf, size_t len, size_t addr_size,
	bool big_endian) {
	/* GNU has their own extensions GNU locviews that we can't parse */
	const ut8 *const buf_start = buf;
	const ut8 *buf_end = buf + len;
	/* for recognizing Base address entry */
	ut64 max_offset = get_max_offset(addr_size);

	ut64 address_base = 0; /* remember base of the loclist */
	ut64 list_offset = 0;

	RzBinDwarfLocList *loc_list = NULL;
	RzBinDwarfLocRange *range = NULL;
	while (buf && buf < buf_end) {
		ut64 start_addr = dwarf_read_address(addr_size, big_endian, &buf, buf_end);
		ut64 end_addr = dwarf_read_address(addr_size, big_endian, &buf, buf_end);

		if (start_addr == 0 && end_addr == 0) { /* end of list entry: 0, 0 */
			if (loc_list) {
				if (!ht_up_insert(loc_table, loc_list->offset, loc_list)) {
					loc_list_tree(loc_list);
				}
				list_offset = buf - buf_start;
				loc_list = NULL;
			}
			address_base = 0;
			continue;
		} else if (start_addr == max_offset && end_addr != max_offset) {
			/* base address, DWARF2 doesn't have this type of entry, these entries shouldn't
			   be in the list, they are just informational entries for further parsing (address_base) */
			address_base = end_addr;
		} else { /* location list entry: */
			if (!loc_list) {
				loc_list = create_loc_list(list_offset);
			}
			/* TODO in future parse expressions to better structure in dwarf.c and not in dwarf_process.c */
			RzBinDwarfBlock *block = RZ_NEW0(RzBinDwarfBlock);
			block->length = READ16(buf);
			buf = fill_block_data(buf, buf_end, block);
			range = create_loc_range(start_addr + address_base, end_addr + address_base, block);
			if (!range) {
				block_free(block);
			}
			rz_list_append(loc_list->list, range);
			range = NULL;
		}
	}
	/* if for some reason end of list is missing, then loc_list would leak */
	if (loc_list) {
		loc_list_tree(loc_list);
	}
	return loc_table;
}

/**
 * @brief Parses out the .debug_loc section into a table that maps each list as
 *        offset of a list -> LocationList
 *
 * @param binfile
 * @param addr_size machine address size used in executable (necessary for parsing)
 * @return RZ_API*
 */
RZ_API HtUP /*<offset, RzBinDwarfLocList *>*/ *rz_bin_dwarf_loc_parse(RzBinFile *binfile, int addr_size) {
	rz_return_val_if_fail(binfile, NULL);
	/* The standarparse_loc_raw_frame, not sure why is that */
	size_t len = 0;
	ut8 *buf = get_section_bytes(binfile, "debug_loc", &len);
	if (!buf) {
		return NULL;
	}
	HtUP /*<offset, RzBinDwarfLocList*/ *loc_table = ht_up_new0();
	if (!loc_table) {
		free(buf);
		return NULL;
	}
	loc_table = parse_loc_raw(loc_table, buf, len, addr_size, binfile->o && binfile->o->info && binfile->o->info->big_endian);
	free(buf);
	return loc_table;
}

static void ht_loc_list_free(HtUPKv *kv) {
	if (kv) {
		loc_list_tree(kv->value);
	}
}

RZ_API void rz_bin_dwarf_loc_free(HtUP /*<offset, RzBinDwarfLocList *>*/ *loc_table) {
	if (!loc_table) {
		return;
	}
	loc_table->opt.freefn = ht_loc_list_free;
	ht_up_free(loc_table);
}

RZ_API RZ_OWN RzBinDwarf *rz_bin_dwarf_parse(RZ_BORROW RZ_NONNULL RzBinFile *bf, RZ_BORROW RZ_NONNULL const RzBinDwarfParseOptions *opt) {
	rz_return_val_if_fail(bf && opt, NULL);
	RzBinDwarf *dw = RZ_NEW0(RzBinDwarf);
	if (!dw) {
		return NULL;
	}

	if (opt->flags & RZ_BIN_DWARF_PARSE_ABBREVS) {
		RZ_LOG_DEBUG(".debug_abbrev\n");
		dw->abbrevs = rz_bin_dwarf_abbrev_parse(bf);
	}
	if (opt->flags & RZ_BIN_DWARF_PARSE_INFO && dw->abbrevs) {
		RZ_LOG_DEBUG(".debug_info\n");
		dw->info = rz_bin_dwarf_info_parse(bf, dw->abbrevs);
	}
	if (opt->flags & RZ_BIN_DWARF_PARSE_LOC) {
		RZ_LOG_DEBUG(".debug_loc\n");
		dw->loc = rz_bin_dwarf_loc_parse(bf, opt->addr_size);
	}
	if (opt->flags & RZ_BIN_DWARF_PARSE_LINES && dw->info) {
		RZ_LOG_DEBUG(".debug_line\n");
		dw->lines = rz_bin_dwarf_parse_line(bf, dw->info, opt->line_mask);
	}
	if (opt->flags & RZ_BIN_DWARF_PARSE_ARANGES) {
		RZ_LOG_DEBUG(".debug_aranges\n");
		dw->aranges = rz_bin_dwarf_aranges_parse(bf);
	}
	return dw;
}

RZ_API void rz_bin_dwarf_free(RZ_OWN RzBinDwarf *dw) {
	if (!dw) {
		return;
	}
	rz_bin_dwarf_abbrev_free(dw->abbrevs);
	rz_bin_dwarf_info_free(dw->info);
	rz_bin_dwarf_loc_free(dw->loc);
	rz_bin_dwarf_line_info_free(dw->lines);
	rz_list_free(dw->aranges);
	free(dw);
}

RZ_API RzBinDwarfAttr *rz_bin_dwarf_die_get_attr(const RzBinDwarfDie *die, enum DW_AT name) {
	rz_return_val_if_fail(die, NULL);
	RzBinDwarfAttr *attr = NULL;
	rz_vector_foreach(&die->attrs, attr) {
		if (attr->name == name) {
			return attr;
		}
	}
	return NULL;
}

RZ_API RzBinDwarfAttrDef *rz_bin_dwarf_abbrev_get_attr(RZ_NONNULL const RzBinDwarfAbbrevDecl *abbrev, enum DW_AT name) {
	rz_return_val_if_fail(abbrev, NULL);
	RzBinDwarfAttrDef *attr = NULL;
	rz_vector_foreach(&abbrev->defs, attr) {
		if (attr->name == name) {
			return attr;
		}
	}
	return NULL;
}

RZ_API size_t rz_bin_dwarf_abbrev_count(RZ_NONNULL const RzBinDwarfDebugAbbrevs *da) {
	rz_return_val_if_fail(da, 0);
	return rz_vector_len(&da->decls);
}

RZ_API RzBinDwarfAbbrevDecl *rz_bin_dwarf_abbrev_get(RZ_NONNULL const RzBinDwarfDebugAbbrevs *da, size_t idx) {
	rz_return_val_if_fail(da, NULL);
	return rz_vector_index_ptr(&da->decls, idx);
}

RZ_API RzBinDwarfAbbrevDecl *rz_bin_dwarf_abbrev_by_offet(RZ_NONNULL const RzBinDwarfDebugAbbrevs *da, size_t offset) {
	rz_return_val_if_fail(da, NULL);
	return ht_up_find(da->decl_tbl, offset, NULL);
}

RZ_API size_t rz_bin_dwarf_abbrev_index_by_offet(RZ_NONNULL const RzBinDwarfDebugAbbrevs *da, size_t offset) {
	rz_return_val_if_fail(da, 0);
	return ht_uu_find(da->decl_index_tbl, offset, NULL);
}

RZ_API size_t rz_bin_dwarf_abbrev_decl_count(RZ_NONNULL const RzBinDwarfAbbrevDecl *decl) {
	rz_return_val_if_fail(decl, 0);
	return rz_vector_len(&decl->defs);
}

RZ_API RzBinDwarfAttrDef *rz_bin_dwarf_abbrev_decl_get(RZ_NONNULL const RzBinDwarfAbbrevDecl *decl, size_t idx) {
	rz_return_val_if_fail(decl, NULL);
	return rz_vector_index_ptr(&decl->defs, idx);
}

RZ_API RzBinDwarfAttrDef *rz_bin_dwarf_abbrev_attr_get(RZ_NONNULL const RzBinDwarfAbbrevDecl *decl, size_t idx) {
	rz_return_val_if_fail(decl, NULL);
	return rz_vector_index_ptr(&decl->defs, idx);
}
