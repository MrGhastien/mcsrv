#ifndef IOMUX_H
#define IOMUX_H

#include "containers/bytebuffer.h"
#include "definitions.h"

#include <stdio.h>
#include <zlib.h>

enum IOType {
    IO_FILE,
    IO_GZFILE,
    IO_ZLIB,
    IO_BUFFER,
    IO_STRING
};

typedef i64 IOMux;

IOMux iomux_wrap_buffer(ByteBuffer* buffer);
IOMux iomux_wrap_stdfile(FILE* file);
IOMux iomux_wrap_gz(gzFile file);
IOMux iomux_wrap_string(const string* str);
IOMux iomux_wrap_zlib(IOMux compressed_stream, Arena* arena);

IOMux iomux_open(const string* path, const char* mode);
IOMux iomux_gz_open(const string* path, const char* mode);
IOMux iomux_new_string(Arena* arena);

i32 iomux_write(IOMux multiplexer, const void* data, u64 size);
i32 iomux_read(IOMux multiplexer, void* data, u64 size);
i32 iomux_getc(IOMux multiplexer);
i32 iomux_ungetc(IOMux multiplexer, i32 c);

i32 iomux_writef(IOMux multiplexer, const char* format, ...);
bool iomux_writec(IOMux multiplexer, i32 chr);
i32 iomux_writes(IOMux multiplexer, const char* cstr);
i32 iomux_write_str(IOMux multiplexer, const string* str);

bool iomux_eof(IOMux multiplexer);
string iomux_error(IOMux multiplexer, i32* out_code);

i32 iomux_seek(IOMux multiplexer, i32 off, i32 method);
i32 iomux_tell(IOMux multiplexer);

void iomux_close(IOMux multiplexer);
string iomux_string(IOMux multiplexer, Arena* arena);

#endif /* ! IOMUX_H */
