#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libtarmod.h"

int main(int argc, char *argv[])
{
	const char default_infi[] = "test.tar";
	const char *infi = default_infi;
	if(argc > 1)
		infi = argv[1];
	TAR *pTar = NULL;

	int ret = tar_open(&pTar, infi, NULL, O_RDONLY, 0, 0, NULL);
	if(ret != 0)
	{
		printf("Failed to open input tar\n");
		return ret;
	}

	//Build index for quick access
	int i=0, nBlocks=0;
	while ((i = th_read(pTar)) == 0)
	{
		if(TH_ISREG(pTar))
		{
			//streampos pos = st.pubseekoff (0, ios_base::cur);

			//fileInPos.push_back(pos);
			if (tar_skip_regfile(pTar) != 0) return -1;
		}
		nBlocks += 1;
		printf ("%d\n",nBlocks);
	}

	return 0;
}
