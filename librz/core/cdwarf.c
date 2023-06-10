// SPDX-FileCopyrightText: 2021 thestr4ng3r <info@florianmaerkl.de>
// SPDX-License-Identifier: LGPL-3.0-only

#include <rz_core.h>

RZ_API void rz_core_bin_dwarf_print_abbrev_section(const RzBinDwarfDebugAbbrevs *da) {
	if (!da) {
		return;
	}
	void *itdecl;
	rz_vector_foreach(&da->decls, itdecl) {
		if (!itdecl) {
			return;
		}
		RzBinDwarfAbbrevDecl *decl = itdecl;
		rz_cons_printf("   %-4" PFMT64d " ", decl->code);
		const char *tagname = rz_bin_dwarf_get_tag_name(decl->tag);
		if (tagname) {
			rz_cons_printf("  %-25s ", tagname);
		}
		rz_cons_printf("[%s]", decl->has_children ? "has children" : "no children");
		rz_cons_printf(" (0x%" PFMT64x ")\n", decl->offset);

		void *itdef;
		rz_vector_foreach(&decl->defs, itdef) {
			RzBinDwarfAttrDef *def = itdef;
			const char *attr_name = rz_bin_dwarf_get_attr_name(def->name);
			const char *attr_form_name = rz_bin_dwarf_get_attr_form_name(def->form);
			if (attr_name && attr_form_name) {
				rz_cons_printf("    %-30s %-30s\n", attr_name, attr_form_name);
			}
		}
	}
}

RZ_API void rz_core_bin_dwarf_print_attr_value(const RzBinDwarfAttr *val) {
	size_t i;
	rz_return_if_fail(val);

	switch (val->form) {
	case DW_FORM_block:
	case DW_FORM_block1:
	case DW_FORM_block2:
	case DW_FORM_block4:
	case DW_FORM_exprloc:
		rz_cons_printf("%" PFMT64u " byte block:", val->block.length);
		for (i = 0; i < val->block.length; i++) {
			rz_cons_printf(" 0x%02x", val->block.data[i]);
		}
		break;
	case DW_FORM_data1:
	case DW_FORM_data2:
	case DW_FORM_data4:
	case DW_FORM_data8:
	case DW_FORM_data16:
		rz_cons_printf("%" PFMT64u "", val->uconstant);
		if (val->name == DW_AT_language) {
			const char *lang_name = rz_bin_dwarf_get_lang_name(val->uconstant);
			if (lang_name) {
				rz_cons_printf("   (%s)", lang_name);
			}
		}
		break;
	case DW_FORM_string:
		if (val->string.content) {
			rz_cons_printf("%s", val->string.content);
		} else {
			rz_cons_print("No string found");
		}
		break;
	case DW_FORM_flag:
		rz_cons_printf("%u", val->flag);
		break;
	case DW_FORM_sdata:
		rz_cons_printf("%" PFMT64d "", val->sconstant);
		break;
	case DW_FORM_udata:
		rz_cons_printf("%" PFMT64u "", val->uconstant);
		break;
	case DW_FORM_ref_addr:
	case DW_FORM_ref1:
	case DW_FORM_ref2:
	case DW_FORM_ref4:
	case DW_FORM_ref8:
	case DW_FORM_ref_sig8:
	case DW_FORM_ref_udata:
	case DW_FORM_ref_sup4:
	case DW_FORM_ref_sup8:
	case DW_FORM_sec_offset:
		rz_cons_printf("<0x%" PFMT64x ">", val->reference);
		break;
	case DW_FORM_flag_present:
		rz_cons_print("1");
		break;
	case DW_FORM_strx:
	case DW_FORM_strx1:
	case DW_FORM_strx2:
	case DW_FORM_strx3:
	case DW_FORM_strx4:
	case DW_FORM_line_ptr:
	case DW_FORM_strp_sup:
	case DW_FORM_strp:
		rz_cons_printf("(indirect string, offset: 0x%" PFMT64x "): %s",
			val->string.offset, val->string.content);
		break;
	case DW_FORM_addr:
	case DW_FORM_addrx:
	case DW_FORM_addrx1:
	case DW_FORM_addrx2:
	case DW_FORM_addrx3:
	case DW_FORM_addrx4:
	case DW_FORM_loclistx:
	case DW_FORM_rnglistx:
		rz_cons_printf("0x%" PFMT64x "", val->address);
		break;
	case DW_FORM_implicit_const:
		rz_cons_printf("0x%" PFMT64d "", val->uconstant);
		break;
	default:
		rz_cons_printf("Unknown attr value form %" PFMT32d "\n", val->form);
		break;
	};
}

RZ_API void rz_core_bin_dwarf_print_debug_info(const RzBinDwarfDebugInfo *info) {
	rz_return_if_fail(info);
	void *it;
	rz_vector_foreach(&info->units, it) {
		RzBinDwarfCompUnit *unit = it;
		rz_cons_print("\n");
		rz_cons_printf("  Compilation Unit @ offset 0x%" PFMT64x ":\n", unit->offset);
		rz_cons_printf("   Length:        0x%" PFMT64x "\n", unit->hdr.length);
		rz_cons_printf("   Version:       %d\n", unit->hdr.version);
		rz_cons_printf("   Abbrev Offset: 0x%" PFMT64x "\n", unit->hdr.abbrev_offset);
		rz_cons_printf("   Pointer Size:  %d\n", unit->hdr.address_size);
		const char *unit_type_name = rz_bin_dwarf_get_unit_type_name(unit->hdr.unit_type);
		if (unit_type_name) {
			rz_cons_printf("   Unit Type:     %s\n", unit_type_name);
		}
		rz_cons_print("\n");

		void *it_die;
		rz_vector_foreach(&unit->dies, it_die) {
			RzBinDwarfDie *die = it_die;
			rz_cons_printf("<0x%" PFMT64x ">: Abbrev Number: %-4" PFMT64u " ", die->offset, die->abbrev_code);

			const char *tag_name = rz_bin_dwarf_get_tag_name(die->tag);
			if (tag_name) {
				rz_cons_printf("(%s)\n", tag_name);
			} else {
				rz_cons_print("(Unknown abbrev tag)\n");
			}

			if (!die->abbrev_code) {
				continue;
			}

			void *it_attr;
			rz_vector_foreach(&die->attrs, it_attr) {
				RzBinDwarfAttr *attr = it_attr;
				if (!attr->name) {
					continue;
				}
				const char *attr_name = rz_bin_dwarf_get_attr_name(attr->name);
				if (attr_name) {
					rz_cons_printf("     %-25s : ", attr_name);
				} else {
					rz_cons_printf("     AT_UNKWN [0x%-3" PFMT32x "]\t : ", attr->name);
				}
				rz_core_bin_dwarf_print_attr_value(attr);
				rz_cons_printf("\n");
			}
		}
	}
}

static int offset_comp(const void *a, const void *b) {
	const RzBinDwarfLocList *f = a;
	const RzBinDwarfLocList *s = b;
	ut64 first = f->offset;
	ut64 second = s->offset;
	if (first < second) {
		return -1;
	}
	if (first > second) {
		return 1;
	}
	return 0;
}

static bool sort_loclists(void *user, const ut64 key, const void *value) {
	RzBinDwarfLocList *loc_list = (RzBinDwarfLocList *)value;
	RzList *sort_list = user;
	rz_list_add_sorted(sort_list, loc_list, offset_comp);
	return true;
}

RZ_API void rz_core_bin_dwarf_print_loc(HtUP /*<offset, RzBinDwarfLocList *>*/ *loc_table, int addr_size) {
	rz_return_if_fail(loc_table);
	rz_cons_print("\nContents of the .debug_loc section:\n");
	RzList /*<RzBinDwarfLocList *>*/ *sort_list = rz_list_new();
	/* sort the table contents by offset and print sorted
	   a bit ugly, but I wanted to decouple the parsing and printing */
	ht_up_foreach(loc_table, sort_loclists, sort_list);
	RzListIter *i;
	RzBinDwarfLocList *loc_list;
	rz_list_foreach (sort_list, i, loc_list) {
		RzListIter *j;
		RzBinDwarfLocRange *range;
		ut64 base_offset = loc_list->offset;
		rz_list_foreach (loc_list->list, j, range) {
			rz_cons_printf("0x%" PFMT64x " 0x%" PFMT64x " 0x%" PFMT64x "\n", base_offset, range->start, range->end);
			base_offset += addr_size * 2;
			if (range->expression) {
				base_offset += 2 + range->expression->length; /* 2 bytes for expr length */
			}
		}
		rz_cons_printf("0x%" PFMT64x " <End of list>\n", base_offset);
	}
	rz_cons_print("\n");
	rz_list_free(sort_list);
}

RZ_API void rz_core_bin_dwarf_print_aranges(RzList /*<RzBinDwarfARangeSet *>*/ *aranges) {
	rz_return_if_fail(aranges);
	rz_cons_print("\nContents of the .debug_aranges section:\n");
	RzListIter *it;
	RzBinDwarfARangeSet *set;
	rz_list_foreach (aranges, it, set) {
		rz_cons_print("  Address Range Set\n");
		rz_cons_printf("   Unit Length:           0x%" PFMT64x "\n", set->unit_length);
		rz_cons_printf("   64bit:                 %s\n", rz_str_bool(set->is_64bit));
		rz_cons_printf("   Version:               %u\n", (unsigned int)set->version);
		rz_cons_printf("   Offset in .debug_info: 0x%" PFMT64x "\n", set->debug_info_offset);
		rz_cons_printf("   Address Size:          %u\n", (unsigned int)set->address_size);
		rz_cons_printf("   Segment Size:          %u\n", (unsigned int)set->segment_size);
		rz_cons_print("   Ranges:\n");
		rz_cons_print("    address            length\n");
		for (size_t i = 0; i < set->aranges_count; i++) {
			rz_cons_printf("    0x%016" PFMT64x " 0x%016" PFMT64x "\n", set->aranges[i].addr, set->aranges[i].length);
		}
	}
	rz_cons_print("\n");
}

/**
 * \param regs optional, the state after op has been executed. If not null, some meaningful results from this context will be shown.
 */
static void print_line_op(RzBinDwarfLineOp *op, RzBinDwarfLineHeader *hdr, RZ_NULLABLE RzBinDwarfSMRegisters *regs) {
	switch (op->type) {
	case RZ_BIN_DWARF_LINE_OP_TYPE_STD:
		switch (op->opcode) {
		case DW_LNS_copy:
			rz_cons_print("Copy");
			break;
		case DW_LNS_advance_pc:
			rz_cons_printf("Advance PC by %" PFMT64u, op->args.advance_pc * hdr->min_inst_len);
			if (regs) {
				rz_cons_printf(" to 0x%" PFMT64x, regs->address);
			}
			break;
		case DW_LNS_advance_line:
			rz_cons_printf("Advance line by %" PFMT64d, op->args.advance_line);
			if (regs) {
				rz_cons_printf(", to %" PFMT64d, regs->line);
			}
			break;
		case DW_LNS_set_file:
			rz_cons_printf("Set file to %" PFMT64d, op->args.set_file);
			break;
		case DW_LNS_set_column:
			rz_cons_printf("Set column to %" PFMT64d, op->args.set_column);
			break;
		case DW_LNS_negate_stmt:
			if (regs) {
				rz_cons_printf("Set is_stmt to %u", (unsigned int)regs->is_stmt);
			} else {
				rz_cons_print("Negate is_stmt");
			}
			break;
		case DW_LNS_set_basic_block:
			rz_cons_print("set_basic_block");
			break;
		case DW_LNS_const_add_pc:
			rz_cons_printf("Advance PC by constant %" PFMT64u, rz_bin_dwarf_line_header_get_spec_op_advance_pc(hdr, 255));
			if (regs) {
				rz_cons_printf(" to 0x%" PFMT64x, regs->address);
			}
			break;
		case DW_LNS_fixed_advance_pc:
			rz_cons_printf("Fixed advance pc by %" PFMT64u, op->args.fixed_advance_pc);
			rz_cons_printf(" to %" PFMT64d, regs->address);
			break;
		case DW_LNS_set_prologue_end:
			rz_cons_print("set_prologue_end");
			break;
		case DW_LNS_set_epilogue_begin:
			rz_cons_print("set_epilogue_begin");
			break;
		case DW_LNS_set_isa:
			rz_cons_printf("set_isa to %" PFMT64u, op->args.set_isa);
			break;
		default:
			rz_cons_printf("Unknown Standard Opcode %u", (unsigned int)op->opcode);
			break;
		}
		break;
	case RZ_BIN_DWARF_LINE_OP_TYPE_EXT:
		rz_cons_printf("Extended opcode %u: ", (unsigned int)op->opcode);
		switch (op->opcode) {
		case DW_LNE_end_sequence:
			rz_cons_print("End of Sequence");
			break;
		case DW_LNE_set_address:
			rz_cons_printf("set Address to 0x%" PFMT64x, op->args.set_address);
			break;
		case DW_LNE_define_file:
			rz_cons_printf("define_file \"%s\", dir_index %" PFMT64u ", ",
				op->args.define_file.filename,
				op->args.define_file.dir_index);
			break;
		case DW_LNE_set_discriminator:
			rz_cons_printf("set Discriminator to %" PFMT64u "\n", op->args.set_discriminator);
			break;
		default:
			rz_cons_printf("Unknown");
			break;
		}
		break;
	case RZ_BIN_DWARF_LINE_OP_TYPE_SPEC:
		rz_cons_printf("Special opcode %u: ", (unsigned int)rz_bin_dwarf_line_header_get_adj_opcode(hdr, op->opcode));
		rz_cons_printf("advance Address by %" PFMT64u, rz_bin_dwarf_line_header_get_spec_op_advance_pc(hdr, op->opcode));
		if (regs) {
			rz_cons_printf(" to 0x%" PFMT64x, regs->address);
		}
		rz_cons_printf(" and Line by %" PFMT64d, rz_bin_dwarf_line_header_get_spec_op_advance_line(hdr, op->opcode));
		if (regs) {
			rz_cons_printf(" to %" PFMT64u, regs->line);
		}
		break;
	default:
		rz_cons_printf("Unknown opcode type %u, opcode: %x", (unsigned int)op->type, op->opcode);
		break;
	}
	rz_cons_print("\n");
}

RZ_API void rz_core_bin_dwarf_print_line_units(RzList /*<RzBinDwarfLineUnit *>*/ *lines) {
	rz_return_if_fail(lines);
	rz_cons_print("Raw dump of debug contents of section .debug_line:\n\n");
	RzListIter *it;
	RzBinDwarfLineUnit *unit;
	bool first = true;
	rz_list_foreach (lines, it, unit) {
		if (first) {
			first = false;
		} else {
			rz_cons_print("\n");
		}
		RzBinDwarfLineHeader *hdr = &unit->header;
		rz_cons_print(" Header information:\n");
		rz_cons_printf("  Length:                             %" PFMT64u "\n", hdr->unit_length);
		rz_cons_printf("  DWARF Version:                      %d\n", hdr->version);
		rz_cons_printf("  Header Length:                      %" PFMT64d "\n", hdr->header_length);
		rz_cons_printf("  Minimum Instruction Length:         %d\n", hdr->min_inst_len);
		rz_cons_printf("  Maximum Operations per Instruction: %d\n", hdr->max_ops_per_inst);
		rz_cons_printf("  Initial value of 'is_stmt':         %d\n", hdr->default_is_stmt);
		rz_cons_printf("  Line Base:                          %d\n", hdr->line_base);
		rz_cons_printf("  Line Range:                         %d\n", hdr->line_range);
		rz_cons_printf("  Opcode Base:                        %d\n\n", hdr->opcode_base);
		rz_cons_print(" Opcodes:\n");
		for (size_t i = 1; i < hdr->opcode_base; i++) {
			rz_cons_printf("  Opcode %zu has %d arg\n", i, hdr->std_opcode_lengths[i - 1]);
		}
		rz_cons_print("\n");
		if (rz_pvector_len(&hdr->directories) > 0) {
			rz_cons_printf(" The Directory Table:\n");
			for (size_t i = 0; i < rz_pvector_len(&hdr->directories); i++) {
				rz_cons_printf("  %u     %s\n", (unsigned int)i + 1, (char *)rz_pvector_at(&hdr->directories, i));
			}
		}
		if (rz_vector_len(&hdr->file_names)) {
			rz_cons_print("\n");
			rz_cons_print(" The File Name Table:\n");
			rz_cons_print("  Entry Dir     Time      Size       Name\n");
			for (size_t i = 0; i < rz_vector_len(&hdr->file_names); i++) {
				RzBinDwarfLineFileEntry *f = rz_vector_index_ptr(&hdr->file_names, i);
				rz_cons_printf("  %u     %" PFMT32u "       %" PFMT32u "         %" PFMT32u "          %s\n",
					(unsigned int)i + 1, f->id_idx, f->mod_time, f->file_len, f->name);
			}
			rz_cons_print("\n");
		}
		// also execute all ops simultaneously which gives us nice intermediate value printing
		RzBinDwarfSMRegisters regs;
		rz_bin_dwarf_line_header_reset_regs(&unit->header, &regs);
		rz_cons_print(" Line Number Statements:\n");
		void *opsit;
		size_t i;
		rz_vector_enumerate(&unit->ops, opsit, i) {
			RzBinDwarfLineOp *op = opsit;
			rz_bin_dwarf_line_op_run(&unit->header, &regs, op, NULL, NULL, NULL);
			rz_cons_print("  ");
			print_line_op(op, &unit->header, &regs);
			if (op->type == RZ_BIN_DWARF_LINE_OP_TYPE_EXT && op->ext_opcode == DW_LNE_end_sequence && i + 1 < rz_vector_len(&unit->ops)) {
				// extra newline for nice sequence separation
				rz_cons_print("\n");
			}
		}
	}
	rz_cons_print("\n");
}
