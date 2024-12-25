#ifndef IOMUX_H
#define IOMUX_H

#include "containers/bytebuffer.h"
#include "definitions.h"

#include <stdio.h>
#include <zlib.h>

enum IOType {
    IO_FILE,
    IO_GZFILE,
    IO_BUFFER,
    IO_STRING
};

typedef i64 IOMux;

IOMux iomux_wrap_buffer(ByteBuffer* buffer);
IOMux iomux_wrap_stdfile(FILE* file);
IOMux iomux_wrap_gz(gzFile file);

IOMux iomux_open(const string* path, const char* mode);
IOMux iomux_gz_open(const string* path, const char* mode);

i32 iomux_write(IOMux multiplexer, const void* data, u64 size);
i32 iomux_read(IOMux multiplexer, void* data, u64 size);

bool iomux_eof(IOMux multiplexer);
string iomux_error(IOMux multiplexer, i32* out_code);

void iomux_close(IOMux multiplexer);

#endif /* ! IOMUX_H */
