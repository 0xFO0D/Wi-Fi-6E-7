#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/zlib.h>
#include <linux/xz.h>
#include "fw_common.h"

/* CRC32 table for checksum calculation */
static u32 crc32_table[256];
static bool crc32_initialized;

static void init_crc32_table(void)
{
    u32 i, j, crc;
    
    for (i = 0; i < 256; i++) {
        crc = i;
        for (j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc = crc >> 1;
        }
        crc32_table[i] = crc;
    }
    crc32_initialized = true;
}

int fw_verify_checksum(const void *data, size_t len, u32 expected)
{
    const u8 *buf = data;
    u32 crc = 0xFFFFFFFF;
    size_t i;

    if (!crc32_initialized)
        init_crc32_table();

    for (i = 0; i < len; i++)
        crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ buf[i]];

    crc = ~crc;
    return (crc == expected) ? FW_ERR_NONE : FW_ERR_VERIFY;
}

int fw_decompress_zlib(const void *src, size_t src_len,
                      void *dst, size_t *dst_len)
{
    z_stream strm = {};
    int ret;

    ret = zlib_inflateInit(&strm);
    if (ret != Z_OK)
        return FW_ERR_COMPRESS;

    strm.avail_in = src_len;
    strm.next_in = src;
    strm.avail_out = *dst_len;
    strm.next_out = dst;

    ret = zlib_inflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        zlib_inflateEnd(&strm);
        return FW_ERR_COMPRESS;
    }

    *dst_len = strm.total_out;
    zlib_inflateEnd(&strm);
    return FW_ERR_NONE;
}

int fw_decompress_xz(const void *src, size_t src_len,
                    void *dst, size_t *dst_len)
{
    struct xz_dec *dec;
    struct xz_buf buf;
    int ret;

    dec = xz_dec_init(XZ_SINGLE, 0);
    if (!dec)
        return FW_ERR_COMPRESS;

    buf.in = src;
    buf.in_pos = 0;
    buf.in_size = src_len;
    buf.out = dst;
    buf.out_pos = 0;
    buf.out_size = *dst_len;

    ret = xz_dec_run(dec, &buf);
    xz_dec_end(dec);

    if (ret != XZ_STREAM_END)
        return FW_ERR_COMPRESS;

    *dst_len = buf.out_pos;
    return FW_ERR_NONE;
}

int fw_version_compare(const struct fw_version *v1,
                      const struct fw_version *v2)
{
    if (v1->major != v2->major)
        return v1->major - v2->major;
    if (v1->minor != v2->minor)
        return v1->minor - v2->minor;
    if (v1->patch != v2->patch)
        return v1->patch - v2->patch;
    return v1->build - v2->build;
}

void fw_version_to_string(const struct fw_version *ver,
                         char *str, size_t len)
{
    if (ver->extra[0])
        snprintf(str, len, "%u.%u.%u.%u-%s",
                ver->major, ver->minor,
                ver->patch, ver->build,
                ver->extra);
    else
        snprintf(str, len, "%u.%u.%u.%u",
                ver->major, ver->minor,
                ver->patch, ver->build);
} 