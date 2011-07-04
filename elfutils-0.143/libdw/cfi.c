/* CFI program execution.
   Copyright (C) 2009 Red Hat, Inc.
   This file is part of Red Hat elfutils.

   Red Hat elfutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 2 of the License.

   Red Hat elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with Red Hat elfutils; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301 USA.

   In addition, as a special exception, Red Hat, Inc. gives You the
   additional right to link the code of Red Hat elfutils with code licensed
   under any Open Source Initiative certified open source license
   (http://www.opensource.org/licenses/index.php) which requires the
   distribution of source code with any binary distribution and to
   distribute linked combinations of the two.  Non-GPL Code permitted under
   this exception must only link to the code of Red Hat elfutils through
   those well defined interfaces identified in the file named EXCEPTION
   found in the source code files (the "Approved Interfaces").  The files
   of Non-GPL Code may instantiate templates or use macros or inline
   functions from the Approved Interfaces without causing the resulting
   work to be covered by the GNU General Public License.  Only Red Hat,
   Inc. may make changes or additions to the list of Approved Interfaces.
   Red Hat's grant of this exception is conditioned upon your not adding
   any new exceptions.  If you wish to add a new Approved Interface or
   exception, please contact Red Hat.  You must obey the GNU General Public
   License in all respects for all of the Red Hat elfutils code and other
   code used in conjunction with Red Hat elfutils except the Non-GPL Code
   covered by this exception.  If you modify this file, you may extend this
   exception to your version of the file, but you are not obligated to do
   so.  If you do not wish to provide this exception without modification,
   you must delete this exception statement from your version and license
   this file solely under the GPL without exception.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <dwarf.h>
#include "../libebl/libebl.h"
#include "cfi.h"
#include "memory-access.h"
#include "encoded-value.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define CFI_PRIMARY_MAX	0x3f

static Dwarf_Frame *
duplicate_frame_state (const Dwarf_Frame *original,
		       Dwarf_Frame *prev)
{
  size_t size = offsetof (Dwarf_Frame, regs[original->nregs]);
  Dwarf_Frame *copy = malloc (size);
  if (likely (copy != NULL))
    {
      memcpy (copy, original, size);
      copy->prev = prev;
    }
  return copy;
}

/* Returns a DWARF_E_* error code, usually NOERROR or INVALID_CFI.  */
static int
execute_cfi (Dwarf_CFI *cache,
	     const struct dwarf_cie *cie,
	     Dwarf_Frame **state,
	     const uint8_t *program, const uint8_t *const end, bool abi_cfi,
	     Dwarf_Addr loc, Dwarf_Addr find_pc)
{
  /* The caller should not give us anything out of range.  */
  assert (loc <= find_pc);

  int result = DWARF_E_NOERROR;

#define cfi_assert(ok) do {						      \
    if (likely (ok)) break;						      \
    result = DWARF_E_INVALID_CFI;					      \
    goto out;								      \
  } while (0)

  Dwarf_Frame *fs = *state;
  inline bool enough_registers (Dwarf_Word reg)
    {
      if (fs->nregs <= reg)
	{
	  size_t size = offsetof (Dwarf_Frame, regs[reg + 1]);
	  Dwarf_Frame *bigger = realloc (fs, size);
	  if (unlikely (bigger == NULL))
	    {
	      result = DWARF_E_NOMEM;
	      return false;
	    }
	  else
	    {
	      bigger->nregs = reg + 1;
	      fs = bigger;
	    }
	}
      return true;
    }

#define register_rule(regno, r_rule, r_value) do {	\
    if (unlikely (! enough_registers (regno)))		\
      goto out;						\
    fs->regs[regno].rule = reg_##r_rule;		\
    fs->regs[regno].value = (r_value);			\
  } while (0)

  while (program < end)
    {
      uint8_t opcode = *program++;
      Dwarf_Word regno;
      Dwarf_Word offset;
      Dwarf_Word sf_offset;
      Dwarf_Word operand = opcode & CFI_PRIMARY_MAX;
      switch (opcode)
	{
	  /* These cases move LOC, i.e. "create a new table row".  */

	case DW_CFA_advance_loc1:
	  operand = *program++;
	case DW_CFA_advance_loc + 0 ... DW_CFA_advance_loc + CFI_PRIMARY_MAX:
	advance_loc:
	  loc += operand * cie->code_alignment_factor;
	  break;

	case DW_CFA_advance_loc2:
	  operand = read_2ubyte_unaligned_inc (cache, program);
	  goto advance_loc;
	case DW_CFA_advance_loc4:
	  operand = read_4ubyte_unaligned_inc (cache, program);
	  goto advance_loc;
	case DW_CFA_MIPS_advance_loc8:
	  operand = read_8ubyte_unaligned_inc (cache, program);
	  goto advance_loc;

	case DW_CFA_set_loc:
	  if (unlikely (read_encoded_value (cache, cie->fde_encoding, &program,
					    &loc)))
	    return INTUSE(dwarf_errno) ();
	  break;

	  /* Now all following cases affect this row, but do not touch LOC.
	     These cases end with 'continue'.  We only get out of the
	     switch block for the row-copying (LOC-moving) cases above.  */

	case DW_CFA_def_cfa:
	  get_uleb128 (operand, program);
	  get_uleb128 (offset, program);
	def_cfa:
	  fs->cfa_rule = cfa_offset;
	  fs->cfa_val_reg = operand;
	  fs->cfa_val_offset = offset;
	  /* Prime the rest of the Dwarf_Op so dwarf_frame_cfa can use it.  */
	  fs->cfa_data.offset.atom = DW_OP_bregx;
	  fs->cfa_data.offset.offset = 0;
	  continue;

	case DW_CFA_def_cfa_register:
	  get_uleb128 (regno, program);
	  cfi_assert (fs->cfa_rule == cfa_offset);
	  fs->cfa_val_reg = regno;
	  continue;

	case DW_CFA_def_cfa_sf:
	  get_uleb128 (operand, program);
	  get_sleb128 (sf_offset, program);
	  offset = sf_offset * cie->data_alignment_factor;
	  goto def_cfa;

	case DW_CFA_def_cfa_offset:
	  get_uleb128 (offset, program);
	def_cfa_offset:
	  cfi_assert (fs->cfa_rule == cfa_offset);
	  fs->cfa_val_offset = offset;
	  continue;

	case DW_CFA_def_cfa_offset_sf:
	  get_sleb128 (sf_offset, program);
	  offset = sf_offset * cie->data_alignment_factor;
	  goto def_cfa_offset;

	case DW_CFA_def_cfa_expression:
	  /* DW_FORM_block is a ULEB128 length followed by that many bytes.  */
	  get_uleb128 (operand, program);
	  cfi_assert (operand <= (Dwarf_Word) (end - program));
	  fs->cfa_rule = cfa_expr;
	  fs->cfa_data.expr.data = (unsigned char *) program;
	  fs->cfa_data.expr.length = operand;
	  program += operand;
	  continue;

	case DW_CFA_undefined:
	  get_uleb128 (regno, program);
	  register_rule (regno, undefined, 0);
	  continue;

	case DW_CFA_same_value:
	  get_uleb128 (regno, program);
	  register_rule (regno, same_value, 0);
	  continue;

	case DW_CFA_offset_extended:
	  get_uleb128 (operand, program);
	case DW_CFA_offset + 0 ... DW_CFA_offset + CFI_PRIMARY_MAX:
	  get_uleb128 (offset, program);
	  offset *= cie->data_alignment_factor;
	offset_extended:
	  register_rule (operand, offset, offset);
	  continue;

	case DW_CFA_offset_extended_sf:
	  get_uleb128 (operand, program);
	  get_sleb128 (sf_offset, program);
	offset_extended_sf:
	  offset = sf_offset * cie->data_alignment_factor;
	  goto offset_extended;

	case DW_CFA_GNU_negative_offset_extended:
	  /* GNU extension obsoleted by DW_CFA_offset_extended_sf.  */
	  get_uleb128 (operand, program);
	  get_uleb128 (offset, program);
	  sf_offset = -offset;
	  goto offset_extended_sf;

	case DW_CFA_val_offset:
	  get_uleb128 (operand, program);
	  get_uleb128 (offset, program);
	  offset *= cie->data_alignment_factor;
	val_offset:
	  register_rule (operand, val_offset, offset);
	  continue;

	case DW_CFA_val_offset_sf:
	  get_uleb128 (operand, program);
	  get_sleb128 (sf_offset, program);
	  offset = sf_offset * cie->data_alignment_factor;
	  goto val_offset;

	case DW_CFA_register:
	  get_uleb128 (regno, program);
	  get_uleb128 (operand, program);
	  register_rule (regno, register, operand);
	  continue;

	case DW_CFA_expression:
	  get_uleb128 (regno, program);
	  offset = program - (const uint8_t *) cache->data->d.d_buf;
	  /* DW_FORM_block is a ULEB128 length followed by that many bytes.  */
	  get_uleb128 (operand, program);
	  cfi_assert (operand <= (Dwarf_Word) (end - program));
	  program += operand;
	  register_rule (regno, expression, offset);
	  continue;

	case DW_CFA_val_expression:
	  get_uleb128 (regno, program);
	  /* DW_FORM_block is a ULEB128 length followed by that many bytes.  */
	  offset = program - (const uint8_t *) cache->data->d.d_buf;
	  get_uleb128 (operand, program);
	  cfi_assert (operand <= (Dwarf_Word) (end - program));
	  program += operand;
	  register_rule (regno, val_expression, offset);
	  continue;

	case DW_CFA_restore_extended:
	  get_uleb128 (operand, program);
	case DW_CFA_restore + 0 ... DW_CFA_restore + CFI_PRIMARY_MAX:

	  if (unlikely (abi_cfi) && likely (opcode == DW_CFA_restore))
	    {
	      /* Special case hack to give backend abi_cfi a shorthand.  */
	      cache->default_same_value = true;
	      continue;
	    }

	  /* This can't be used in the CIE's own initial instructions.  */
	  cfi_assert (cie->initial_state != NULL);

	  /* Restore the CIE's initial rule for this register.  */
	  if (unlikely (! enough_registers (operand)))
	    goto out;
	  if (cie->initial_state->nregs > operand)
	    fs->regs[operand] = cie->initial_state->regs[operand];
	  else
	    fs->regs[operand].rule = reg_unspecified;
	  continue;

	case DW_CFA_remember_state:
	  {
	    /* Duplicate the state and chain the copy on.  */
	    Dwarf_Frame *copy = duplicate_frame_state (fs, fs);
	    if (unlikely (copy == NULL))
	      {
		result = DWARF_E_NOMEM;
		goto out;
	      }
	    fs = copy;
	    continue;
	  }

	case DW_CFA_restore_state:
	  {
	    /* Pop the current state off and use the old one instead.  */
	    Dwarf_Frame *prev = fs->prev;
	    cfi_assert (prev != NULL);
	    free (fs);
	    fs = prev;
	  }
	  continue;

	case DW_CFA_nop:
	  continue;

	case DW_CFA_GNU_window_save:
	  /* This is magic shorthand used only by SPARC.  It's equivalent
	     to a bunch of DW_CFA_register and DW_CFA_offset operations.  */
	  if (unlikely (! enough_registers (31)))
	    goto out;
	  for (regno = 8; regno < 16; ++regno)
	    {
	      /* Find each %oN in %iN.  */
	      fs->regs[regno].rule = reg_register;
	      fs->regs[regno].value = regno + 16;
	    }
	  unsigned int address_size = (cache->e_ident[EI_CLASS] == ELFCLASS32
				       ? 4 : 8);
	  for (; regno < 32; ++regno)
	    {
	      /* Find %l0..%l7 and %i0..%i7 in a block at the CFA.  */
	      fs->regs[regno].rule = reg_offset;
	      fs->regs[regno].value = (regno - 16) * address_size;
	    }
	  continue;

	case DW_CFA_GNU_args_size:
	  /* XXX is this useful for anything? */
	  get_uleb128 (operand, program);
	  continue;

	default:
	  cfi_assert (false);
	  continue;
	}

      /* We get here only for the cases that have just moved LOC.  */
      if (find_pc >= loc)
	/* This advance has not yet reached FIND_PC.  */
	fs->start = loc;
      else
	{
	  /* We have just advanced past the address we're looking for.
	     The state currently described is what we want to see.  */
	  fs->end = loc;
	  break;
	}
    }

  /* "The end of the instruction stream can be thought of as a
     DW_CFA_set_loc (initial_location + address_range) instruction."
     (DWARF 3.0 Section 6.4.3)

     When we fall off the end of the program without an advance_loc/set_loc
     that put us past FIND_PC, the final state left by the FDE program
     applies to this address (the caller ensured it was inside the FDE).
     This address (FDE->end) is already in FS->end as set by the caller.  */

#undef register_rule
#undef cfi_assert

 out:

  /* Pop any remembered states left on the stack.  */
  while (fs->prev != NULL)
    {
      Dwarf_Frame *prev = fs->prev;
      fs->prev = prev->prev;
      free (prev);
    }

  if (result == DWARF_E_NOERROR)
    *state = fs;

  return result;
}

static int
cie_cache_initial_state (Dwarf_CFI *cache, struct dwarf_cie *cie)
{
  int result = DWARF_E_NOERROR;

  if (likely (cie->initial_state != NULL))
    return result;

  /* This CIE has not been used before.  Play out its initial
     instructions and cache the initial state that results.
     First we'll let the backend fill in the default initial
     state for this machine's ABI.  */

  Dwarf_CIE abi_info = { DW_CIE_ID_64, NULL, NULL, 1, 1, -1, "", NULL, 0, 0 };

  /* Make sure we have a backend handle cached.  */
  if (unlikely (cache->ebl == NULL))
    {
      cache->ebl = ebl_openbackend (cache->data->s->elf);
      if (unlikely (cache->ebl == NULL))
	cache->ebl = (void *) -1l;
    }

  /* Fetch the ABI's default CFI program.  */
  if (likely (cache->ebl != (void *) -1l)
      && unlikely (ebl_abi_cfi (cache->ebl, &abi_info) < 0))
    return DWARF_E_UNKNOWN_ERROR;

  Dwarf_Frame *cie_fs = calloc (1, sizeof (Dwarf_Frame));
  if (unlikely (cie_fs == NULL))
    return DWARF_E_NOMEM;

  /* If the default state of any register is not "undefined"
     (i.e. call-clobbered), then the backend supplies instructions
     for the standard initial state.  */
  if (abi_info.initial_instructions_end > abi_info.initial_instructions)
    {
      /* Dummy CIE for backend's instructions.  */
      struct dwarf_cie abi_cie =
	{
	  .code_alignment_factor = abi_info.code_alignment_factor,
	  .data_alignment_factor = abi_info.data_alignment_factor,
	};
      result = execute_cfi (cache, &abi_cie, &cie_fs,
			    abi_info.initial_instructions,
			    abi_info.initial_instructions_end, true,
			    0, (Dwarf_Addr) -1l);
    }

  /* Now run the CIE's initial instructions.  */
  if (cie->initial_instructions_end > cie->initial_instructions
      && likely (result == DWARF_E_NOERROR))
    result = execute_cfi (cache, cie, &cie_fs,
			  cie->initial_instructions,
			  cie->initial_instructions_end, false,
			  0, (Dwarf_Addr) -1l);

  if (unlikely (result != DWARF_E_NOERROR))
    {
      free (cie_fs);
      return result;
    }

  /* Now we have the initial state of things that all
     FDEs using this CIE will start from.  */
  cie_fs->cache = cache;
  cie->initial_state = cie_fs;

  return result;
}

int
internal_function
__libdw_frame_at_address (Dwarf_CFI *cache, struct dwarf_fde *fde,
			  Dwarf_Addr address, Dwarf_Frame **frame)
{
  int result = cie_cache_initial_state (cache, fde->cie);
  if (likely (result == DWARF_E_NOERROR))
    {
      Dwarf_Frame *fs = duplicate_frame_state (fde->cie->initial_state, NULL);
      if (unlikely (fs == NULL))
	return DWARF_E_NOMEM;

      fs->fde = fde;
      fs->start = fde->start;
      fs->end = fde->end;

      result = execute_cfi (cache, fde->cie, &fs,
			    fde->instructions, fde->instructions_end, false,
			    fde->start, address);
      if (unlikely (result != DWARF_E_NOERROR))
	free (fs);
      else
	*frame = fs;
    }
  return result;
}
