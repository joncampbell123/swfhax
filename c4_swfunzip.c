
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

#include "swf.h"

static unsigned char temp[4096];

int main(int argc,char **argv) {
	swf_reader *swf;
	int fd,ofd,rd;

	if (argc < 3) {
		fprintf(stderr,"c4_swfdump <file> <output uncompressed swf>\n");
		return 1;
	}

	fd = open(argv[1],O_RDONLY);
	if (fd < 0) {
		fprintf(stderr,"Unable to open file %s\n",argv[1]);
		return 1;
	}
	ofd = open(argv[2],O_WRONLY|O_CREAT|O_TRUNC,0644);
	if (ofd < 0) {
		fprintf(stderr,"Unable to create file %s\n",argv[2]);
		return 1;
	}
	if ((swf=swf_reader_create()) == NULL) {
		fprintf(stderr,"Cannot create SWF reader\n");
		return 1;
	}
	if (swf_reader_assign_fd(swf,fd)) {
		fprintf(stderr,"Cannot assign fd\n");
		return 1;
	}
	swf_reader_assign_fd_ownership(swf);
	if (swf_reader_get_file_header(swf)) {
		fprintf(stderr,"Unable to read file header.\n");
		return 1;
	}

	printf("SWF version %u (%c%c%c), file length %lu\n",
		swf->header.Version,
		swf->header.Signature[0],
		swf->header.Signature[1],
		swf->header.Signature[2],
		(unsigned long)swf->header.FileLength);

	/* copy */
	if (swf_reader_seek(swf,0)) {
		printf("Unable to seek to offset 0\n");
		return 1;
	}

	while ((rd=swf_reader_read(swf,temp,sizeof(temp))) > 0)
		write(ofd,temp,rd);

	swf = swf_reader_destroy(swf);
	close(ofd);
	return 0;
}

