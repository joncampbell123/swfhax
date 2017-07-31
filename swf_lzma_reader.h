
#ifndef __ISP_UTILS_V4_SWF_FLASH_SWF_LZMA_READER_H
#define __ISP_UTILS_V4_SWF_FLASH_SWF_LZMA_READER_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

#include "sliding_window.h"
#include "swf.h"

int swf_reader_lzma_read(void *io,void *buf,int len);
int swf_reader_lzma_seek(void *io,uint64_t pos);
int swf_reader_init_lzma(swf_reader *swf);

#endif /* __ISP_UTILS_V4_SWF_FLASH_SWF_LZMA_READER_H */

