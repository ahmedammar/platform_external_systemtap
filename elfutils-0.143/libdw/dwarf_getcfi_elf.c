/* Get CFI from ELF file's exception-handling info.
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "libdwP.h"
#include "cfi.h"
#include "encoded-value.h"
#include <dwarf.h>


static Dwarf_CFI *
allocate_cfi (Elf *elf, GElf_Addr vaddr)
{
  Dwarf_CFI *cfi = calloc (1, sizeof *cfi);
  if (cfi == NULL)
    {
      __libdw_seterrno (DWARF_E_NOMEM);
      return NULL;
    }

  cfi->e_ident = (unsigned char *) elf_getident (elf, NULL);
  if (cfi->e_ident == NULL)
    {
      free (cfi);
      __libdw_seterrno (DWARF_E_GETEHDR_ERROR);
      return NULL;
    }

  if ((BYTE_ORDER == LITTLE_ENDIAN && cfi->e_ident[EI_DATA] == ELFDATA2MSB)
      || (BYTE_ORDER == BIG_ENDIAN && cfi->e_ident[EI_DATA] == ELFDATA2LSB))
    cfi->other_byte_order = true;

  cfi->frame_vaddr = vaddr;
  cfi->textrel = 0;		/* XXX ? */
  cfi->datarel = 0;		/* XXX ? */

  return cfi;
}

static const uint8_t *
parse_eh_frame_hdr (const uint8_t *hdr, size_t hdr_size, GElf_Addr hdr_vaddr,
		    const GElf_Ehdr *ehdr, GElf_Addr *eh_frame_vaddr,
		    size_t *table_entries, uint8_t *table_encoding)
{
  const uint8_t *h = hdr;

  if (*h++ != 1)		/* version */
    return (void *) -1l;

  uint8_t eh_frame_ptr_encoding = *h++;
  uint8_t fde_count_encoding = *h++;
  uint8_t fde_table_encoding = *h++;

  if (eh_frame_ptr_encoding == DW_EH_PE_omit)
    return (void *) -1l;

  /* Dummy used by read_encoded_value.  */
  Elf_Data_Scn dummy_cfi_hdr_data =
    {
      .d = { .d_buf = (void *) hdr, .d_size = hdr_size }
    };
  Dwarf_CFI dummy_cfi =
    {
      .e_ident = ehdr->e_ident,
      .datarel = hdr_vaddr,
      .frame_vaddr = hdr_vaddr,
      .data = &dummy_cfi_hdr_data,
    };

  if (unlikely (read_encoded_value (&dummy_cfi, eh_frame_ptr_encoding, &h,
				    eh_frame_vaddr)))
    return (void *) -1l;

  if (fde_count_encoding != DW_EH_PE_omit)
    {
      Dwarf_Word fde_count;
      if (unlikely (read_encoded_value (&dummy_cfi, fde_count_encoding, &h,
					&fde_count)))
	return (void *) -1l;
      if (fde_count != 0 && (size_t) fde_count == fde_count
	  && fde_table_encoding != DW_EH_PE_omit
	  && (fde_table_encoding &~ DW_EH_PE_signed) != DW_EH_PE_uleb128)
	{
	  *table_entries = fde_count;
	  *table_encoding = fde_table_encoding;
	  return h;
	}
    }

  return NULL;
}

static Dwarf_CFI *
getcfi_gnu_eh_frame (Elf *elf, const GElf_Ehdr *ehdr, const GElf_Phdr *phdr)
{
  if (unlikely (phdr->p_filesz < 4))
    goto invalid;

  Elf_Data *data = elf_getdata_rawchunk (elf, phdr->p_offset, phdr->p_filesz,
					 ELF_T_BYTE);
  if (data == NULL)
    {
    invalid_hdr:
    invalid:
      /* XXX might be read error or corrupt phdr */
      __libdw_seterrno (DWARF_E_INVALID_CFI);
      return NULL;
    }

  Dwarf_Addr eh_frame_ptr;
  size_t search_table_entries;
  uint8_t search_table_encoding;
  const uint8_t *search_table = parse_eh_frame_hdr (data->d_buf, phdr->p_filesz,
						    phdr->p_vaddr, ehdr,
						    &eh_frame_ptr,
						    &search_table_entries,
						    &search_table_encoding);
  if (search_table == (void *) -1l)
    goto invalid_hdr;

  Dwarf_Off eh_frame_offset = eh_frame_ptr - phdr->p_vaddr + phdr->p_offset;
  Dwarf_Word eh_frame_size = 0;

  /* XXX we have no way without section headers to know the size
     of the .eh_frame data.  Calculate the largest it might possibly be.
     This won't be wasteful if the file is already mmap'd, but if it isn't
     it might be quite excessive.  */
  size_t filesize;
  if (elf_rawfile (elf, &filesize) != NULL)
    eh_frame_size = filesize - eh_frame_offset;

  data = elf_getdata_rawchunk (elf, eh_frame_offset, eh_frame_size, ELF_T_BYTE);
  if (data == NULL)
    {
      __libdw_seterrno (DWARF_E_INVALID_ELF); /* XXX might be read error */
      return NULL;
    }
  Dwarf_CFI *cfi = allocate_cfi (elf, eh_frame_ptr);
  if (cfi != NULL)
    {
      cfi->data = (Elf_Data_Scn *) data;

      if (search_table != NULL)
	{
	  cfi->search_table = search_table;
	  cfi->search_table_vaddr = phdr->p_vaddr;
	  cfi->search_table_encoding = search_table_encoding;
	  cfi->search_table_entries = search_table_entries;
	}
    }
  return cfi;
}

/* Search the phdrs for PT_GNU_EH_FRAME.  */
static Dwarf_CFI *
getcfi_phdr (Elf *elf, const GElf_Ehdr *ehdr)
{
  const uint_fast16_t phnum = ehdr->e_phnum;

  for (uint_fast16_t i = 0; i < phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (elf, i, &phdr_mem);
      if (unlikely (phdr == NULL))
	return NULL;
      if (phdr->p_type == PT_GNU_EH_FRAME)
	return getcfi_gnu_eh_frame (elf, ehdr, phdr);
    }

  __libdw_seterrno (DWARF_E_NO_DWARF);
  return NULL;
}

static Dwarf_CFI *
getcfi_scn_eh_frame (Elf *elf, const GElf_Ehdr *ehdr,
		     Elf_Scn *scn, GElf_Shdr *shdr,
		     Elf_Scn *hdr_scn, GElf_Addr hdr_vaddr)
{
  Elf_Data *data = elf_rawdata (scn, NULL);
  if (data == NULL)
    {
      __libdw_seterrno (DWARF_E_INVALID_ELF);
      return NULL;
    }
  Dwarf_CFI *cfi = allocate_cfi (elf, shdr->sh_addr);
  if (cfi != NULL)
    {
      cfi->data = (Elf_Data_Scn *) data;
      if (hdr_scn != NULL)
	{
	  Elf_Data *hdr_data = elf_rawdata (hdr_scn, NULL);
	  if (hdr_data != NULL)
	    {
	      GElf_Addr eh_frame_vaddr;
	      cfi->search_table_vaddr = hdr_vaddr;
	      cfi->search_table
		= parse_eh_frame_hdr (hdr_data->d_buf, hdr_data->d_size,
				      hdr_vaddr, ehdr, &eh_frame_vaddr,
				      &cfi->search_table_entries,
				      &cfi->search_table_encoding);
	      if (cfi->search_table == (void *) -1l)
		{
		  free (cfi);
		  /* XXX might be read error or corrupt phdr */
		  __libdw_seterrno (DWARF_E_INVALID_CFI);
		  return NULL;
		}

	      /* Sanity check.  */
	      if (unlikely (eh_frame_vaddr != shdr->sh_addr))
		cfi->search_table = NULL;
	    }
	}
    }
  return cfi;
}

/* Search for the sections named ".eh_frame" and ".eh_frame_hdr".  */
static Dwarf_CFI *
getcfi_shdr (Elf *elf, const GElf_Ehdr *ehdr)
{
  size_t shstrndx;
  if (elf_getshdrstrndx (elf, &shstrndx) != 0)
    {
      __libdw_seterrno (DWARF_E_GETEHDR_ERROR);
      return NULL;
    }

  if (shstrndx != 0)
    {
      Elf_Scn *hdr_scn = NULL;
      GElf_Addr hdr_vaddr = 0;
      Elf_Scn *scn = NULL;
      while ((scn = elf_nextscn (elf, scn)) != NULL)
	{
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	  if (shdr == NULL)
	    continue;
	  const char *name = elf_strptr (elf, shstrndx, shdr->sh_name);
	  if (name == NULL)
	    continue;
	  if (!strcmp (name, ".eh_frame_hdr"))
	    {
	      hdr_scn = scn;
	      hdr_vaddr = shdr->sh_addr;
	    }
	  else if (!strcmp (name, ".eh_frame"))
	    return getcfi_scn_eh_frame (elf, ehdr, scn, shdr,
					hdr_scn, hdr_vaddr);
	}
    }

  return (void *) -1l;
}

Dwarf_CFI *
dwarf_getcfi_elf (elf)
     Elf *elf;
{
  if (elf_kind (elf) != ELF_K_ELF)
    {
      __libdw_seterrno (DWARF_E_NOELF);
      return NULL;
    }

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (unlikely (ehdr == NULL))
    {
      __libdw_seterrno (DWARF_E_INVALID_ELF);
      return NULL;
    }

  Dwarf_CFI *result = getcfi_shdr (elf, ehdr);
  if (result == (void *) -1l)
    result = getcfi_phdr (elf, ehdr);

  return result;
}
INTDEF (dwarf_getcfi_elf)
