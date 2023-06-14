// SPDX-FileCopyrightText: 2012-2018 pancake <pancake@nopcode.org>
// SPDX-FileCopyrightText: 2012-2018 Fedor Sakharov <fedor.sakharov@gmail.com>
// SPDX-FileCopyrightText: 2023 billow <billow.fun@gmail.com>
// SPDX-License-Identifier: LGPL-3.0-only

#include <rz_bin_dwarf.h>

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
	ut64 len = strlen(x) + 1;
	rz_buf_seek(buffer, (st64)len, SEEK_CUR);
	if (len <= 1) {
		free(x);
		return NULL;
	}
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
#define ULE128_WARP1(out, warp, ...) warp(rz_buf_uleb128(buffer, (ut64 *)&(out)) > 0, __VA_ARGS__)
#define SLE128_WARP1(out, warp, ...) warp(rz_buf_sleb128(buffer, (st64 *)&(out)) > 0, __VA_ARGS__)

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
#define U16_OR_GOTO(out, label)    U16_WARP1(out, GOTO_IF_FAIL, label)
#define U32_OR_GOTO(out, label)    U32_WARP1(out, GOTO_IF_FAIL, label)
#define U64_OR_GOTO(out, label)    U64_WARP1(out, GOTO_IF_FAIL, label)
#define UX_OR_GOTO(out, x, label)  UX_WRAP1(out, x, GOTO_IF_FAIL, label)
#define ULE128_OR_GOTO(out, label) ULE128_WARP1(out, GOTO_IF_FAIL, label)
#define SLE128_OR_GOTO(out, label) SLE128_WARP(out, GOTO_IF_FAIL, label)

static const char *indent_tbl[] = {
	"",
	"\t",
	"\t\t",
	"\t\t\t",
	"\t\t\t\t",
	"\t\t\t\t\t",
	"\t\t\t\t\t\t",
};

const char *indent_str(int indent) {
	if (indent < 0) {
		return "";
	}
	if (indent > RZ_ARRAY_SIZE(indent_tbl)) {
		indent = 6;
	}
	return indent_tbl[indent];
}

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

RZ_API const char *rz_bin_dwarf_children(enum DW_CHILDREN children) {
	if (children >= RZ_ARRAY_SIZE(dwarf_children)) {
		return NULL;
	}
	return dwarf_children[children];
}

static const char *dwarf_lns[] = {
	[DW_LNS_copy] = "DW_LNS_copy",
	[DW_LNS_advance_pc] = "DW_LNS_advance_pc",
	[DW_LNS_advance_line] = "DW_LNS_advance_line",
	[DW_LNS_set_file] = "DW_LNS_set_file",
	[DW_LNS_set_column] = "DW_LNS_set_column",
	[DW_LNS_negate_stmt] = "DW_LNS_negate_stmt",
	[DW_LNS_set_basic_block] = "DW_LNS_set_basic_block",
	[DW_LNS_const_add_pc] = "DW_LNS_const_add_pc",
	[DW_LNS_fixed_advance_pc] = "DW_LNS_fixed_advance_pc",
	[DW_LNS_set_prologue_end] = "DW_LNS_set_prologue_end",
	[DW_LNS_set_epilogue_begin] = "DW_LNS_set_epilogue_begin",
	[DW_LNS_set_isa] = "DW_LNS_set_isa",
};

RZ_API const char *rz_bin_dwarf_lns(enum DW_LNS lns) {
	if (lns >= RZ_ARRAY_SIZE(dwarf_lns)) {
		return NULL;
	}
	return dwarf_lns[lns];
}

static const char *dwarf_lne[] = {
	[DW_LNE_end_sequence] = "DW_LNE_end_sequence",
	[DW_LNE_set_address] = "DW_LNE_set_address",
	[DW_LNE_define_file] = "DW_LNE_define_file",
	[DW_LNE_set_discriminator] = "DW_LNE_set_discriminator",
	[DW_LNE_lo_user] = "DW_LNE_lo_user",
	[DW_LNE_hi_user] = "DW_LNE_hi_user",
};

RZ_API const char *rz_bin_dwarf_lne(enum DW_LNE lne) {
	if (lne >= RZ_ARRAY_SIZE(dwarf_lne)) {
		return NULL;
	}
	return dwarf_lne[lne];
}

static const char *dwarf_lnct[] = {
	[DW_LNCT_path] = "DW_LNCT_path",
	[DW_LNCT_directory_index] = "DW_LNCT_directory_index",
	[DW_LNCT_timestamp] = "DW_LNCT_timestamp",
	[DW_LNCT_size] = "DW_LNCT_size",
	[DW_LNCT_MD5] = "DW_LNCT_MD5",
	[DW_LNCT_lo_user] = "DW_LNCT_lo_user",
	[DW_LNCT_hi_user] = "DW_LNCT_hi_user",
};

RZ_API const char *rz_bin_dwarf_lnct(enum DW_LNCT lnct) {
	if (lnct >= RZ_ARRAY_SIZE(dwarf_lnct)) {
		return NULL;
	}
	return dwarf_lnct[lnct];
}

typedef struct {
	ut8 address_size;
	bool big_endian;
} Encoding;

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

typedef enum {
	DW_ATTR_TYPE_DEF,
	DW_ATTR_TYPE_FILE_ENTRY_FORMAT,
} DwAttrType;

typedef struct {
	DwAttrType type;
	union {
		RzBinDwarfAttrDef *def;
		RzBinDwarfFileEntryFormat *format;
	};
	union {
		RzBinDwarfLineHeader *line_hdr;
		RzBinDwarfCompUnitHdr *comp_unit_hdr;
	};
	RzBuffer *str_buffer;
	Encoding encoding;
} DwAttrOption;

/**
 * This function is quite incomplete and requires lot of work
 * With parsing various new FORM values
 * \brief Parses attribute value based on its definition
 *        and stores it into `value`
 */
static bool attr_parse(RzBuffer *buffer, RzBinDwarfAttr *value, DwAttrOption *in) {
	rz_return_val_if_fail(in && value && buffer, NULL);

	enum DW_AT name = 0;
	enum DW_FORM form = 0;
	enum DW_LNCT lnct = 0;
	ut8 address_size = 0;
	bool is_64bit = false;
	ut64 unit_offset = 0;
	if (in->type == DW_ATTR_TYPE_DEF) {
		name = in->def->name;
		form = in->def->form;
		address_size = in->comp_unit_hdr->address_size;
		is_64bit = in->comp_unit_hdr->is_64bit;
		unit_offset = in->comp_unit_hdr->unit_offset;
	} else if (in->type == DW_ATTR_TYPE_FILE_ENTRY_FORMAT) {
		lnct = in->format->content_type;
		form = in->format->form;
		address_size = in->line_hdr->address_size;
		is_64bit = in->line_hdr->is_64bit;
		unit_offset = in->line_hdr->offset;
	}

	bool big_endian = in->encoding.big_endian;
	RzBuffer *str_buffer = in->str_buffer;

	value->form = form;
	value->name = name;
	value->block.data = NULL;
	value->string.content = NULL;
	value->string.offset = 0;

	// http://www.dwarfstd.org/doc/DWARF4.pdf#page=161&zoom=100,0,560
	switch (form) {
	case DW_FORM_addr:
		value->kind = DW_AT_KIND_ADDRESS;
		UX_OR_RET_FALSE(value->address, address_size);
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
		value->kind = DW_AT_KIND_UCONSTANT;
		ULE128_OR_RET_FALSE(value->uconstant);
		break;
	case DW_FORM_string:
		value->kind = DW_AT_KIND_STRING;
		value->string.content = buf_get_string(buffer);
#define CHECK_STRING \
	if (!value->string.content) { \
		const char *tag_str = in->type == DW_ATTR_TYPE_DEF ? rz_bin_dwarf_attr(name) : (in->type == DW_ATTR_TYPE_FILE_ENTRY_FORMAT ? rz_bin_dwarf_lnct(lnct) : "unknown"); \
		RZ_LOG_ERROR("Failed to read string %s [%s]\n", tag_str, rz_bin_dwarf_form(form)); \
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
		RET_FALSE_IF_FAIL(read_offset(buffer, &value->string.offset, is_64bit, big_endian));
		if (str_buffer && value->string.offset < rz_buf_size(str_buffer)) {
			value->string.content = rz_buf_get_string(str_buffer, value->string.offset);
		}
		CHECK_STRING;
		break;
		// offset in .debug_info
	case DW_FORM_ref_addr:
		value->kind = DW_AT_KIND_REFERENCE;
		RET_FALSE_IF_FAIL(read_offset(buffer, &value->reference, is_64bit, big_endian));
		break;
		// This type of reference is an offset from the first byte of the compilation
		// header for the compilation unit containing the reference
	case DW_FORM_ref1:
		value->kind = DW_AT_KIND_REFERENCE;
		U8_OR_RET_FALSE(value->reference);
		value->reference += unit_offset;
		break;
	case DW_FORM_ref2:
		value->kind = DW_AT_KIND_REFERENCE;
		U16_OR_RET_FALSE(value->reference);
		value->reference += unit_offset;
		break;
	case DW_FORM_ref4:
		value->kind = DW_AT_KIND_REFERENCE;
		U32_OR_RET_FALSE(value->reference);
		value->reference += unit_offset;
		break;
	case DW_FORM_ref8:
		value->kind = DW_AT_KIND_REFERENCE;
		U64_OR_RET_FALSE(value->reference);
		value->reference += unit_offset;
		break;
	case DW_FORM_ref_udata:
		value->kind = DW_AT_KIND_REFERENCE;
		// uleb128 is enough to fit into ut64?
		ULE128_OR_RET_FALSE(value->reference);
		value->reference += unit_offset;
		break;
		// offset in a section other than .debug_info or .debug_str
	case DW_FORM_sec_offset:
		value->kind = DW_AT_KIND_REFERENCE;
		RET_FALSE_IF_FAIL(read_offset(buffer, &value->reference, is_64bit, big_endian));
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
		value->uconstant = in->type == DW_ATTR_TYPE_DEF ? in->def->special : 0;
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
		RET_FALSE_IF_FAIL(read_offset(buffer, &value->string.offset, is_64bit, big_endian));
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
		RET_FALSE_IF_FAIL(read_offset(buffer, &value->reference, is_64bit, big_endian));
		break;
		// An index into the .debug_rnglists
	case DW_FORM_rnglistx:
		value->kind = DW_AT_KIND_ADDRESS;
		ULE128_OR_RET_FALSE(value->address);
		break;
	default:
		RZ_LOG_ERROR("Unknown DW_FORM 0x%02" PFMT32x "\n", form);
		value->uconstant = 0;
		return false;
	}
	return true;
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

#include "./dwarf/line.inc"
#include "./dwarf/aranges.inc"
#include "./dwarf/abbrev.inc"
#include "./dwarf/unit.inc"
#include "./dwarf/loclists.inc"
#include "./dwarf/rnglists.inc"

RZ_OWN RzBinDwarf *rz_bin_dwarf_parse(RZ_BORROW RZ_NONNULL RzBinFile *bf, RZ_BORROW RZ_NONNULL const RzBinDwarfParseOptions *opt) {
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
