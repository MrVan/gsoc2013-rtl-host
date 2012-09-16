/*
 * Copyright (c) 2011, Chris Johns <chrisj@rtems.org> 
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <rld.h>

#if __WIN32__
#define CREATE_MODE (S_IRUSR | S_IWUSR)
#define OPEN_FLAGS  (O_BINARY)
#else
#define CREATE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)
#define OPEN_FLAGS  (0)
#endif

namespace rld
{
  namespace files
  {
    /**
     * Scan the decimal number returning the value found.
     */
    uint64_t
    scan_decimal (const uint8_t* string, size_t len)
    {
      uint64_t value = 0;
      
      while (len && (*string != ' '))
      {
        value *= 10;
        value += *string - '0';
        ++string;
        --len;
      }

      return value;
    }

    void
    set_number (uint32_t value, uint8_t* string, size_t len, bool octal = false)
    {
      std::ostringstream oss;
      if (octal)
        oss << std::oct;
      oss << value;
      size_t l = oss.str ().length ();
      if (l > len)
        l = len;
      memcpy (string, oss.str ().c_str (), l);
    }

    std::string
    basename (const std::string& name)
    {
      size_t b = name.find_last_of (RLD_PATH_SEPARATOR);
      if (b != std::string::npos)
        return name.substr (b + 1);
      return name;
    }

    void
    path_split (const std::string& path, rld::files::paths& paths)
    {
      strings ps;
      rld::split (path, ps, RLD_PATHSTR_SEPARATOR);
      if (ps.size ())
      {
        for (strings::iterator psi = ps.begin ();
             psi != ps.end ();
             ++psi)
        {
          if (check_directory (*psi))
            paths.push_back (*psi);
        }
      }
    }

    void
    path_join (const std::string& path_, const std::string& file_, std::string& joined)
    {
      if ((path_[path_.size () - 1] != RLD_PATH_SEPARATOR) &&
          (file_[0] != RLD_PATH_SEPARATOR))
        joined = path_ + RLD_PATH_SEPARATOR + file_;
      else if ((path_[path_.size () - 1] == RLD_PATH_SEPARATOR) &&
               (file_[0] == RLD_PATH_SEPARATOR))
        joined = path_ + &file_[1];        
      else
        joined = path_ + file_;
    }

    bool
    check_file (const std::string& path)
    {
      struct stat sb;
      if (::stat (path.c_str (), &sb) == 0)
        if (S_ISREG (sb.st_mode))
          return true;
      return false;
    }

    bool
    check_directory (const std::string& path)
    {
      struct stat sb;
      if (::stat (path.c_str (), &sb) == 0)
        if (S_ISDIR (sb.st_mode))
          return true;
      return false;
    }

    void
    find_file (std::string& path, const std::string& name, paths& search_paths)
    {
      for (rld::files::paths::iterator pi = search_paths.begin ();
           pi != search_paths.end ();
           ++pi)
      {
        path_join (*pi, name, path);
        if (check_file (path))
          return;
      }
      path.clear ();
    }

    file::file (const std::string& aname,
                const std::string& oname,
                off_t              offset,
                size_t             size)
      : aname_ (aname), 
        oname_ (oname),
        offset_ (offset),
        size_ (size)
    {
    }

    file::file (const std::string& path, bool is_object)
      : offset_ (0),
        size_ (0)
    {
      set (path, is_object);
    }

    file::file ()
      : offset_ (0),
        size_ (0)
    {
    }

    void
    file::set (const std::string& path, bool is_object)
    {
      /*
       * If there is a path look for a colon. If there is no colon we assume
       * it is an object file. If the colon is the last character in the path
       * it is just an archive.
       */
      if (!path.empty ())
      {
        bool get_size = false;
        if (is_object)
        {
          size_t colon = path.find_last_of (':');
          if ((colon != std::string::npos) && (colon > RLD_DRIVE_SEPARATOR))
          {
            aname_ = path.substr (0, colon - 1);
            oname_ = path.substr (colon + 1);
            // @todo Add offset scanning.
          }
          else
          {
            oname_ = path;
            get_size = true;
          }
        }
        else
        {
          aname_ = path;
            get_size = true;
        }

        if (get_size)
        {
          struct stat sb;
          if (::stat (path.c_str (), &sb) == 0)
            size_ = sb.st_size;
        }
      }
    }

    bool
    file::is_archive () const
    {
      return !aname_.empty () && oname_.empty ();
    }

    bool
    file::is_object () const
    {
      return !oname_.empty ();
    }

    bool
    file::is_valid () const
    {
      return !aname_.empty () || ~oname_.empty ();
    }

    bool
    file::exists () const
    {
      /*
       * No name set returns false.
       */
      bool result = false;
      const std::string p = path ();
      if (!p.empty ())
        result = check_file (p);
      return result;
    }

    const std::string
    file::path () const
    {
      if (!aname_.empty ())
        return aname_;
      return oname_;
    }
    
    const std::string 
    file::full () const
    {
      std::string f;
      if (!aname_.empty ())
      {
        f = aname_;
        if (!oname_.empty ())
          f += ':';
      }
      if (!oname_.empty ())
        f += oname_;
      if (!aname_.empty () && !oname_.empty ())
        f += '@' + rld::to_string (offset_);
      return f;
    }

    const std::string
    file::basename () const
    {
      return rld::files::basename (full ());
    }

    const std::string&
    file::aname () const
    {
      return aname_;
    }

    const std::string&
    file::oname () const
    {
      return oname_;
    }

    off_t
    file::offset () const
    {
       return offset_;
    }

    size_t
    file::size () const
    {
       return size_;
    }

    image::image (file& name)
      : name_ (name),
        references_ (0),
        fd_ (-1),
        elf_ (0)
    {
    }

    image::image (const std::string& path, bool is_object)
      : name_ (path, is_object),
        references_ (0),
        fd_ (-1),
        elf_ (0),
        symbol_refs (0)
    {
    }

    image::image ()
      : references_ (0),
        fd_ (-1),
        elf_ (0),
        symbol_refs (0)
    {
    }

    image::~image ()
    {
      if (references_)
        throw rld_error_at ("references when destructing image");
      if (fd_ >= 0)
        ::close (fd_);
    }

    void
    image::open (file& name)
    {
      name_ = name;
      open ();
    }

    void
    image::open (bool writable)
    {
      const std::string path = name_.path ();

      if (path.empty ())
        throw rld::error ("No file name", "open" + path);

      if (rld::verbose () >= RLD_VERBOSE_DETAILS)
        std::cout << "image::open: " << name (). full ()
                  << " refs:" << references_ + 1 << std::endl;

      if (fd_ < 0)
      {
        if (writable)
          fd_ = ::open (path.c_str (), OPEN_FLAGS | O_RDWR | O_CREAT | O_TRUNC, CREATE_MODE);
        else
          fd_ = ::open (path.c_str (), OPEN_FLAGS | O_RDONLY);
        if (fd_ < 0)
          throw rld::error (::strerror (errno), "open:" + path);
      }

      ++references_;
    }

    void
    image::close ()
    {
      if (references_ > 0)
      {
        if (rld::verbose () >= RLD_VERBOSE_DETAILS)
          std::cout << "image::close: " << name ().full ()
                    << " refs:" << references_ << std::endl;

        --references_;
        if (references_ == 0)
        {
          ::close (fd_);
          fd_ = -1;
        }
      }
    }

    ssize_t
    image::read (uint8_t* buffer, size_t size)
    {
      ssize_t rsize = ::read (fd (), buffer, size);
      if (rsize < 0)
        throw rld::error (strerror (errno), "read:" + name ().path ());
      return rsize;
    }
 
    ssize_t
    image::write (const void* buffer, size_t size)
    {
      ssize_t wsize = ::write (fd (), buffer, size);
      if (wsize < 0)
        throw rld::error (strerror (errno), "write:" + name ().path ());
      return wsize;
    }
 
    void
    image::seek (off_t offset)
    {
      if (::lseek (fd (), name_.offset () + offset, SEEK_SET) < 0)
        throw rld::error (strerror (errno), "lseek:" + name ().path ());
    }
 
    bool
    image::seek_read (off_t offset, uint8_t* buffer, size_t size)
    {
      seek (offset);
      return size == (size_t) read (buffer, size);
    }
 
    bool
    image::seek_write (off_t offset, const void* buffer, size_t size)
    {
      seek (offset);
      return size == (size_t) write (buffer, size);
    }
 
    const file&
    image::name () const
    {
      return name_;
    }

    int
    image::references () const 
    {
      return references_;
    }

    size_t
    image::size () const 
    {
      return name ().size ();
    }

    int
    image::fd () const 
    {
      return fd_;
    }

    rld::elf::elf*
    image::elf (bool )
    {
      return elf_;
    }

    void
    image::set_elf (rld::elf::elf* elf)
    {
      elf_ = elf;
    }

    void
    image::symbol_referenced ()
    {
      ++symbol_refs;
    }
    
    int
    image::symbol_references () const
    {
      return symbol_refs;
    }

    void
    copy_file (image& in, image& out, size_t size)
    {
      #define COPY_FILE_BUFFER_SIZE (8 * 1024)
      uint8_t* buffer = 0;
      try
      {
        buffer = new uint8_t[COPY_FILE_BUFFER_SIZE];
        while (size)
        {
          size_t l = size < COPY_FILE_BUFFER_SIZE ? size : COPY_FILE_BUFFER_SIZE;
          ssize_t r = ::read (in.fd (), buffer, l);

          if (r < 0)
            throw rld::error (::strerror (errno), "reading: " + in.name ().full ());

          if (r == 0)
          {
            std::ostringstream oss;
            oss << "reading: " + in.name ().full () << " (" << size << ')';
            throw rld::error ("input too short", oss.str ());
          }

          ssize_t w = ::write (out.fd (), buffer, r);

          if (w < 0)
            throw rld::error (::strerror (errno), "writing: " + out.name ().full ());

          if (w != r)
            throw rld::error ("output trucated", "writing: " + out.name ().full ());

          size -= r;
        }
      }
      catch (...)
      {
        delete [] buffer;
        throw;
      }

      if (buffer)
        delete [] buffer;
    }

    /**
     * Defines for the header of an archive.
     */
    #define rld_archive_ident         "!<arch>\n"
    #define rld_archive_ident_size    (sizeof (rld_archive_ident) - 1)
    #define rld_archive_fhdr_base     rld_archive_ident_size
    #define rld_archive_fname         (0)
    #define rld_archive_fname_size    (16)
    #define rld_archive_mtime         (16)
    #define rld_archive_mtime_size    (12)
    #define rld_archive_uid           (28)
    #define rld_archive_uid_size      (6)
    #define rld_archive_gid           (34)
    #define rld_archive_gid_size      (6)
    #define rld_archive_mode          (40)
    #define rld_archive_mode_size     (8)
    #define rld_archive_size          (48)
    #define rld_archive_size_size     (10)
    #define rld_archive_magic         (58)
    #define rld_archive_magic_size    (2)
    #define rld_archive_fhdr_size     (60)
    #define rld_archive_max_file_size (1024)

    archive::archive (const std::string& path)
      : image (path, false)
    {
      if (!name ().is_valid ())
        throw rld_error_at ("name is empty");
      if (!name ().is_archive ())
        throw rld_error_at ("name is not an archive: " + name ().oname ());
    }

    archive::~archive ()
    {
      close ();
    }

    bool
    archive::is (const std::string& path) const
    {
      return name ().path () == path;
    }

    bool
    archive::is_valid ()
    {
      open ();
      uint8_t header[rld_archive_ident_size];
      seek_read (0, &header[0], rld_archive_ident_size);
      bool result = ::memcmp (header, rld_archive_ident,
                              rld_archive_ident_size) == 0 ? true : false;
      close ();
      return result;
    }

    void
    archive::load_objects (objects& objs)
    {
      off_t extended_file_names = 0;
      off_t offset = rld_archive_fhdr_base;
      size_t size = 0;

      while (true)
      {
        uint8_t header[rld_archive_fhdr_size];

        if (!read_header (offset, &header[0]))
          break;

        /*
         * The archive file headers are always aligned to an even address.
         */
        size = 
          (scan_decimal (&header[rld_archive_size], 
                         rld_archive_size_size) + 1) & ~1;

        /*
         * Check for the GNU extensions.
         */
        if (header[0] == '/')
        {
          off_t extended_off;

          switch (header[1])
          {
            case ' ':
              /*
               * Symbols table. Ignore the table.
               */
              break;
            case '/':
              /*
               * Extended file names table. Remember.
               */
              extended_file_names = offset + rld_archive_fhdr_size;
              break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
              /*
               * Offset into the extended file name table. If we do not have the
               * offset to the extended file name table find it.
               */
              extended_off = scan_decimal (&header[1], rld_archive_fname_size);

              if (extended_file_names == 0)
              {
                off_t off = offset;
                while (extended_file_names == 0)
                {
                  size_t esize = 
                    (scan_decimal (&header[rld_archive_size],
                                   rld_archive_size_size) + 1) & ~1;
                  off += esize + rld_archive_fhdr_size;
                    
                  if (!read_header (off, &header[0]))
                    throw rld::error ("No GNU extended file name section found",
                                      "get-names:" + name ().path ());
              
                  if ((header[0] == '/') && (header[1] == '/'))
                  {
                    extended_file_names = off + rld_archive_fhdr_size;
                    break;
                  }
                }
              }

              if (extended_file_names)
              {
                /*
                 * We know the offset in the archive to the extended file. Read
                 * the name from the table and compare with the name we are
                 * after.
                 */
                char cname[rld_archive_max_file_size];
                seek_read (extended_file_names + extended_off,
                           (uint8_t*) &cname[0], rld_archive_max_file_size);
                add_object (objs, cname,
                            offset + rld_archive_fhdr_size, size);
              }
              break;
            default:
              /*
               * Ignore the file because we do not know what it it.
               */
              break;
          }
        }
        else
        {
          /*
           * Normal archive name.
           */
          add_object (objs,
                      (char*) &header[rld_archive_fname],
                      offset + rld_archive_fhdr_size, size);
        }

        offset += size + rld_archive_fhdr_size;
      }
    }

    bool
    archive::operator< (const archive& rhs) const
    {
      return name ().path () < rhs.name ().path ();
    }

    bool
    archive::read_header (off_t offset, uint8_t* header)
    {
      if (!seek_read (offset, header, rld_archive_fhdr_size))
        return false;

      if ((header[rld_archive_magic] != 0x60) ||
          (header[rld_archive_magic + 1] != 0x0a))
        throw rld::error ("Invalid header magic numbers at " +
                          rld::to_string (offset), "read-header:" + name ().path ());
      
      return true;
    }

    void
    archive::add_object (objects& objs, const char* path, off_t offset, size_t size)
    {
      const char* end = path;
      while ((*end != '\0') && (*end != '/'))
        ++end;

      std::string str;
      str.append (path, end - path);

      if (rld::verbose () >= RLD_VERBOSE_FULL_DEBUG)
        std::cout << "archive::add-object: " << str << std::endl;

      file n (name ().path (), str, offset, size);
      objs[n.full()] = new object (*this, n);
    }

    void
    archive::write_header (const std::string& name,
                           uint32_t           mtime,
                           int                uid,
                           int                gid,
                           int                mode,
                           size_t             size)
    {
        uint8_t header[rld_archive_fhdr_size];

        memset (header, ' ', sizeof (header));
        
        size_t len = name.length ();
        if (len > rld_archive_fname_size)
          len = rld_archive_fname_size;
        memcpy (&header[rld_archive_fname], &name[0], len);

        set_number (mtime, header + rld_archive_mtime, rld_archive_mtime_size);
        set_number (uid, header + rld_archive_uid, rld_archive_uid_size);
        set_number (gid, header + rld_archive_gid, rld_archive_gid_size);
        set_number (mode, header + rld_archive_mode, rld_archive_mode_size, true);
        set_number (size, header + rld_archive_size, rld_archive_size_size);

        header[rld_archive_magic] = 0x60;
        header[rld_archive_magic + 1] = 0x0a;

        write (header, sizeof (header));
    }

    void
    archive::create (object_list& objects)
    {
      open (true);

      try
      {
        seek_write (0, rld_archive_ident, rld_archive_ident_size);

        /*
         * GNU extended filenames.
         */
        std::string extended_file_names;
        
        for (object_list::iterator oi = objects.begin ();
             oi != objects.end ();
             ++oi)
        {
          object& obj = *(*oi);
          const std::string&  oname = basename (obj.name ().oname ());
          if (oname.length () > rld_archive_fname_size)
            extended_file_names += oname + '\n';
        }

        if (!extended_file_names.empty ())
        {
          write_header ("//", 0, 0, 0, 0, extended_file_names.length ());
          write (extended_file_names.c_str (), extended_file_names.length ());
        }

        for (object_list::iterator oi = objects.begin ();
             oi != objects.end ();
             ++oi)
        {
          object& obj = *(*oi);

          obj.open ();

          try
          {
            std::string oname = basename (obj.name ().oname ());

            /*
             * Convert the file name to an offset into the extended file name
             * table if the file name is too long for the header.
             */

            if (oname.length () > rld_archive_fname_size)
            {
              size_t pos = extended_file_names.find_first_of (oname + '\n');
              if (pos == std::string::npos)
                throw rld_error_at ("extended file name not found");
              std::ostringstream oss;
              oss << '/' << pos;
              oname = oss.str ();
            }

            write_header (oname, 0, 0, 0, 0666, obj.name ().size ());
            obj.seek (0);
            copy_file (obj, *this, obj.name ().size ());
          }
          catch (...)
          {
            obj.close ();
            throw;
          }

          obj.close ();
        }
      }
      catch (...)
      {
        close ();
        throw;
      }
      
      close ();
    }

    object::object (archive& archive_, file& name_)
      : image (name_),
        archive_ (&archive_)
    {
      if (!name ().is_valid ())
        throw rld_error_at ("name is empty");
    }

    object::object (const std::string& path)
      : image (path),
        archive_ (0)
    {
      if (!name ().is_valid ())
        throw rld_error_at ("name is empty");
    }

    object::object ()
      : archive_ (0)
    {
    }

    object::~object ()
    {
      end ();
      close ();
    }

    void
    object::open ()
    {
      if (rld::verbose () >= RLD_VERBOSE_TRACE)
        std::cout << "object::open: " << name ().full () << std::endl;

      if (archive_)
        archive_->open ();
      else
        image::open ();
    }

    void
    object::close ()
    {
      if (rld::verbose () >= RLD_VERBOSE_TRACE)
        std::cout << "object::close: " << name ().full () << std::endl;

      if (archive_)
        archive_->close ();
      else
        image::close ();
    }

    void
    object::begin ()
    {
      /*
       * Begin an ELF session and get the ELF header.
       */
      rld::elf::begin (*this);
      rld::elf::get_header (*this, ehdr);
    }

    void
    object::end ()
    {
      rld::elf::end (*this);
    }

    void
    object::load_symbols (rld::symbols::table& symbols, bool local)
    {
      if (rld::verbose () >= RLD_VERBOSE_DETAILS)
        std::cout << "object:load-sym: " << name ().full () << std::endl;
      rld::elf::load_symbols (symbols, *this, local);
    }

    std::string
    object::get_string (int section, size_t offset)
    {
      return rld::elf::get_string (*this, section, offset);
    }
    
    int
    object::references () const 
    {
      if (archive_)
        return archive_->references ();
      return image::references ();
    }

    size_t
    object::size () const 
    {
      if (archive_)
        return archive_->size ();
      return image::size ();
    }

    int
    object::fd () const 
    {
      if (archive_)
        return archive_->fd ();
      return image::fd ();
    }

    rld::elf::elf* 
    object::elf (bool archive__)
    {
      if (archive__ && archive_)
        return archive_->elf ();
      return image::elf ();
    }

    void
    object::symbol_referenced ()
    {
      image::symbol_referenced ();
      if (archive_)
        archive_->symbol_referenced ();
    }
    
    archive*
    object::get_archive ()
    {
      return archive_;
    }

    int
    object::sections () const
    {
      return ehdr.e_shnum;
    }

    int
    object::section_strings () const
    {
      return ehdr.e_shstrndx;
    }

    rld::symbols::table&
    object::unresolved_symbols ()
    {
      return unresolved;
    }

    rld::symbols::list&
    object::external_symbols ()
    {
      return externals;
    }

    cache::cache ()
      : opened (false)
    {
    }

    cache::~cache ()
    {
      close ();
    }

    void
    cache::open ()
    {
      if (!opened)
      {
        collect_object_files ();
        archives_begin ();
        opened = true;
      }
    }

    void
    cache::close ()
    {
      if (opened)
      {
        /*
         * Must delete the object first as they could depend on archives.
         */
        for (objects::iterator oi = objects_.begin (); oi != objects_.end (); ++oi)
          delete (*oi).second;
        for (archives::iterator ai = archives_.begin (); ai != archives_.end (); ++ai)
          delete (*ai).second;
        opened = false;
      }
    }

    void
    cache::add (const std::string& path)
    {
        paths_.push_back (path);
        input (path);
    }

    void
    cache::add (paths& paths__)
    {
      for (paths::iterator pi = paths__.begin();
           pi != paths__.end();
           ++pi)
        add (*pi);
    }

    void
    cache::add_libraries (paths& paths__)
    {
      for (paths::iterator pi = paths__.begin();
           pi != paths__.end();
           ++pi)
        input (*pi);
    }

    void
    cache::archive_begin (const std::string& path)
    {
      archives::iterator ai = archives_.find (path);
      if (ai != archives_.end ())
      {
        archive* ar = (*ai).second;
        if (!ar->is_open ())
        {
          if (rld::verbose () >= RLD_VERBOSE_TRACE)
            std::cout << "cache:archive-begin: " << path << std::endl;
          ar->open ();
          rld::elf::begin (*ar);
        }
      }
    }

    void
    cache::archive_end (const std::string& path)
    {
      archives::iterator ai = archives_.find (path);
      if (ai != archives_.end ())
      {
        archive* ar = (*ai).second;
        if (ar->is_open ())
        {
          if (rld::verbose () >= RLD_VERBOSE_TRACE)
            std::cout << "cache:archive-end: " << path << std::endl;
          rld::elf::end (*ar);
          ar->close ();
        }
      }
    }

    void
    cache::archives_begin ()
    {
      for (archives::iterator ai = archives_.begin (); ai != archives_.end (); ++ai)
        archive_begin (((*ai).second)->path ());
    }

    void
    cache::archives_end ()
    {
      for (archives::iterator ai = archives_.begin (); ai != archives_.end (); ++ai)
        archive_end (((*ai).second)->path ());
    }
    
    void
    cache::collect_object_files ()
    {
      for (paths::iterator ni = paths_.begin (); ni != paths_.end (); ++ni)
        collect_object_files (*ni);
    }

    void
    cache::collect_object_files (const std::string& path)
    {
      archive* ar = new archive (path);

      if (ar->is_valid ())
      {
        try
        {
          archives_[path] = ar;
          ar->open ();
          ar->load_objects (objects_);
          ar->close ();
        }
        catch (...)
        {
          delete ar;
          throw;
        }
      }
      else
      {
        delete ar;
        object* obj = new object (path);
        if (!obj->name ().exists ())
        {
          delete obj;
          throw rld::error ("'" + path + "', Not found or a regular file.",
                            "file-check");
        }
        try
        {
          obj->open ();
          obj->begin ();
          obj->end ();
          obj->close ();
          objects_[path] = obj;
        }
        catch (...)
        {
          delete obj;
          throw;
        }
      }
    }

    void
    cache::load_symbols (rld::symbols::table& symbols, bool local)
    {
      for (objects::iterator oi = objects_.begin ();
           oi != objects_.end ();
           ++oi)
      {
        object* obj = (*oi).second;
        obj->open ();
        obj->begin ();
        obj->load_symbols (symbols, local);
        obj->end ();
        obj->close ();
      }
    }

    void
    cache::output_unresolved_symbols (std::ostream& out)
    {
      for (objects::iterator oi = objects_.begin ();
           oi != objects_.end ();
           ++oi)
      {
        object* obj = (*oi).second;
        if (obj)
        {
          out << obj->name ().full () << ':' << std::endl;
          rld::symbols::output (out, obj->unresolved_symbols ());
        }
      }
    }

    archives&
    cache::get_archives ()
    {
      return archives_;
    }
 
    objects&
    cache::get_objects ()
    {
      return objects_;
    }

    void
    cache::get_objects (object_list& list)
    {
      list.clear ();
      for (paths::iterator pi = paths_.begin ();
           pi != paths_.end ();
           ++pi)
      {
        objects::iterator oi = objects_.find (*pi);
        if (oi == objects_.end ())
          throw rld_error_at ("path not found in objects");
        list.push_back ((*oi).second);
      }
    }

    const paths&
    cache::get_paths () const
    {
      return paths_;
    }

    int
    cache::archive_count () const
    {
      return archives_.size ();
    }

    int
    cache::object_count () const
    {
      return objects_.size ();
    }

    int
    cache::path_count () const
    {
      return paths_.size ();
    }

    void
    cache::get_archive_files (files& afiles)
    {
      for (archives::iterator ai = archives_.begin (); ai != archives_.end (); ++ai)
        afiles.push_back ((*ai).second->name ().full ());
    }

    void
    cache::get_object_files (files& ofiles)
    {
      for (objects::iterator oi = objects_.begin (); oi != objects_.end (); ++oi)
        ofiles.push_back ((*oi).second->name ());
    }

    void
    cache::output_archive_files (std::ostream& out)
    {
      for (archives::iterator ai = archives_.begin (); ai != archives_.end (); ++ai)
        out << ' ' << (*ai).second->name ().full () << std::endl;
    }

    void
    cache::output_object_files (std::ostream& out)
    {
      for (objects::iterator oi = objects_.begin (); oi != objects_.end (); ++oi)
        out << ' ' << (*oi).second->name ().full () << std::endl;
    }

    void
    cache::input (const std::string& path)
    {
      if (opened)
      {
        collect_object_files (path);
        archive_begin (path);
      }
    }

    void
    find_libraries (paths& libraries, paths& libpaths, paths& libs)
    {
      if (rld::verbose () >= RLD_VERBOSE_INFO)
        std::cout << "Finding libraries:" << std::endl;
      libraries.clear ();
      for (paths::size_type l = 0; l < libs.size (); ++l)
      {
        std::string lib = "lib" + libs[l] + ".a";
        if (rld::verbose () >= RLD_VERBOSE_DETAILS)
          std::cout << "searching: " << lib << std::endl;
        bool found = false;
        for (paths::size_type p = 0; p < libpaths.size (); ++p)
        {
          std::string plib;
          path_join (libpaths[p], lib, plib);
          if (rld::verbose () >= RLD_VERBOSE_DETAILS)
              std::cout << "checking: " << plib << std::endl;
          if (check_file (plib))
          {
            if (rld::verbose () >= RLD_VERBOSE_INFO)
              std::cout << "found: " << plib << std::endl;
            libraries.push_back (plib);
            found = true;
            break;
          }
        }

        if (!found)
          throw rld::error ("Not found", lib);
      }
    }

  }
}