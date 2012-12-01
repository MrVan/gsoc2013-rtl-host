/*
 * Copyright (c) 2012, Chris Johns <chrisj@rtems.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * @file
 *
 * @ingroup rtems_ld
 *
 * @brief RTEMS Linker.
 *
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <algorithm>
#include <list>
#include <iomanip>

#include <rld.h>
#include <rld-compression.h>
#include <rld-rap.h>

namespace rld
{
  namespace rap
  {
    /**
     * The sections of interest in a RAP file.
     */
    enum sections
    {
      rap_text = 0,
      rap_const = 1,
      rap_ctor = 2,
      rap_dtor = 3,
      rap_data = 4,
      rap_bss = 5,
      rap_secs = 6
    };

    /**
     * The names of the RAP sections.
     */
    static const char* section_names[rap_secs] =
    {
      ".text",
      ".const",
      ".ctor",
      ".dtor",
      ".data",
      ".bss"
    };

    /**
     * The RAP section data.
     */
    struct section
    {
      std::string name;   //< The name of the section.
      uint32_t    size;   //< The size of the section.
      uint32_t    offset; //< The offset of the section.
      uint32_t    align;  //< The alignment of the section.

      /**
       * Operator to add up section data.
       */
      section& operator += (const section& sec);

      /**
       * Default constructor.
       */
      section ();
    };

    /**
     * A symbol. This matches the symbol structure 'rtems_rtl_obj_sym_t' in the
     * target code.
     */
    struct external
    {
      /**
       * Size of an external in the RAP file.
       */
      static const uint32_t rap_size = sizeof (uint32_t) * 3;

      const uint32_t name;  //< The string table's name index.
      const sections sec;   //< The section the symbols belongs to.
      const uint32_t value; //< The offset from the section base.
      const uint32_t data;  //< The ELF st.info field.

      /**
       * The default constructor.
       */
      external (const uint32_t name,
                const sections sec,
                const uint32_t value,
                const uint32_t info);

      /**
       * Copy constructor.
       */
      external (const external& orig);

    };

    /**
     * A container of externals.
     */
    typedef std::list < external > externals;

    /**
     * The specific data for each object we need to collect to create the RAP
     * format file.
     */
    struct object
    {

      files::object&  obj;            //< The object file.
      files::sections text;           //< All executable code.
      files::sections const_;         //< All read only data.
      files::sections ctor;           //< The static constructor table.
      files::sections dtor;           //< The static destructor table.
      files::sections data;           //< All initialised read/write data.
      files::sections bss;            //< All uninitialised read/write data
      files::sections relocs;         //< All relocation records.
      files::sections symtab;         //< All exported symbols.
      files::sections strtab;         //< All exported strings.
      section         secs[rap_secs]; //< The sections of interest.
      uint32_t        symtab_off;     //< The symbols section file offset.
      uint32_t        symtab_size;    //< The symbols section size.
      uint32_t        strtab_off;     //< The strings section file offset.
      uint32_t        strtab_size;    //< The strings section size.
      uint32_t        relocs_off;     //< The reloc's section file offset.
      uint32_t        relocs_size;    //< The reloc's section size.

      /**
       * The constructor. Need to have an object file to create.
       */
      object (files::object& obj);

      /**
       * The copy constructor.
       */
      object (const object& orig);

      /**
       * Find the section type that matches the section index.
       */
      sections find (const uint32_t index) const;

    private:
      /**
       * No default constructor allowed.
       */
      object ();
    };

    /**
     * A container of objects.
     */
    typedef std::list < object > objects;

    /**
     * The RAP image.
     */
    class image
    {
    public:
      /**
       * Construct the image.
       */
      image ();

      /**
       * Load the layout data from the object files.
       */
      void layout (const files::object_list& app_objects);

      /**
       * Write the compressed output file.
       */
      void write (compress::compressor& comp,
                  const std::string&    init,
                  const std::string&    fini);

      /**
       * Write the sections to the compressed output file.
       */
      void write (compress::compressor&  comp,
                  files::object&         obj,
                  const files::sections& secs);

    private:

      objects     objs;           //< The RAP objects
      section     secs[rap_secs]; //< The sections of interest.
      externals   externs;        //< The symbols in the image
      uint32_t    symtab_size;    //< The size of the symbols.
      std::string strtab;         //< The strings table.
      uint32_t    relocs_size;    //< The relocations size.
    };

    /**
     * Output helper function to report the sections in an object file.
     * This is useful when seeing the flags in the sections.
     *
     * @param name The name of the section group in the RAP file.
     * @param size The total of the section size's in the group.
     * @param secs The container of sections in the group.
     */
    void
    output (const char* name, size_t size, const files::sections& secs)
    {
      if (size)
      {
        std::cout << ' ' << name << ": size: " << size << std::endl;

        for (files::sections::const_iterator si = secs.begin ();
             si != secs.end ();
             ++si)
        {
          files::section sec = *si;

          if (sec.size)
          {
            #define SF(f, i, c) if (sec.flags & (f)) flags[i] = c

            std::string flags ("--------------");

            SF (SHF_WRITE,            0, 'W');
            SF (SHF_ALLOC,            1, 'A');
            SF (SHF_EXECINSTR,        2, 'E');
            SF (SHF_MERGE,            3, 'M');
            SF (SHF_STRINGS,          4, 'S');
            SF (SHF_INFO_LINK,        5, 'I');
            SF (SHF_LINK_ORDER,       6, 'L');
            SF (SHF_OS_NONCONFORMING, 7, 'N');
            SF (SHF_GROUP,            8, 'G');
            SF (SHF_TLS,              9, 'T');
            SF (SHF_AMD64_LARGE,     10, 'a');
            SF (SHF_ENTRYSECT,       11, 'e');
            SF (SHF_COMDEF,          12, 'c');
            SF (SHF_ORDERED,         13, 'O');

            std::cout << "  " << std::left
                      << std::setw (15) << sec.name
                      << " " << flags
                      << " size: " << std::setw (5) << sec.size
                      << " align: " << sec.alignment
                      << std::right << std::endl;
          }
        }
      }
    }

    section::section ()
      : size (0),
        offset (0),
        align (0)
    {
    }

    section&
    section::operator += (const section& sec)
    {
      if (sec.size)
      {
        if (align == 0)
          align = sec.align;
        else if (align != sec.align)
          throw rld::error ("Alignments do not match for section '" + name + "'",
                            "rap::section");

        if (size && (align == 0))
          throw rld::error ("Invalid alignment '" + name + "'",
                            "rap::section");

        size += sec.size;
        offset = sec.offset + sec.size;

        uint32_t mask = (1 << (align - 1)) - 1;

        if (offset & mask)
        {
          offset &= ~mask;
          offset += (1 << align);
        }
      }

      return *this;
    }

    external::external (const uint32_t name,
                        const sections sec,
                        const uint32_t value,
                        const uint32_t data)
      : name (name),
        sec (sec),
        value (value),
        data (data)
    {
    }

    external::external (const external& orig)
      : name (orig.name),
        sec (orig.sec),
        value (orig.value),
        data (orig.data)
    {
    }

    object::object (files::object& obj)
      : obj (obj),
        symtab_off (0),
        symtab_size (0),
        strtab_off (0),
        strtab_size (0),
        relocs_off (0),
        relocs_size (0)
    {
      /*
       * Set up the names of the sections.
       */
      for (int s = 0; s < rap_secs; ++s)
        secs[s].name = section_names[s];

      /*
       * Get from the object file the various sections we need to format a
       * memory layout.
       */

      obj.get_sections (text,   SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR);
      obj.get_sections (const_, SHT_PROGBITS, SHF_ALLOC | SHF_MERGE, SHF_WRITE | SHF_EXECINSTR);
      obj.get_sections (ctor,   ".ctors");
      obj.get_sections (dtor,   ".dtors");
      obj.get_sections (data,   SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
      obj.get_sections (bss,    SHT_NOBITS,   SHF_ALLOC | SHF_WRITE);
      obj.get_sections (symtab, SHT_SYMTAB);
      obj.get_sections (strtab, ".strtab");

      /*
       * Only interested in the relocation records for the text sections.
       */
      for (files::sections::const_iterator ti = text.begin ();
           ti != text.end ();
           ++ti)
      {
        files::section sec = *ti;
        obj.get_sections (relocs, ".rel" + sec.name);
        obj.get_sections (relocs, ".rela" + sec.name);
      }

      secs[rap_text].size = files::sum_sizes (text);
      if (!text.empty ())
        secs[rap_text].align = (*text.begin ()).alignment;

      secs[rap_const].size = files::sum_sizes (const_);
      if (!const_.empty ())
        secs[rap_const].align = (*const_.begin ()).alignment;

      secs[rap_ctor].size = files::sum_sizes (ctor);
      if (!ctor.empty ())
        secs[rap_ctor].align = (*ctor.begin ()).alignment;

      secs[rap_dtor].size = files::sum_sizes (dtor);
      if (!dtor.empty ())
        secs[rap_dtor].align = (*dtor.begin ()).alignment;

      secs[rap_data].size = files::sum_sizes (data);
      if (!data.empty ())
        secs[rap_data].align = (*data.begin ()).alignment;

      secs[rap_bss].size = files::sum_sizes (bss);
      if (!bss.empty ())
        secs[rap_bss].align = (*bss.begin ()).alignment;

      symtab_size = files::sum_sizes (symtab);
      strtab_size = files::sum_sizes (strtab);
      relocs_size = files::sum_sizes (relocs);

      if (rld::verbose () >= RLD_VERBOSE_TRACE)
      {
        std::cout << "rap:object: " << obj.name ().full () << std::endl;
        output ("text", secs[rap_text].size, text);
        output ("const", secs[rap_const].size, const_);
        output ("ctor", secs[rap_ctor].size, ctor);
        output ("dtor", secs[rap_dtor].size, dtor);
        output ("data", secs[rap_data].size, data);
        if (secs[rap_bss].size)
          std::cout << " bss: size: " << secs[rap_bss].size << std::endl;
        output ("relocs", relocs_size, relocs);
        output ("symtab", symtab_size, symtab);
        output ("strtab", strtab_size, strtab);
      }
    }

    object::object (const object& orig)
      : obj (orig.obj),
        text (orig.text),
        const_ (orig.const_),
        ctor (orig.ctor),
        dtor (orig.dtor),
        data (orig.data),
        bss (orig.bss),
        relocs (orig.relocs),
        symtab (orig.symtab),
        strtab (orig.strtab),
        symtab_off (orig.symtab_off),
        symtab_size (orig.symtab_size),
        strtab_off (orig.strtab_off),
        strtab_size (orig.strtab_size),
        relocs_off (orig.relocs_off),
        relocs_size (orig.relocs_size)
    {
      for (int s = 0; s < rap_secs; ++s)
        secs[s] = orig.secs[s];
    }

    sections
    object::find (const uint32_t index) const
    {
      const files::section* sec;

      sec = files::find (text, index);
      if (sec)
        return rap_text;

      sec = files::find (const_, index);
      if (sec)
        return rap_const;

      sec = files::find (ctor, index);
      if (sec)
        return rap_ctor;

      sec = files::find (dtor, index);
      if (sec)
        return rap_dtor;

      sec = files::find (data, index);
      if (sec)
        return rap_data;

      sec = files::find (bss, index);
      if (sec)
        return rap_bss;

      throw rld::error ("Section index not found: " + obj.name ().full (),
                        "rap::object");
    }

    image::image ()
      : symtab_size (0),
        relocs_size (0)
    {
      /*
       * Set up the names of the sections.
       */
      for (int s = 0; s < rap_secs; ++s)
        secs[s].name = section_names[s];
    }

    void
    image::layout (const files::object_list& app_objects)
    {
      /*
       * Create the local objects which contain the layout information.
       */
      for (files::object_list::const_iterator aoi = app_objects.begin ();
           aoi != app_objects.end ();
           ++aoi)
      {
        files::object& app_obj = *(*aoi);

        if (!app_obj.valid ())
          throw rld::error ("Not valid: " + app_obj.name ().full (),
                            "rap::layout");

        objs.push_back (object (app_obj));
      }

      for (int s = 0; s < rap_secs; ++s)
      {
        secs[s].size = 0;
        secs[s].offset = 0;
        secs[s].align = 0;
      }

      for (objects::iterator oi = objs.begin ();
           oi != objs.end ();
           ++oi)
      {
        object& obj = *oi;

        secs[rap_text] += obj.secs[rap_text];
        secs[rap_const] += obj.secs[rap_const];
        secs[rap_ctor] += obj.secs[rap_ctor];
        secs[rap_dtor] += obj.secs[rap_dtor];
        secs[rap_data] += obj.secs[rap_data];
        secs[rap_bss] += obj.secs[rap_bss];

        symtab_size = 0;
        strtab.clear ();

        uint32_t sym_count = 0;

        symbols::pointers& esyms = obj.obj.external_symbols ();
        for (symbols::pointers::const_iterator ei = esyms.begin ();
             ei != esyms.end ();
             ++ei, ++sym_count)
        {
          const symbols::symbol& sym = *(*ei);

          if ((sym.type () == STT_OBJECT) || (sym.type () == STT_FUNC))
          {
            if ((sym.binding () == STB_GLOBAL) || (sym.binding () == STB_WEAK))
            {
              externs.push_back (external (strtab.size () + 2,
                                           obj.find (sym.index ()),
                                           sym.value (),
                                           sym.info ()));
              symtab_size += external::rap_size;
              strtab += sym.name ();
              strtab += '\0';
            }
          }
        }

        relocs_size += obj.relocs_size;
      }

      if (rld::verbose () >= RLD_VERBOSE_INFO)
      {
        uint32_t total = (secs[rap_text].size + secs[rap_data].size +
                          secs[rap_data].size + secs[rap_bss].size +
                          symtab_size + strtab.size() + relocs_size);
        std::cout << "rap::layout: total:" << total
                  << " text:" << secs[rap_text].size
                  << " const:" << secs[rap_const].size
                  << " ctor:" << secs[rap_ctor].size
                  << " dtor:" << secs[rap_dtor].size
                  << " data:" << secs[rap_data].size
                  << " bss:" << secs[rap_bss].size
                  << " symbols:" << symtab_size << " (" << externs.size () << ')'
                  << " strings:" << strtab.size ()
                  << " relocs:" << relocs_size
                  << std::endl;
      }
    }

    /**
     * Helper for for_each to write out the various sections.
     */
    class section_writer:
      public std::unary_function < object, void >
    {
    public:

      section_writer (image&                img,
                      compress::compressor& comp,
                      sections              sec);

      void operator () (object& obj);

    private:

      image&                img;
      compress::compressor& comp;
      sections              sec;
    };

    section_writer::section_writer (image&                img,
                                    compress::compressor& comp,
                                    sections              sec)
      : img (img),
        comp (comp),
        sec (sec)
    {
    }

    void
    section_writer::operator () (object& obj)
    {
      switch (sec)
      {
        case rap_text:
          img.write (comp, obj.obj, obj.text);
          break;
        case rap_const:
          img.write (comp, obj.obj, obj.const_);
          break;
        case rap_ctor:
          img.write (comp, obj.obj, obj.ctor);
          break;
        case rap_dtor:
          img.write (comp, obj.obj, obj.dtor);
          break;
        case rap_data:
          img.write (comp, obj.obj, obj.data);
          break;
          default:
            break;
      }
    }

    void
    image::write (compress::compressor& comp,
                  const std::string&    init,
                  const std::string&    fini)
    {
      /*
       * Start with the machine type so the target can check the applicatiion
       * is ok and can be loaded. Add the init and fini labels to the string
       * table and add the references to the string table next. Follow this
       * with the section details then the string table and symbol table then
       * finally the relocation records.
       */

      comp << elf::object_machine_type ()
           << elf::object_datatype ()
           << elf::object_class ();

      comp << (uint32_t) strtab.size ();
      strtab += init;
      strtab += '\0';

      comp << (uint32_t) strtab.size ();
      strtab += fini;
      strtab += '\0';

      comp << symtab_size
           << (uint32_t) strtab.size ()
           << (uint32_t) 0;

      for (int s = 0; s < rap_secs; ++s)
        comp << secs[s].size
             << secs[s].align
             << secs[s].offset;

      /*
       * Output the sections from each object file.
       */

      std::for_each (objs.begin (), objs.end (),
                     section_writer (*this, comp, rap_text));
      std::for_each (objs.begin (), objs.end (),
                     section_writer (*this, comp, rap_const));
      std::for_each (objs.begin (), objs.end (),
                     section_writer (*this, comp, rap_ctor));
      std::for_each (objs.begin (), objs.end (),
                     section_writer (*this, comp, rap_dtor));
      std::for_each (objs.begin (), objs.end (),
                     section_writer (*this, comp, rap_data));

      comp << strtab;

      for (externals::const_iterator ei = externs.begin ();
           ei != externs.end ();
           ++ei)
      {
        const external& ext = *ei;
        comp << (uint32_t) ((ext.sec << 16) | ext.data)
             << ext.name
             << ext.value;
      }
    }

    void
    image::write (compress::compressor&  comp,
                  files::object&         obj,
                  const files::sections& secs)
    {
      obj.open ();

      try
      {
        obj.begin ();
        for (files::sections::const_iterator si = secs.begin ();
             si != secs.end ();
             ++si)
        {
          const files::section& sec = *si;
          comp.write (obj, sec.offset, sec.size);
        }

        obj.end ();
      }
      catch (...)
      {
        obj.close ();
        throw;
      }

      obj.close ();
    }

    void
    write (files::image&             app,
           const std::string&        init,
           const std::string&        fini,
           const files::object_list& app_objects,
           const symbols::table&     /* symbols */) /* Add back for incremental
                                                     * linking */
    {
      compress::compressor compressor (app, 2 * 1024);
      image                rap;

      rap.layout (app_objects);
      rap.write (compressor, init, fini);

      compressor.flush ();

      if (rld::verbose () >= RLD_VERBOSE_INFO)
      {
        int pcent = (compressor.compressed () * 100) / compressor.transferred ();
        int premand = (((compressor.compressed () * 1000) + 500) /
                       compressor.transferred ()) % 10;
        std::cout << "rap: objects: " << app_objects.size ()
                  << ", size: " << compressor.compressed ()
                  << ", compression: " << pcent << '.' << premand << '%'
                  << std::endl;
      }
    }

  }
}
