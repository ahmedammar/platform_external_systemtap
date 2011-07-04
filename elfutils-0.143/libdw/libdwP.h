/* Internal definitions for libdwarf.
   Copyright (C) 2002-2009 Red Hat, Inc.
   This file is part of Red Hat elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#ifndef _LIBDWP_H
#define _LIBDWP_H 1

#include <libintl.h>
#include <stdbool.h>

#include <libdw.h>


/* gettext helper macros.  */
#define _(Str) dgettext ("elfutils", Str)


/* Version of the DWARF specification we support.  */
#define DWARF_VERSION 3

/* Version of the CIE format.  */
#define CIE_VERSION 1


/* Known location expressions already decoded.  */
struct loc_s
{
  void *addr;
  Dwarf_Op *loc;
  size_t nloc;
};

/* Known DW_OP_implicit_value blocks already decoded.
   This overlaps struct loc_s exactly, but only the
   first member really has to match.  */
struct loc_block_s
{
  void *addr;
  unsigned char *data;
  size_t length;
};

/* Valid indeces for the section data.  */
enum
  {
    IDX_debug_info = 0,
    IDX_debug_abbrev,
    IDX_debug_aranges,
    IDX_debug_line,
    IDX_debug_frame,
    IDX_debug_loc,
    IDX_debug_pubnames,
    IDX_debug_str,
    IDX_debug_funcnames,
    IDX_debug_typenames,
    IDX_debug_varnames,
    IDX_debug_weaknames,
    IDX_debug_macinfo,
    IDX_debug_ranges,
    IDX_last
  };


/* Error values.  */
enum
{
  DWARF_E_NOERROR = 0,
  DWARF_E_UNKNOWN_ERROR,
  DWARF_E_INVALID_ACCESS,
  DWARF_E_NO_REGFILE,
  DWARF_E_IO_ERROR,
  DWARF_E_INVALID_ELF,
  DWARF_E_NO_DWARF,
  DWARF_E_NOELF,
  DWARF_E_GETEHDR_ERROR,
  DWARF_E_NOMEM,
  DWARF_E_UNIMPL,
  DWARF_E_INVALID_CMD,
  DWARF_E_INVALID_VERSION,
  DWARF_E_INVALID_FILE,
  DWARF_E_NO_ENTRY,
  DWARF_E_INVALID_DWARF,
  DWARF_E_NO_STRING,
  DWARF_E_NO_ADDR,
  DWARF_E_NO_CONSTANT,
  DWARF_E_NO_REFERENCE,
  DWARF_E_INVALID_REFERENCE,
  DWARF_E_NO_DEBUG_LINE,
  DWARF_E_INVALID_DEBUG_LINE,
  DWARF_E_TOO_BIG,
  DWARF_E_VERSION,
  DWARF_E_INVALID_DIR_IDX,
  DWARF_E_ADDR_OUTOFRANGE,
  DWARF_E_NO_LOCLIST,
  DWARF_E_NO_BLOCK,
  DWARF_E_INVALID_LINE_IDX,
  DWARF_E_INVALID_ARANGE_IDX,
  DWARF_E_NO_MATCH,
  DWARF_E_NO_FLAG,
  DWARF_E_INVALID_OFFSET,
  DWARF_E_NO_DEBUG_RANGES,
  DWARF_E_INVALID_CFI,
};


/* This is the structure representing the debugging state.  */
struct Dwarf
{
  /* The underlying ELF file.  */
  Elf *elf;

  /* The section data.  */
  Elf_Data *sectiondata[IDX_last];

  /* True if the file has a byte order different from the host.  */
  bool other_byte_order;

  /* If true, we allocated the ELF descriptor ourselves.  */
  bool free_elf;

  /* Information for traversing the .debug_pubnames section.  This is
     an array and separately allocated with malloc.  */
  struct pubnames_s
  {
    Dwarf_Off cu_offset;
    Dwarf_Off set_start;
    unsigned int cu_header_size;
    int address_len;
  } *pubnames_sets;
  size_t pubnames_nsets;

  /* Search tree for the CUs.  */
  void *cu_tree;
  Dwarf_Off next_cu_offset;

  /* Address ranges.  */
  Dwarf_Aranges *aranges;

  /* Cached info from the CFI section.  */
  struct Dwarf_CFI_s *cfi;

  /* Internal memory handling.  This is basically a simplified
     reimplementation of obstacks.  Unfortunately the standard obstack
     implementation is not usable in libraries.  */
  struct libdw_memblock
  {
    size_t size;
    size_t remaining;
    struct libdw_memblock *prev;
    char mem[0];
  } *mem_tail;

  /* Default size of allocated memory blocks.  */
  size_t mem_default_size;

  /* Registered OOM handler.  */
  Dwarf_OOM oom_handler;
};


/* Abbreviation representation.  */
struct Dwarf_Abbrev
{
  Dwarf_Off offset;
  unsigned char *attrp;
  unsigned int attrcnt;
  unsigned int code;
  unsigned int tag;
  bool has_children;
};

#include "dwarf_abbrev_hash.h"


/* Files in line information records.  */
struct Dwarf_Files_s
  {
    struct Dwarf_CU *cu;
    unsigned int ndirs;
    unsigned int nfiles;
    struct Dwarf_Fileinfo_s
    {
      char *name;
      Dwarf_Word mtime;
      Dwarf_Word length;
    } info[0];
    /* nfiles of those, followed by char *[ndirs].  */
  };
typedef struct Dwarf_Fileinfo_s Dwarf_Fileinfo;


/* Representation of a row in the line table.  */

struct Dwarf_Line_s
{
  Dwarf_Files *files;

  Dwarf_Addr addr;
  unsigned int file;
  int line;
  unsigned short int column;
  unsigned int is_stmt:1;
  unsigned int basic_block:1;
  unsigned int end_sequence:1;
  unsigned int prologue_end:1;
  unsigned int epilogue_begin:1;
};

struct Dwarf_Lines_s
{
  size_t nlines;
  struct Dwarf_Line_s info[0];
};

/* Representation of address ranges.  */
struct Dwarf_Aranges_s
{
  Dwarf *dbg;
  size_t naranges;

  struct Dwarf_Arange_s
  {
    Dwarf_Addr addr;
    Dwarf_Word length;
    Dwarf_Off offset;
  } info[0];
};


/* CU representation.  */
struct Dwarf_CU
{
  Dwarf *dbg;
  Dwarf_Off start;
  Dwarf_Off end;
  uint8_t address_size;
  uint8_t offset_size;
  uint16_t version;

  /* Hash table for the abbreviations.  */
  Dwarf_Abbrev_Hash abbrev_hash;
  /* Offset of the first abbreviation.  */
  size_t orig_abbrev_offset;
  /* Offset past last read abbreviation.  */
  size_t last_abbrev_offset;

  /* The srcline information.  */
  Dwarf_Lines *lines;

  /* The source file information.  */
  Dwarf_Files *files;

  /* Known location lists.  */
  void *locs;
};

/* Compute the offset of a CU's first DIE from its offset.  This
   is either:
        LEN       VER     OFFSET    ADDR
      4-bytes + 2-bytes + 4-bytes + 1-byte  for 32-bit dwarf
     12-bytes + 2-bytes + 8-bytes + 1-byte  for 64-bit dwarf

   Note the trick in the computation.  If the offset_size is 4
   the '- 4' term changes the '3 *' into a '2 *'.  If the
   offset_size is 8 it accounts for the 4-byte escape value
   used at the start of the length.  */
#define DIE_OFFSET_FROM_CU_OFFSET(cu_offset, offset_size) \
  ((cu_offset) + 3 * (offset_size) - 4 + 3)

#define CUDIE(fromcu) \
  ((Dwarf_Die)								      \
   {									      \
     .cu = (fromcu),							      \
     .addr = ((char *) (fromcu)->dbg->sectiondata[IDX_debug_info]->d_buf      \
	      + (fromcu)->start + 3 * (fromcu)->offset_size - 4 + 3),	      \
   })


/* Macro information.  */
struct Dwarf_Macro_s
{
  unsigned int opcode;
  Dwarf_Word param1;
  union
  {
    Dwarf_Word u;
    const char *s;
  } param2;
};


/* We have to include the file at this point because the inline
   functions access internals of the Dwarf structure.  */
#include "memory-access.h"


/* Set error value.  */
extern void __libdw_seterrno (int value) internal_function;


/* Memory handling, the easy parts.  This macro does not do any locking.  */
#define libdw_alloc(dbg, type, tsize, cnt) \
  ({ struct libdw_memblock *_tail = (dbg)->mem_tail;			      \
     size_t _required = (tsize) * (cnt);				      \
     type *_result = (type *) (_tail->mem + (_tail->size - _tail->remaining));\
     size_t _padding = ((__alignof (type)				      \
			 - ((uintptr_t) _result & (__alignof (type) - 1)))    \
			& (__alignof (type) - 1));			      \
     if (unlikely (_tail->remaining < _required + _padding))		      \
       _result = (type *) __libdw_allocate (dbg, _required, __alignof (type));\
     else								      \
       {								      \
	 _required += _padding;						      \
	 _result = (type *) ((char *) _result + _padding);		      \
	 _tail->remaining -= _required;					      \
       }								      \
     _result; })

#define libdw_typed_alloc(dbg, type) \
  libdw_alloc (dbg, type, sizeof (type), 1)

/* Callback to allocate more.  */
extern void *__libdw_allocate (Dwarf *dbg, size_t minsize, size_t align)
     __attribute__ ((__malloc__)) __nonnull_attribute__ (1);

/* Default OOM handler.  */
extern void __libdw_oom (void) __attribute ((noreturn, visibility ("hidden")));

/* Find CU for given offset.  */
extern struct Dwarf_CU *__libdw_findcu (Dwarf *dbg, Dwarf_Off offset)
     __nonnull_attribute__ (1) internal_function;

/* Return tag of given DIE.  */
extern Dwarf_Abbrev *__libdw_findabbrev (struct Dwarf_CU *cu,
					 unsigned int code)
     __nonnull_attribute__ (1) internal_function;

/* Get abbreviation at given offset.  */
extern Dwarf_Abbrev *__libdw_getabbrev (Dwarf *dbg, struct Dwarf_CU *cu,
					Dwarf_Off offset, size_t *lengthp,
					Dwarf_Abbrev *result)
     __nonnull_attribute__ (1) internal_function;

/* Helper functions for form handling.  */
extern size_t __libdw_form_val_len (Dwarf *dbg, struct Dwarf_CU *cu,
				    unsigned int form,
				    const unsigned char *valp)
     __nonnull_attribute__ (1, 2, 4) internal_function;

/* Helper function for DW_FORM_ref* handling.  */
extern int __libdw_formref (Dwarf_Attribute *attr, Dwarf_Off *return_offset)
     __nonnull_attribute__ (1, 2) internal_function;


/* Helper function to locate attribute.  */
extern unsigned char *__libdw_find_attr (Dwarf_Die *die,
					 unsigned int search_name,
					 unsigned int *codep,
					 unsigned int *formp)
     __nonnull_attribute__ (1) internal_function;

/* Helper function to access integer attribute.  */
extern int __libdw_attr_intval (Dwarf_Die *die, int *valp, int attval)
     __nonnull_attribute__ (1, 2) internal_function;

/* Helper function to walk scopes.  */
struct Dwarf_Die_Chain
{
  Dwarf_Die die;
  struct Dwarf_Die_Chain *parent;
  bool prune;			/* The PREVISIT function can set this.  */
};
extern int __libdw_visit_scopes (unsigned int depth,
				 struct Dwarf_Die_Chain *root,
				 int (*previsit) (unsigned int depth,
						  struct Dwarf_Die_Chain *,
						  void *arg),
				 int (*postvisit) (unsigned int depth,
						   struct Dwarf_Die_Chain *,
						   void *arg),
				 void *arg)
  __nonnull_attribute__ (2, 3) internal_function;

/* Parse a DWARF Dwarf_Block into an array of Dwarf_Op's,
   and cache the result (via tsearch).  */
extern int __libdw_intern_expression (Dwarf *dbg,
				      bool other_byte_order,
				      unsigned int address_size,
				      void **cache, const Dwarf_Block *block,
				      bool valuep,
				      Dwarf_Op **llbuf, size_t *listlen,
				      int sec_index)
  __nonnull_attribute__ (4, 5, 7, 8) internal_function;


/* Return error code of last failing function call.  This value is kept
   separately for each thread.  */
extern int __dwarf_errno_internal (void);


/* Reader hooks.  */

/* Relocation hooks return -1 on error (in that case the error code
   must already have been set), 0 if there is no relocation and 1 if a
   relocation was present.*/

static inline int
__libdw_relocate_address (Dwarf *dbg __attribute__ ((unused)),
			  int sec_index __attribute__ ((unused)),
			  const void *addr __attribute__ ((unused)),
			  int width __attribute__ ((unused)),
			  Dwarf_Addr *val __attribute__ ((unused)))
{
  return 0;
}

static inline int
__libdw_relocate_offset (Dwarf *dbg __attribute__ ((unused)),
			 int sec_index __attribute__ ((unused)),
			 const void *addr __attribute__ ((unused)),
			 int width __attribute__ ((unused)),
			 Dwarf_Off *val __attribute__ ((unused)))
{
  return 0;
}

static inline Elf_Data *
__libdw_checked_get_data (Dwarf *dbg, int sec_index)
{
  Elf_Data *data = dbg->sectiondata[sec_index];
  if (unlikely (data == NULL)
      || unlikely (data->d_buf == NULL))
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }
  return data;
}

static inline int
__libdw_offset_in_section (Dwarf *dbg, int sec_index,
			   Dwarf_Off offset, size_t size)
{
  Elf_Data *data = __libdw_checked_get_data (dbg, sec_index);
  if (data == NULL)
    return -1;
  if (unlikely (offset > data->d_size)
      || unlikely (data->d_size - offset < size))
    {
      __libdw_seterrno (DWARF_E_INVALID_OFFSET);
      return -1;
    }

  return 0;
}

static inline bool
__libdw_in_section (Dwarf *dbg, int sec_index,
		    const void *addr, size_t size)
{
  Elf_Data *data = __libdw_checked_get_data (dbg, sec_index);
  if (data == NULL)
    return false;
  if (unlikely (addr < data->d_buf)
      || unlikely (data->d_size - (addr - data->d_buf) < size))
    {
      __libdw_seterrno (DWARF_E_INVALID_OFFSET);
      return false;
    }

  return true;
}

#define READ_AND_RELOCATE(RELOC_HOOK, VAL)				\
  ({									\
    if (!__libdw_in_section (dbg, sec_index, addr, width))		\
      return -1;							\
									\
    const unsigned char *orig_addr = addr;				\
    if (width == 4)							\
      VAL = read_4ubyte_unaligned_inc (dbg, addr);			\
    else								\
      VAL = read_8ubyte_unaligned_inc (dbg, addr);			\
									\
    int status = RELOC_HOOK (dbg, sec_index, orig_addr, width, &VAL);	\
    if (status < 0)							\
      return status;							\
    status > 0;								\
   })

static inline int
__libdw_read_address_inc (Dwarf *dbg,
			  int sec_index, const unsigned char **addrp,
			  int width, Dwarf_Addr *ret)
{
  const unsigned char *addr = *addrp;
  READ_AND_RELOCATE (__libdw_relocate_address, (*ret));
  *addrp = addr;
  return 0;
}

static inline int
__libdw_read_address (Dwarf *dbg,
		      int sec_index, const unsigned char *addr,
		      int width, Dwarf_Addr *ret)
{
  READ_AND_RELOCATE (__libdw_relocate_address, (*ret));
  return 0;
}

static inline int
__libdw_read_offset_inc (Dwarf *dbg,
			 int sec_index, const unsigned char **addrp,
			 int width, Dwarf_Off *ret, int sec_ret,
			 size_t size)
{
  const unsigned char *addr = *addrp;
  READ_AND_RELOCATE (__libdw_relocate_offset, (*ret));
  *addrp = addr;
  return __libdw_offset_in_section (dbg, sec_ret, *ret, size);
}

static inline int
__libdw_read_offset (Dwarf *dbg,
		     int sec_index, const unsigned char *addr,
		     int width, Dwarf_Off *ret, int sec_ret,
		     size_t size)
{
  READ_AND_RELOCATE (__libdw_relocate_offset, (*ret));
  return __libdw_offset_in_section (dbg, sec_ret, *ret, size);
}

/* Read up begin/end pair and increment read pointer.
    - If it's normal range record, set up *BEGINP and *ENDP and return 0.
    - If it's base address selection record, set up *BASEP and return 1.
    - If it's end of rangelist, don't set anything and return 2
    - If an error occurs, don't set anything and return <0.  */
int __libdw_read_begin_end_pair_inc (Dwarf *dbg, int sec_index,
				     unsigned char **addr, int width,
				     Dwarf_Addr *beginp, Dwarf_Addr *endp,
				     Dwarf_Addr *basep)
  internal_function;

unsigned char * __libdw_formptr (Dwarf_Attribute *attr, int sec_index,
				 int err_nodata, unsigned char **endpp,
				 Dwarf_Off *offsetp)
  internal_function;



/* Aliases to avoid PLTs.  */
INTDECL (dwarf_attr)
INTDECL (dwarf_attr_integrate)
INTDECL (dwarf_begin_elf)
INTDECL (dwarf_child)
INTDECL (dwarf_dieoffset)
INTDECL (dwarf_diename)
INTDECL (dwarf_end)
INTDECL (dwarf_entrypc)
INTDECL (dwarf_errmsg)
INTDECL (dwarf_formaddr)
INTDECL (dwarf_formblock)
INTDECL (dwarf_formref_die)
INTDECL (dwarf_formsdata)
INTDECL (dwarf_formstring)
INTDECL (dwarf_formudata)
INTDECL (dwarf_getarange_addr)
INTDECL (dwarf_getarangeinfo)
INTDECL (dwarf_getaranges)
INTDECL (dwarf_getsrcfiles)
INTDECL (dwarf_getsrclines)
INTDECL (dwarf_hasattr)
INTDECL (dwarf_haschildren)
INTDECL (dwarf_haspc)
INTDECL (dwarf_highpc)
INTDECL (dwarf_lowpc)
INTDECL (dwarf_nextcu)
INTDECL (dwarf_offdie)
INTDECL (dwarf_ranges)
INTDECL (dwarf_siblingof)
INTDECL (dwarf_tag)

#endif	/* libdwP.h */
