
#include "libtarmod.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h> 
#include <limits.h>
#include <dirent.h>
#include <pwd.h>
#include <fnmatch.h>
#include <libgen.h>
#include <bsd/string.h>
#include <grp.h>
#include <time.h>
#include <utime.h>

void * sys_openfunc(void *opaque, const char *filename, int flags, mode_t mode, ...)
{
	return (void *)open(filename, flags, mode);
}

int sys_closefunc(void *opaque, void *h)
{
	return close(*(int *)&h);
}

ssize_t sys_readfunc(void *opaque, void *h, void * b, size_t c)
{
	return read(*(int *)&h, b, c);
}

ssize_t sys_writefunc(void *opaque, void *h, const void * b, size_t c)
{
	return write(*(int *)&h, b, c);
}

int sys_mkdirfunc(void *opaque, const char *pathname, mode_t mode)
{
	return mkdir(pathname, mode);
}

uint64_t sys_lseekfunc(void *opaque, void *h, uint64_t offset, int whence)
{
	return lseek(*(int *)&h, offset, whence);
}

/***** handle.c ************************************************************/

static tartype_t default_type = { sys_openfunc, sys_closefunc, sys_readfunc, sys_writefunc, sys_lseekfunc,
	sys_mkdirfunc,
	sys_openfunc, sys_closefunc, sys_readfunc, sys_writefunc };

static int
tar_init(TAR **t, const char *pathname, tartype_t *type,
	 int oflags, int mode, int options, void *opaque)
{
	if ((oflags & O_ACCMODE) == O_RDWR)
	{
		errno = EINVAL;
		return -1;
	}

	*t = (TAR *)calloc(1, sizeof(TAR));
	if (*t == NULL)
		return -1;

	(*t)->pathname = (char *)pathname;
	(*t)->options = options;
	(*t)->type = (type ? type : &default_type);
	(*t)->oflags = oflags;
	(*t)->opaque = opaque;

	if ((oflags & O_ACCMODE) == O_RDONLY)
		(*t)->h = libtarmod_hash_new(256,
					  (libtarmod_hashfunc_t)path_hashfunc);
	else
		(*t)->h = libtarmod_hash_new(16, (libtarmod_hashfunc_t)dev_hash);
	if ((*t)->h == NULL)
	{
		free(*t);
		return -1;
	}

	return 0;
}


/* open a new tarfile handle */
int
tar_open(TAR **t, const char *pathname, tartype_t *type,
	 int oflags, int mode, int options, void *opaque)
{
	if (tar_init(t, pathname, type, oflags, mode, options, opaque) == -1)
		return -1;

	if ((options & TAR_NOOVERWRITE) && (oflags & O_CREAT))
		oflags |= O_EXCL;

#ifdef O_BINARY
	oflags |= O_BINARY;
#endif

	(*t)->fd = (*((*t)->type->openfunc))((*t)->opaque, pathname, oflags, mode);
	if ((*t)->fd == (void *)-1)
	{
		libtarmod_hash_free((*t)->h, NULL);
		free(*t);
		return -1;
	}

	return 0;
}


void tar_init_type(tartype_t *type)
{
	memcpy(type, &default_type, sizeof(tartype_t));
}

int
tar_fdopen(TAR **t, void *fd, const char *pathname, tartype_t *type,
	   int oflags, int mode, int options, void *opaque)
{
	if (tar_init(t, pathname, type, oflags, mode, options, opaque) == -1)
		return -1;

	(*t)->fd = fd;
	return 0;
}


void *
tar_fd(TAR *t)
{
	return t->fd;
}


/* close tarfile handle */
int
tar_close(TAR *t)
{
	int i;

	i = (*(t->type->closefunc))(t->opaque, t->fd);

	if (t->h != NULL)
		libtarmod_hash_free(t->h, ((t->oflags & O_ACCMODE) == O_RDONLY
					? free
					: (libtarmod_freefunc_t)tar_dev_free));
	if (t->th_pathname != NULL)
		free(t->th_pathname);
	free(t);

	return i;
}

/***** append.c ************************************************************/

struct tar_dev
{
	dev_t td_dev;
	libtarmod_hash_t *td_h;
};
typedef struct tar_dev tar_dev_t;

struct tar_ino
{
	ino_t ti_ino;
	char ti_name[MAXPATHLEN];
};
typedef struct tar_ino tar_ino_t;


/* free memory associated with a tar_dev_t */
void
tar_dev_free(tar_dev_t *tdp)
{
	libtarmod_hash_free(tdp->td_h, free);
	free(tdp);
}


/* appends a file to the tar archive */
int
tar_append_file(TAR *t, const char *realname, const char *savename)
{
	struct stat s;
	int i;
	libtarmod_hashptr_t hp;
	tar_dev_t *td = NULL;
	tar_ino_t *ti = NULL;
	char path[MAXPATHLEN];

#ifdef DEBUG
	printf("==> tar_append_file(TAR=0x%lx (\"%s\"), realname=\"%s\", "
	       "savename=\"%s\")\n", t, t->pathname, realname,
	       (savename ? savename : "[NULL]"));
#endif

	if (lstat(realname, &s) != 0)
	{
#ifdef DEBUG
		perror("lstat()");
#endif
		return -1;
	}

	/* set header block */
#ifdef DEBUG
	puts("    tar_append_file(): setting header block...");
#endif
	memset(&(t->th_buf), 0, sizeof(struct tar_header));
	th_set_from_stat(t, &s);

	/* set the header path */
#ifdef DEBUG
	puts("    tar_append_file(): setting header path...");
#endif
	th_set_path(t, (savename ? savename : realname));

	/* check if it's a hardlink */
#ifdef DEBUG
	puts("    tar_append_file(): checking inode cache for hardlink...");
#endif
	libtarmod_hashptr_reset(&hp);
	if (libtarmod_hash_getkey(t->h, &hp, &(s.st_dev),
			       (libtarmod_matchfunc_t)dev_match) != 0)
		td = (tar_dev_t *)libtarmod_hashptr_data(&hp);
	else
	{
#ifdef DEBUG
		printf("+++ adding hash for device (0x%lx, 0x%lx)...\n",
		       major(s.st_dev), minor(s.st_dev));
#endif
		td = (tar_dev_t *)calloc(1, sizeof(tar_dev_t));
		td->td_dev = s.st_dev;
		td->td_h = libtarmod_hash_new(256, (libtarmod_hashfunc_t)ino_hash);
		if (td->td_h == NULL)
			return -1;
		if (libtarmod_hash_add(t->h, td) == -1)
			return -1;
	}
	libtarmod_hashptr_reset(&hp);
	if (libtarmod_hash_getkey(td->td_h, &hp, &(s.st_ino),
			       (libtarmod_matchfunc_t)ino_match) != 0)
	{
		ti = (tar_ino_t *)libtarmod_hashptr_data(&hp);
#ifdef DEBUG
		printf("    tar_append_file(): encoding hard link \"%s\" "
		       "to \"%s\"...\n", realname, ti->ti_name);
#endif
		t->th_buf.typeflag = LNKTYPE;
		th_set_link(t, ti->ti_name);
	}
	else
	{
#ifdef DEBUG
		printf("+++ adding entry: device (0x%lx,0x%lx), inode %ld "
		       "(\"%s\")...\n", major(s.st_dev), minor(s.st_dev),
		       s.st_ino, realname);
#endif
		ti = (tar_ino_t *)calloc(1, sizeof(tar_ino_t));
		if (ti == NULL)
			return -1;
		ti->ti_ino = s.st_ino;
		snprintf(ti->ti_name, sizeof(ti->ti_name), "%s",
			 savename ? savename : realname);
		libtarmod_hash_add(td->td_h, ti);
	}

	/* check if it's a symlink */
	if (TH_ISSYM(t))
	{
		i = readlink(realname, path, sizeof(path));
		if (i == -1)
			return -1;
		if (i >= MAXPATHLEN)
			i = MAXPATHLEN - 1;
		path[i] = '\0';
#ifdef DEBUG
		printf("    tar_append_file(): encoding symlink \"%s\" -> "
		       "\"%s\"...\n", realname, path);
#endif
		th_set_link(t, path);
	}

	/* print file info */
	if (t->options & TAR_VERBOSE)
		th_print_long_ls(t);

#ifdef DEBUG
	puts("    tar_append_file(): writing header");
#endif
	/* write header */
	if (th_write(t) != 0)
	{
#ifdef DEBUG
		printf("t->fd = %d\n", t->fd);
#endif
		return -1;
	}
#ifdef DEBUG
	puts("    tar_append_file(): back from th_write()");
#endif

	/* if it's a regular file, write the contents as well */
	if (TH_ISREG(t) && tar_append_regfile(t, realname) != 0)
		return -1;

	return 0;
}


/* write EOF indicator */
int
tar_append_eof(TAR *t)
{
	int i, j;
	char block[T_BLOCKSIZE];

	memset(&block, 0, T_BLOCKSIZE);
	for (j = 0; j < 2; j++)
	{
		i = tar_block_write(t, &block);
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}
	}

	return 0;
}


/* add file contents to a tarchive */
int
tar_append_regfile(TAR *t, const char *realname)
{
	char block[T_BLOCKSIZE];
	int filefd;
	int i, j;
	size_t size;
	int rv = -1;

#if defined(O_BINARY)
	filefd = open(realname, O_RDONLY|O_BINARY);
#else
	filefd = open(realname, O_RDONLY);
#endif
	if (filefd == -1)
	{
#ifdef DEBUG
		perror("open()");
#endif
		return -1;
	}

	size = th_get_size(t);
	for (i = size; i > T_BLOCKSIZE; i -= T_BLOCKSIZE)
	{
		j = read(filefd, &block, T_BLOCKSIZE);
		if (j != T_BLOCKSIZE)
		{
			if (j != -1)
				errno = EINVAL;
			goto fail;
		}
		if (tar_block_write(t, &block) == -1)
			goto fail;
	}

	if (i > 0)
	{
		j = read(filefd, &block, i);
		if (j == -1)
			goto fail;
		memset(&(block[i]), 0, T_BLOCKSIZE - i);
		if (tar_block_write(t, &block) == -1)
			goto fail;
	}

	/* success! */
	rv = 0;
fail:
	close(filefd);

	return rv;
}

/***** block.c *************************************************************/

#define BIT_ISSET(bitmask, bit) ((bitmask) & (bit))


/* read a header block */
/* FIXME: the return value of this function should match the return value
	  of tar_block_read(), which is a macro which references a prototype
	  that returns a ssize_t.  So far, this is safe, since tar_block_read()
	  only ever reads 512 (T_BLOCKSIZE) bytes at a time, so any difference
	  in size of ssize_t and int is of negligible risk.  BUT, if
	  T_BLOCKSIZE ever changes, or ever becomes a variable parameter
	  controllable by the user, all the code that calls it,
	  including this function and all code that calls it, should be
	  fixed for security reasons.
	  Thanks to Chris Palmer for the critique.
*/
int
th_read_internal(TAR *t)
{
	int i;
	int num_zero_blocks = 0;

#ifdef DEBUG
	printf("==> th_read_internal(TAR=\"%s\")\n", t->pathname);
#endif

	while ((i = tar_block_read(t, &(t->th_buf))) == T_BLOCKSIZE)
	{
		/* two all-zero blocks mark EOF */
		if (t->th_buf.name[0] == '\0')
		{
			num_zero_blocks++;
			if (!BIT_ISSET(t->options, TAR_IGNORE_EOT)
			    && num_zero_blocks >= 2)
				return 0;	/* EOF */
			else
				continue;
		}

		/* verify magic and version */
		if (BIT_ISSET(t->options, TAR_CHECK_MAGIC)
		    && strncmp(t->th_buf.magic, TMAGIC, TMAGLEN - 1) != 0)
		{
#ifdef DEBUG
			puts("!!! unknown magic value in tar header");
#endif
			return -2;
		}

		if (BIT_ISSET(t->options, TAR_CHECK_VERSION)
		    && strncmp(t->th_buf.version, TVERSION, TVERSLEN) != 0)
		{
#ifdef DEBUG
			puts("!!! unknown version value in tar header");
#endif
			return -2;
		}

		/* check chksum */
		if (!BIT_ISSET(t->options, TAR_IGNORE_CRC)
		    && !th_crc_ok(t))
		{
#ifdef DEBUG
			puts("!!! tar header checksum error");
#endif
			return -2;
		}

		break;
	}

#ifdef DEBUG
	printf("<== th_read_internal(): returning %d\n", i);
#endif
	return i;
}


/* wrapper function for th_read_internal() to handle GNU extensions */
int
th_read(TAR *t)
{
	int i;
	size_t sz, j, blocks;
	char *ptr;

#ifdef DEBUG
	printf("==> th_read(t=0x%lx)\n", t);
#endif

	if (t->th_buf.gnu_longname != NULL)
		free(t->th_buf.gnu_longname);
	if (t->th_buf.gnu_longlink != NULL)
		free(t->th_buf.gnu_longlink);
	memset(&(t->th_buf), 0, sizeof(struct tar_header));

	i = th_read_internal(t);
	if (i == 0)
		return 1;
	else if (i != T_BLOCKSIZE)
	{
		if (i != -1)
			errno = EINVAL;
		return -1;
	}

	/* check for GNU long link extention */
	if (TH_ISLONGLINK(t))
	{
		sz = th_get_size(t);
		blocks = (sz / T_BLOCKSIZE) + (sz % T_BLOCKSIZE ? 1 : 0);
		if (blocks > ((size_t)-1 / T_BLOCKSIZE))
		{
			errno = E2BIG;
			return -1;
		}
#ifdef DEBUG
		printf("    th_read(): GNU long linkname detected "
		       "(%ld bytes, %d blocks)\n", sz, blocks);
#endif
		t->th_buf.gnu_longlink = (char *)malloc(blocks * T_BLOCKSIZE);
		if (t->th_buf.gnu_longlink == NULL)
			return -1;

		for (j = 0, ptr = t->th_buf.gnu_longlink; j < blocks;
		     j++, ptr += T_BLOCKSIZE)
		{
#ifdef DEBUG
			printf("    th_read(): reading long linkname "
			       "(%d blocks left, ptr == %ld)\n", blocks-j, ptr);
#endif
			i = tar_block_read(t, ptr);
			if (i != T_BLOCKSIZE)
			{
				if (i != -1)
					errno = EINVAL;
				return -1;
			}
#ifdef DEBUG
			printf("    th_read(): read block == \"%s\"\n", ptr);
#endif
		}
#ifdef DEBUG
		printf("    th_read(): t->th_buf.gnu_longlink == \"%s\"\n",
		       t->th_buf.gnu_longlink);
#endif

		i = th_read_internal(t);
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}
	}

	/* check for GNU long name extention */
	if (TH_ISLONGNAME(t))
	{
		sz = th_get_size(t);
		blocks = (sz / T_BLOCKSIZE) + (sz % T_BLOCKSIZE ? 1 : 0);
		if (blocks > ((size_t)-1 / T_BLOCKSIZE))
		{
			errno = E2BIG;
			return -1;
		}
#ifdef DEBUG
		printf("    th_read(): GNU long filename detected "
		       "(%ld bytes, %d blocks)\n", sz, blocks);
#endif
		t->th_buf.gnu_longname = (char *)malloc(blocks * T_BLOCKSIZE);
		if (t->th_buf.gnu_longname == NULL)
			return -1;

		for (j = 0, ptr = t->th_buf.gnu_longname; j < blocks;
		     j++, ptr += T_BLOCKSIZE)
		{
#ifdef DEBUG
			printf("    th_read(): reading long filename "
			       "(%d blocks left, ptr == %ld)\n", blocks-j, ptr);
#endif
			i = tar_block_read(t, ptr);
			if (i != T_BLOCKSIZE)
			{
				if (i != -1)
					errno = EINVAL;
				return -1;
			}
#ifdef DEBUG
			printf("    th_read(): read block == \"%s\"\n", ptr);
#endif
		}
#ifdef DEBUG
		printf("    th_read(): t->th_buf.gnu_longname == \"%s\"\n",
		       t->th_buf.gnu_longname);
#endif

		i = th_read_internal(t);
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}
	}

#if 0
	/*
	** work-around for old archive files with broken typeflag fields
	** NOTE: I fixed this in the TH_IS*() macros instead
	*/

	/*
	** (directories are signified with a trailing '/')
	*/
	if (t->th_buf.typeflag == AREGTYPE
	    && t->th_buf.name[strlen(t->th_buf.name) - 1] == '/')
		t->th_buf.typeflag = DIRTYPE;

	/*
	** fallback to using mode bits
	*/
	if (t->th_buf.typeflag == AREGTYPE)
	{
		mode = (mode_t)oct_to_int(t->th_buf.mode);

		if (S_ISREG(mode))
			t->th_buf.typeflag = REGTYPE;
		else if (S_ISDIR(mode))
			t->th_buf.typeflag = DIRTYPE;
		else if (S_ISFIFO(mode))
			t->th_buf.typeflag = FIFOTYPE;
		else if (S_ISCHR(mode))
			t->th_buf.typeflag = CHRTYPE;
		else if (S_ISBLK(mode))
			t->th_buf.typeflag = BLKTYPE;
		else if (S_ISLNK(mode))
			t->th_buf.typeflag = SYMTYPE;
	}
#endif

	return 0;
}


/* write a header block */
int
th_write(TAR *t)
{
	int i, j;
	char type2;
	size_t sz, sz2;
	char *ptr;
	char buf[T_BLOCKSIZE];

#ifdef DEBUG
	printf("==> th_write(TAR=\"%s\")\n", t->pathname);
	th_print(t);
#endif

	if ((t->options & TAR_GNU) && t->th_buf.gnu_longlink != NULL)
	{
#ifdef DEBUG
		printf("th_write(): using gnu_longlink (\"%s\")\n",
		       t->th_buf.gnu_longlink);
#endif
		/* save old size and type */
		type2 = t->th_buf.typeflag;
		sz2 = th_get_size(t);

		/* write out initial header block with fake size and type */
		t->th_buf.typeflag = GNU_LONGLINK_TYPE;
		sz = strlen(t->th_buf.gnu_longlink);
		th_set_size(t, sz);
		th_finish(t);
		i = tar_block_write(t, &(t->th_buf));
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}

		/* write out extra blocks containing long name */
		for (j = (sz / T_BLOCKSIZE) + (sz % T_BLOCKSIZE ? 1 : 0),
		     ptr = t->th_buf.gnu_longlink; j > 1;
		     j--, ptr += T_BLOCKSIZE)
		{
			i = tar_block_write(t, ptr);
			if (i != T_BLOCKSIZE)
			{
				if (i != -1)
					errno = EINVAL;
				return -1;
			}
		}
		memset(buf, 0, T_BLOCKSIZE);
		strncpy(buf, ptr, T_BLOCKSIZE);
		i = tar_block_write(t, &buf);
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}

		/* reset type and size to original values */
		t->th_buf.typeflag = type2;
		th_set_size(t, sz2);
	}

	if ((t->options & TAR_GNU) && t->th_buf.gnu_longname != NULL)
	{
#ifdef DEBUG
		printf("th_write(): using gnu_longname (\"%s\")\n",
		       t->th_buf.gnu_longname);
#endif
		/* save old size and type */
		type2 = t->th_buf.typeflag;
		sz2 = th_get_size(t);

		/* write out initial header block with fake size and type */
		t->th_buf.typeflag = GNU_LONGNAME_TYPE;
		sz = strlen(t->th_buf.gnu_longname);
		th_set_size(t, sz);
		th_finish(t);
		i = tar_block_write(t, &(t->th_buf));
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}

		/* write out extra blocks containing long name */
		for (j = (sz / T_BLOCKSIZE) + (sz % T_BLOCKSIZE ? 1 : 0),
		     ptr = t->th_buf.gnu_longname; j > 1;
		     j--, ptr += T_BLOCKSIZE)
		{
			i = tar_block_write(t, ptr);
			if (i != T_BLOCKSIZE)
			{
				if (i != -1)
					errno = EINVAL;
				return -1;
			}
		}
		memset(buf, 0, T_BLOCKSIZE);
		strncpy(buf, ptr, T_BLOCKSIZE);
		i = tar_block_write(t, &buf);
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}

		/* reset type and size to original values */
		t->th_buf.typeflag = type2;
		th_set_size(t, sz2);
	}

	th_finish(t);

#ifdef DEBUG
	/* print tar header */
	th_print(t);
#endif

	i = tar_block_write(t, &(t->th_buf));
	if (i != T_BLOCKSIZE)
	{
		if (i != -1)
			errno = EINVAL;
		return -1;
	}

#ifdef DEBUG
	puts("th_write(): returning 0");
#endif
	return 0;
}

/***** decode.c ************************************************************/

/* determine full path name */
char *
th_get_pathname(TAR *t)
{
	if (t->th_buf.gnu_longname)
		return t->th_buf.gnu_longname;

	/* allocate the th_pathname buffer if not already */
	if (t->th_pathname == NULL)
	{
		t->th_pathname = (char *)malloc(MAXPATHLEN * sizeof(char));
		if (t->th_pathname == NULL)
			/* out of memory */
			return NULL;
	}

	if (t->th_buf.prefix[0] == '\0')
	{
		snprintf(t->th_pathname, MAXPATHLEN, "%.100s", t->th_buf.name);
	}
	else
	{
		snprintf(t->th_pathname, MAXPATHLEN, "%.155s/%.100s",
			 t->th_buf.prefix, t->th_buf.name);
	}

	/* will be deallocated in tar_close() */
	return t->th_pathname;
}


uid_t
th_get_uid(TAR *t)
{
	int uid;
	struct passwd *pw;

	pw = getpwnam(t->th_buf.uname);
	if (pw != NULL)
		return pw->pw_uid;

	/* if the password entry doesn't exist */
	sscanf(t->th_buf.uid, "%o", &uid);
	return uid;
}


gid_t
th_get_gid(TAR *t)
{
	int gid;
	struct group *gr;

	gr = getgrnam(t->th_buf.gname);
	if (gr != NULL)
		return gr->gr_gid;

	/* if the group entry doesn't exist */
	sscanf(t->th_buf.gid, "%o", &gid);
	return gid;
}


mode_t
th_get_mode(TAR *t)
{
	mode_t mode;

	mode = (mode_t)oct_to_int(t->th_buf.mode);
	if (! (mode & S_IFMT))
	{
		switch (t->th_buf.typeflag)
		{
		case SYMTYPE:
			mode |= S_IFLNK;
			break;
		case CHRTYPE:
			mode |= S_IFCHR;
			break;
		case BLKTYPE:
			mode |= S_IFBLK;
			break;
		case DIRTYPE:
			mode |= S_IFDIR;
			break;
		case FIFOTYPE:
			mode |= S_IFIFO;
			break;
		case AREGTYPE:
			if (t->th_buf.name[strlen(t->th_buf.name) - 1] == '/')
			{
				mode |= S_IFDIR;
				break;
			}
			/* FALLTHROUGH */
		case LNKTYPE:
		case REGTYPE:
		default:
			mode |= S_IFREG;
		}
	}

	return mode;
}

/***** encode.c ************************************************************/

/* magic, version, and checksum */
void
th_finish(TAR *t)
{
	if (t->options & TAR_GNU)
	{
		/* we're aiming for this result, but must do it in
		 * two calls to avoid FORTIFY segfaults on some Linux
		 * systems:
		 *      strncpy(t->th_buf.magic, "ustar  ", 8);
		 */
		strncpy(t->th_buf.magic, "ustar ", 6);
		strncpy(t->th_buf.version, " ", 2);
	}
	else
	{
		strncpy(t->th_buf.version, TVERSION, TVERSLEN);
		strncpy(t->th_buf.magic, TMAGIC, TMAGLEN);
	}

	int_to_oct(th_crc_calc(t), t->th_buf.chksum, 8);
}

/* map a file mode to a typeflag */
void
th_set_type(TAR *t, mode_t mode)
{
	if (S_ISLNK(mode))
		t->th_buf.typeflag = SYMTYPE;
	if (S_ISREG(mode))
		t->th_buf.typeflag = REGTYPE;
	if (S_ISDIR(mode))
		t->th_buf.typeflag = DIRTYPE;
	if (S_ISCHR(mode))
		t->th_buf.typeflag = CHRTYPE;
	if (S_ISBLK(mode))
		t->th_buf.typeflag = BLKTYPE;
	if (S_ISFIFO(mode) || S_ISSOCK(mode))
		t->th_buf.typeflag = FIFOTYPE;
}


/* encode file path */
void
th_set_path(TAR *t, const char *pathname)
{
	char suffix[2] = "";
	char *tmp;

#ifdef DEBUG
	printf("in th_set_path(th, pathname=\"%s\")\n", pathname);
#endif

	if (t->th_buf.gnu_longname != NULL)
		free(t->th_buf.gnu_longname);
	t->th_buf.gnu_longname = NULL;

	if (pathname[strlen(pathname) - 1] != '/' && TH_ISDIR(t))
		strcpy(suffix, "/");

	if (strlen(pathname) > T_NAMELEN-1 && (t->options & TAR_GNU))
	{
		/* GNU-style long name */
		t->th_buf.gnu_longname = strdup(pathname);
		strncpy(t->th_buf.name, t->th_buf.gnu_longname, T_NAMELEN);
	}
	else if (strlen(pathname) > T_NAMELEN)
	{
		/* POSIX-style prefix field */
		tmp = (char *)strchr(&(pathname[strlen(pathname) - T_NAMELEN - 1]), '/');
		if (tmp == NULL)
		{
			printf("!!! '/' not found in \"%s\"\n", pathname);
			return;
		}
		snprintf(t->th_buf.name, 100, "%s%s", &(tmp[1]), suffix);
		snprintf(t->th_buf.prefix,
			 ((tmp - pathname + 1) <
			  155 ? (tmp - pathname + 1) : 155), "%s", pathname);
	}
	else
		/* classic tar format */
		snprintf(t->th_buf.name, 100, "%s%s", pathname, suffix);

#ifdef DEBUG
	puts("returning from th_set_path()...");
#endif
}


/* encode link path */
void
th_set_link(TAR *t, const char *linkname)
{
#ifdef DEBUG
	printf("==> th_set_link(th, linkname=\"%s\")\n", linkname);
#endif

	if (strlen(linkname) > T_NAMELEN-1 && (t->options & TAR_GNU))
	{
		/* GNU longlink format */
		t->th_buf.gnu_longlink = strdup(linkname);
		strcpy(t->th_buf.linkname, "././@LongLink");
	}
	else
	{
		/* classic tar format */
		strlcpy(t->th_buf.linkname, linkname,
			sizeof(t->th_buf.linkname));
		if (t->th_buf.gnu_longlink != NULL)
			free(t->th_buf.gnu_longlink);
		t->th_buf.gnu_longlink = NULL;
	}
}


/* encode device info */
void
th_set_device(TAR *t, dev_t device)
{
#ifdef DEBUG
	printf("th_set_device(): major = %d, minor = %d\n",
	       major(device), minor(device));
#endif
	int_to_oct(major(device), t->th_buf.devmajor, 8);
	int_to_oct(minor(device), t->th_buf.devminor, 8);
}


/* encode user info */
void
th_set_user(TAR *t, uid_t uid)
{
	struct passwd *pw;

	pw = getpwuid(uid);
	if (pw != NULL)
		strlcpy(t->th_buf.uname, pw->pw_name, sizeof(t->th_buf.uname));

	int_to_oct(uid, t->th_buf.uid, 8);
}


/* encode group info */
void
th_set_group(TAR *t, gid_t gid)
{
	struct group *gr;

	gr = getgrgid(gid);
	if (gr != NULL)
		strlcpy(t->th_buf.gname, gr->gr_name, sizeof(t->th_buf.gname));

	int_to_oct(gid, t->th_buf.gid, 8);
}


/* encode file mode */
void
th_set_mode(TAR *t, mode_t fmode)
{
	if (S_ISSOCK(fmode))
	{
		fmode &= ~S_IFSOCK;
		fmode |= S_IFIFO;
	}
	int_to_oct(fmode, (t)->th_buf.mode, 8);
}


void
th_set_from_stat(TAR *t, struct stat *s)
{
	th_set_type(t, s->st_mode);
	if (S_ISCHR(s->st_mode) || S_ISBLK(s->st_mode))
		th_set_device(t, s->st_rdev);
	th_set_user(t, s->st_uid);
	th_set_group(t, s->st_gid);
	th_set_mode(t, s->st_mode);
	th_set_mtime(t, s->st_mtime);
	if (S_ISREG(s->st_mode))
		th_set_size(t, s->st_size);
	else
		th_set_size(t, 0);
}

/***** extract.c ***********************************************************/

static int
tar_set_file_perms(TAR *t, char *realname)
{
	mode_t mode;
	uid_t uid;
	gid_t gid;
	struct utimbuf ut;
	char *filename;

	filename = (realname ? realname : th_get_pathname(t));
	mode = th_get_mode(t);
	uid = th_get_uid(t);
	gid = th_get_gid(t);
	ut.modtime = ut.actime = th_get_mtime(t);

	/* change owner/group */
	if (geteuid() == 0)
#ifdef HAVE_LCHOWN
		if (lchown(filename, uid, gid) == -1)
		{
# ifdef DEBUG
			fprintf(stderr, "lchown(\"%s\", %d, %d): %s\n",
				filename, uid, gid, strerror(errno));
# endif
#else /* ! HAVE_LCHOWN */
		if (!TH_ISSYM(t) && chown(filename, uid, gid) == -1)
		{
# ifdef DEBUG
			fprintf(stderr, "chown(\"%s\", %d, %d): %s\n",
				filename, uid, gid, strerror(errno));
# endif
#endif /* HAVE_LCHOWN */
			return -1;
		}

	/* change access/modification time */
	if (!TH_ISSYM(t) && utime(filename, &ut) == -1)
	{
#ifdef DEBUG
		perror("utime()");
#endif
		return -1;
	}

	/* change permissions */
	if (!TH_ISSYM(t) && chmod(filename, mode) == -1)
	{
#ifdef DEBUG
		perror("chmod()");
#endif
		return -1;
	}

	return 0;
}


/* switchboard */
int
tar_extract_file(TAR *t, char *realname)
{
	int i;
	char *lnp;
	int pathname_len;
	int realname_len;

	if (t->options & TAR_NOOVERWRITE)
	{
		struct stat s;

		if (lstat(realname, &s) == 0 || errno != ENOENT)
		{
			errno = EEXIST;
			return -1;
		}
	}

	if (TH_ISDIR(t))
	{
		i = tar_extract_dir(t, realname);
		if (i == 1)
			i = 0;
	}
	else if (TH_ISLNK(t))
		i = tar_extract_hardlink(t, realname);
	else if (TH_ISSYM(t))
		i = tar_extract_symlink(t, realname);
	else if (TH_ISCHR(t))
		i = tar_extract_chardev(t, realname);
	else if (TH_ISBLK(t))
		i = tar_extract_blockdev(t, realname);
	else if (TH_ISFIFO(t))
		i = tar_extract_fifo(t, realname);
	else /* if (TH_ISREG(t)) */
		i = tar_extract_regfile(t, realname);

	if (i != 0)
		return i;

	i = tar_set_file_perms(t, realname);
	if (i != 0)
		return i;

	pathname_len = strlen(th_get_pathname(t)) + 1;
	realname_len = strlen(realname) + 1;
	lnp = (char *)calloc(1, pathname_len + realname_len);
	if (lnp == NULL)
		return -1;
	strcpy(&lnp[0], th_get_pathname(t));
	strcpy(&lnp[pathname_len], realname);
#ifdef DEBUG
	printf("tar_extract_file(): calling libtarmod_hash_add(): key=\"%s\", "
	       "value=\"%s\"\n", th_get_pathname(t), realname);
#endif
	if (libtarmod_hash_add(t->h, lnp) != 0)
		return -1;

	return 0;
}


/* extract regular file */
int
tar_extract_regfile_blocks(TAR *t, void *fdout, size_t size, writefunc_t outfunc)
{
	char buf[T_BLOCKSIZE];

	/* extract the file */
	for (int i = size; i > 0; i -= T_BLOCKSIZE)
	{
		int k = tar_block_read(t, buf);
		if (k != T_BLOCKSIZE)
			return -1;

		/* write block to output file */
		if (outfunc(t->opaque, fdout, buf,
			  ((i > T_BLOCKSIZE) ? T_BLOCKSIZE : i)) == -1)
			return -1;
	}

	return 0;
}


/* extract regular file */
int
tar_extract_regfile(TAR *t, char *realname)
{
	mode_t mode;
	size_t size;
	uid_t uid;
	gid_t gid;
	void *fdout;
	char *filename;

#ifdef DEBUG
	printf("==> tar_extract_regfile(t=0x%lx, realname=\"%s\")\n", t,
	       realname);
#endif

	if (!TH_ISREG(t))
	{
		errno = EINVAL;
		return -1;
	}

	filename = (realname ? realname : th_get_pathname(t));
	mode = th_get_mode(t);
	size = th_get_size(t);
	uid = th_get_uid(t);
	gid = th_get_gid(t);

	if (mkdirhier(t, dirname(filename)) == -1)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (mode %04o, uid %d, gid %d, %d bytes)\n",
	       filename, mode, uid, gid, size);
#endif
	fdout = t->type->outopenfunc(t->opaque, filename, O_WRONLY | O_CREAT | O_TRUNC
#ifdef O_BINARY
		     | O_BINARY
#endif
		    , 0666);
	if (fdout == (void *)-1)
	{
#ifdef DEBUG
		perror("open()");
#endif
		return -1;
	}

#if 0
	/* change the owner.  (will only work if run as root) */
	if (fchown(fdout, uid, gid) == -1 && errno != EPERM)
	{
#ifdef DEBUG
		perror("fchown()");
#endif
		return -1;
	}

	/* make sure the mode isn't inheritted from a file we're overwriting */
	if (fchmod(fdout, mode & 07777) == -1)
	{
#ifdef DEBUG
		perror("fchmod()");
#endif
		return -1;
	}
#endif

	/* extract the file */
	int k = tar_extract_regfile_blocks(t, fdout, size, t->type->outwritefunc);
	if (k != 0)
		errno = EINVAL;

	/* close output file */
	if (t->type->outclosefunc(t->opaque, fdout) == -1)
		return -1;

#ifdef DEBUG
	printf("### done extracting %s\n", filename);
#endif

	return 0;
}


/* skip regfile */
int
tar_skip_regfile(TAR *t)
{
	int i;
	size_t size;
	char buf[T_BLOCKSIZE];

	if (!TH_ISREG(t))
	{
		errno = EINVAL;
		return -1;
	}

	uint64_t posStart = t->type->seekfunc(t->opaque, t->fd, 0, SEEK_CUR);

	size = th_get_size(t);
	int blockRemainder = (size % T_BLOCKSIZE) > 0;
	size_t roundUpToBlock = (size / T_BLOCKSIZE + blockRemainder) * T_BLOCKSIZE;
	uint64_t pos = t->type->seekfunc(t->opaque, t->fd, roundUpToBlock, SEEK_CUR);

	if(posStart == (uint64_t)-1 || pos == (uint64_t)-1)
	{
		errno = EINVAL;
		return -1;
	}

	if (pos != posStart + roundUpToBlock)
	{
		errno = EINVAL;
		return -1;
	}

	return 0;
}


/* hardlink */
int
tar_extract_hardlink(TAR * t, char *realname)
{
	char *filename;
	char *linktgt = NULL;
	char *lnp;
	libtarmod_hashptr_t hp;

	if (!TH_ISLNK(t))
	{
		errno = EINVAL;
		return -1;
	}

	filename = (realname ? realname : th_get_pathname(t));
	if (mkdirhier(t, dirname(filename)) == -1)
		return -1;
	libtarmod_hashptr_reset(&hp);
	if (libtarmod_hash_getkey(t->h, &hp, th_get_linkname(t),
			       (libtarmod_matchfunc_t)libtarmod_str_match) != 0)
	{
		lnp = (char *)libtarmod_hashptr_data(&hp);
		linktgt = &lnp[strlen(lnp) + 1];
	}
	else
		linktgt = th_get_linkname(t);

#ifdef DEBUG
	printf("  ==> extracting: %s (link to %s)\n", filename, linktgt);
#endif
	if (link(linktgt, filename) == -1)
	{
#ifdef DEBUG
		perror("link()");
#endif
		return -1;
	}

	return 0;
}


/* symlink */
int
tar_extract_symlink(TAR *t, char *realname)
{
	char *filename;

	if (!TH_ISSYM(t))
	{
		errno = EINVAL;
		return -1;
	}

	filename = (realname ? realname : th_get_pathname(t));
	if (mkdirhier(t, dirname(filename)) == -1)
		return -1;

	if (unlink(filename) == -1 && errno != ENOENT)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (symlink to %s)\n",
	       filename, th_get_linkname(t));
#endif
	if (symlink(th_get_linkname(t), filename) == -1)
	{
#ifdef DEBUG
		perror("symlink()");
#endif
		return -1;
	}

	return 0;
}


/* character device */
int
tar_extract_chardev(TAR *t, char *realname)
{
	mode_t mode;
	unsigned long devmaj, devmin;
	char *filename;

	if (!TH_ISCHR(t))
	{
		errno = EINVAL;
		return -1;
	}

	filename = (realname ? realname : th_get_pathname(t));
	mode = th_get_mode(t);
	devmaj = th_get_devmajor(t);
	devmin = th_get_devminor(t);

	if (mkdirhier(t, dirname(filename)) == -1)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (character device %ld,%ld)\n",
	       filename, devmaj, devmin);
#endif
	if (mknod(filename, mode | S_IFCHR,
		  makedev(devmaj, devmin)) == -1)
	{
#ifdef DEBUG
		perror("mknod()");
#endif
		return -1;
	}

	return 0;
}


/* block device */
int
tar_extract_blockdev(TAR *t, char *realname)
{
	mode_t mode;
	unsigned long devmaj, devmin;
	char *filename;

	if (!TH_ISBLK(t))
	{
		errno = EINVAL;
		return -1;
	}

	filename = (realname ? realname : th_get_pathname(t));
	mode = th_get_mode(t);
	devmaj = th_get_devmajor(t);
	devmin = th_get_devminor(t);

	if (mkdirhier(t, dirname(filename)) == -1)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (block device %ld,%ld)\n",
	       filename, devmaj, devmin);
#endif
	if (mknod(filename, mode | S_IFBLK,
		  makedev(devmaj, devmin)) == -1)
	{
#ifdef DEBUG
		perror("mknod()");
#endif
		return -1;
	}

	return 0;
}


/* directory */
int
tar_extract_dir(TAR *t, char *realname)
{
	mode_t mode;
	char *filename;

	if (!TH_ISDIR(t))
	{
		errno = EINVAL;
		return -1;
	}

	filename = (realname ? realname : th_get_pathname(t));
	mode = th_get_mode(t);

	if (mkdirhier(t, dirname(filename)) == -1)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (mode %04o, directory)\n", filename,
	       mode);
#endif
	if (t->type->mkdirfunc(t->opaque, filename, mode) == -1)
	{
		if (errno == EEXIST)
		{
			if (chmod(filename, mode) == -1)
			{
#ifdef DEBUG
				perror("chmod()");
#endif
				return -1;
			}
			else
			{
#ifdef DEBUG
				puts("  *** using existing directory");
#endif
				return 1;
			}
		}
		else
		{
#ifdef DEBUG
			perror("mkdir()");
#endif
			return -1;
		}
	}

	return 0;
}


/* FIFO */
int
tar_extract_fifo(TAR *t, char *realname)
{
	mode_t mode;
	char *filename;

	if (!TH_ISFIFO(t))
	{
		errno = EINVAL;
		return -1;
	}

	filename = (realname ? realname : th_get_pathname(t));
	mode = th_get_mode(t);

	if (mkdirhier(t, dirname(filename)) == -1)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (fifo)\n", filename);
#endif
	if (mkfifo(filename, mode) == -1)
	{
#ifdef DEBUG
		perror("mkfifo()");
#endif
		return -1;
	}

	return 0;
}

/***** output.c ************************************************************/


#ifndef _POSIX_LOGIN_NAME_MAX
# define _POSIX_LOGIN_NAME_MAX	9
#endif


void
th_print(TAR *t)
{
	puts("\nPrinting tar header:");
	printf("  name     = \"%.100s\"\n", t->th_buf.name);
	printf("  mode     = \"%.8s\"\n", t->th_buf.mode);
	printf("  uid      = \"%.8s\"\n", t->th_buf.uid);
	printf("  gid      = \"%.8s\"\n", t->th_buf.gid);
	printf("  size     = \"%.12s\"\n", t->th_buf.size);
	printf("  mtime    = \"%.12s\"\n", t->th_buf.mtime);
	printf("  chksum   = \"%.8s\"\n", t->th_buf.chksum);
	printf("  typeflag = \'%c\'\n", t->th_buf.typeflag);
	printf("  linkname = \"%.100s\"\n", t->th_buf.linkname);
	printf("  magic    = \"%.6s\"\n", t->th_buf.magic);
	/*printf("  version  = \"%.2s\"\n", t->th_buf.version); */
	printf("  version[0] = \'%c\',version[1] = \'%c\'\n",
	       t->th_buf.version[0], t->th_buf.version[1]);
	printf("  uname    = \"%.32s\"\n", t->th_buf.uname);
	printf("  gname    = \"%.32s\"\n", t->th_buf.gname);
	printf("  devmajor = \"%.8s\"\n", t->th_buf.devmajor);
	printf("  devminor = \"%.8s\"\n", t->th_buf.devminor);
	printf("  prefix   = \"%.155s\"\n", t->th_buf.prefix);
	printf("  padding  = \"%.12s\"\n", t->th_buf.padding);
	printf("  gnu_longname = \"%s\"\n",
	       (t->th_buf.gnu_longname ? t->th_buf.gnu_longname : "[NULL]"));
	printf("  gnu_longlink = \"%s\"\n",
	       (t->th_buf.gnu_longlink ? t->th_buf.gnu_longlink : "[NULL]"));
}


void
th_print_long_ls(TAR *t)
{
	char modestring[12];
	struct passwd *pw;
	struct group *gr;
	uid_t uid;
	gid_t gid;
	char username[_POSIX_LOGIN_NAME_MAX];
	char groupname[_POSIX_LOGIN_NAME_MAX];
	time_t mtime;
	struct tm *mtm;

#ifdef HAVE_STRFTIME
	char timebuf[18];
#else
	const char *months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
#endif

	uid = th_get_uid(t);
	pw = getpwuid(uid);
	if (pw == NULL)
		snprintf(username, sizeof(username), "%d", uid);
	else
		strlcpy(username, pw->pw_name, sizeof(username));

	gid = th_get_gid(t);
	gr = getgrgid(gid);
	if (gr == NULL)
		snprintf(groupname, sizeof(groupname), "%d", gid);
	else
		strlcpy(groupname, gr->gr_name, sizeof(groupname));

	strmode(th_get_mode(t), modestring);
	printf("%.10s %-8.8s %-8.8s ", modestring, username, groupname);

	if (TH_ISCHR(t) || TH_ISBLK(t))
		printf(" %3d, %3d ", th_get_devmajor(t), th_get_devminor(t));
	else
		printf("%9ld ", (long)th_get_size(t));

	mtime = th_get_mtime(t);
	mtm = localtime(&mtime);
#ifdef HAVE_STRFTIME
	strftime(timebuf, sizeof(timebuf), "%h %e %H:%M %Y", mtm);
	printf("%s", timebuf);
#else
	printf("%.3s %2d %2d:%02d %4d",
	       months[mtm->tm_mon],
	       mtm->tm_mday, mtm->tm_hour, mtm->tm_min, mtm->tm_year + 1900);
#endif

	printf(" %s", th_get_pathname(t));

	if (TH_ISSYM(t) || TH_ISLNK(t))
	{
		if (TH_ISSYM(t))
			printf(" -> ");
		else
			printf(" link to ");
		if ((t->options & TAR_GNU) && t->th_buf.gnu_longlink != NULL)
			printf("%s", t->th_buf.gnu_longlink);
		else
			printf("%.100s", t->th_buf.linkname);
	}

	putchar('\n');
}

/***** util.c *************************************************************/


/* hashing function for pathnames */
int
path_hashfunc(char *key, int numbuckets)
{
	char buf[MAXPATHLEN];
	char *p;

	strcpy(buf, key);
	p = basename(buf);

	return (((unsigned int)p[0]) % numbuckets);
}


/* matching function for dev_t's */
int
dev_match(dev_t *dev1, dev_t *dev2)
{
	return !memcmp(dev1, dev2, sizeof(dev_t));
}


/* matching function for ino_t's */
int
ino_match(ino_t *ino1, ino_t *ino2)
{
	return !memcmp(ino1, ino2, sizeof(ino_t));
}


/* hashing function for dev_t's */
int
dev_hash(dev_t *dev)
{
	return *dev % 16;
}


/* hashing function for ino_t's */
int
ino_hash(ino_t *inode)
{
	return *inode % 256;
}


/*
** mkdirhier() - create all directories in a given path
** returns:
**	0			success
**	1			all directories already exist
**	-1 (and sets errno)	error
*/
int
mkdirhier(TAR *t, char *path)
{
	char src[MAXPATHLEN], dst[MAXPATHLEN] = "";
	char *dirp, *nextp = src;
	int retval = 1;

	if (strlcpy(src, path, sizeof(src)) > sizeof(src))
	{
		errno = ENAMETOOLONG;
		return -1;
	}

	if (path[0] == '/')
		strcpy(dst, "/");

	while ((dirp = strsep(&nextp, "/")) != NULL)
	{
		if (*dirp == '\0')
			continue;

		if (dst[0] != '\0')
			strcat(dst, "/");
		strcat(dst, dirp);

		if (t->type->mkdirfunc(t->opaque, dst, 0777) == -1)
		{
			if (errno != EEXIST)
				return -1;
		}
		else
			retval = 0;
	}

	return retval;
}


/* calculate header checksum */
int
th_crc_calc(TAR *t)
{
	int i, sum = 0;

	for (i = 0; i < T_BLOCKSIZE; i++)
		sum += ((unsigned char *)(&(t->th_buf)))[i];
	for (i = 0; i < 8; i++)
		sum += (' ' - (unsigned char)t->th_buf.chksum[i]);

	return sum;
}


/* calculate a signed header checksum */
int
th_signed_crc_calc(TAR *t)
{
	int i, sum = 0;

	for (i = 0; i < T_BLOCKSIZE; i++)
		sum += ((signed char *)(&(t->th_buf)))[i];
	for (i = 0; i < 8; i++)
		sum += (' ' - (signed char)t->th_buf.chksum[i]);

	return sum;
}


/* string-octal to integer conversion */
int
oct_to_int(char *oct)
{
	int i;

	return sscanf(oct, "%o", &i) == 1 ? i : 0;
}


/* integer to string-octal conversion, no NULL */
void
int_to_oct_nonull(int num, char *oct, size_t octlen)
{
	snprintf(oct, octlen, "%*lo", octlen - 1, (unsigned long)num);
	oct[octlen - 1] = ' ';
}

/***** wrapper.c **********************************************************/


int
tar_extract_glob(TAR *t, char *globname, char *prefix)
{
	char *filename;
	char buf[MAXPATHLEN];
	int i;

	while ((i = th_read(t)) == 0)
	{
		filename = th_get_pathname(t);
		if (fnmatch(globname, filename, FNM_PATHNAME | FNM_PERIOD))
		{
			if (TH_ISREG(t) && tar_skip_regfile(t))
				return -1;
			continue;
		}
		if (t->options & TAR_VERBOSE)
			th_print_long_ls(t);
		if (prefix != NULL)
			snprintf(buf, sizeof(buf), "%s/%s", prefix, filename);
		else
			strlcpy(buf, filename, sizeof(buf));
		if (tar_extract_file(t, buf) != 0)
			return -1;
	}

	return (i == 1 ? 0 : -1);
}


int
tar_extract_all(TAR *t, char *prefix)
{
	char *filename;
	char buf[MAXPATHLEN];
	int i;

#ifdef DEBUG
	printf("==> tar_extract_all(TAR *t, \"%s\")\n",
	       (prefix ? prefix : "(null)"));
#endif

	while ((i = th_read(t)) == 0)
	{
#ifdef DEBUG
		puts("    tar_extract_all(): calling th_get_pathname()");
#endif
		filename = th_get_pathname(t);
		if (t->options & TAR_VERBOSE)
			th_print_long_ls(t);
		if (prefix != NULL)
			snprintf(buf, sizeof(buf), "%s/%s", prefix, filename);
		else
			strlcpy(buf, filename, sizeof(buf));
#ifdef DEBUG
		printf("    tar_extract_all(): calling tar_extract_file(t, "
		       "\"%s\")\n", buf);
#endif
		if (tar_extract_file(t, buf) != 0)
			return -1;
	}

	return (i == 1 ? 0 : -1);
}


int
tar_append_tree(TAR *t, char *realdir, char *savedir)
{
	char realpath[MAXPATHLEN];
	char savepath[MAXPATHLEN];
	struct dirent *dent;
	DIR *dp;
	struct stat s;

#ifdef DEBUG
	printf("==> tar_append_tree(0x%lx, \"%s\", \"%s\")\n",
	       t, realdir, (savedir ? savedir : "[NULL]"));
#endif

	if (tar_append_file(t, realdir, savedir) != 0)
		return -1;

#ifdef DEBUG
	puts("    tar_append_tree(): done with tar_append_file()...");
#endif

	dp = opendir(realdir);
	if (dp == NULL)
	{
		if (errno == ENOTDIR)
			return 0;
		return -1;
	}
	while ((dent = readdir(dp)) != NULL)
	{
		if (strcmp(dent->d_name, ".") == 0 ||
		    strcmp(dent->d_name, "..") == 0)
			continue;

		snprintf(realpath, MAXPATHLEN, "%s/%s", realdir,
			 dent->d_name);
		if (savedir)
			snprintf(savepath, MAXPATHLEN, "%s/%s", savedir,
				 dent->d_name);

		if (lstat(realpath, &s) != 0)
			return -1;

		if (S_ISDIR(s.st_mode))
		{
			if (tar_append_tree(t, realpath,
					    (savedir ? savepath : NULL)) != 0)
				return -1;
			continue;
		}

		if (tar_append_file(t, realpath,
				    (savedir ? savepath : NULL)) != 0)
			return -1;
	}

	closedir(dp);

	return 0;
}





