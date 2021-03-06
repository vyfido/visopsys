/**
 * ntfsresize - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2005 Szabolcs Szakacsits
 * Copyright (c) 2002-2005 Anton Altaparmakov
 * Copyright (c) 2002-2003 Richard Russon
 *
 * This utility will resize an NTFS volume without data loss.
 *
 * WARNING FOR DEVELOPERS!!! Several external tools grep for text messages
 * to control execution thus if you would like to change any message
 * then PLEASE think twice before doing so then don't modify it. Thanks!
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * Heavily modified 01/2006 by Andy McLaughlin for Visopsys port.
 */

#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <sys/ntfs.h>
#include <sys/api.h>

#include "debug.h"
#include "types.h"
#include "support.h"
#include "endians.h"
#include "bootsect.h"
#include "device.h"
#include "attrib.h"
#include "volume.h"
#include "mft.h"
#include "bitmap.h"
#include "inode.h"
#include "runlist.h"
#include "utils.h"

static const char *invalid_ntfs_msg =
"The device doesn't have a valid NTFS.";

static const char *corrupt_volume_msg =
"This software has detected that your NTFS is corrupted.\n"
"No modification was made to NTFS.";

static const char *hibernated_volume_msg =
"The NTFS partition is hibernated. Windows must be resumed and\n"
"turned off properly, so resizing could be done safely.";

static const char *unclean_journal_msg =
"The NTFS journal file is unclean.";

static const char *bad_sectors_warning_msg =
"WARNING: The disk has bad sector.";

struct {
	int force;
	int info;
	int badsectors;
	s64 bytes;
	char *volume;
} opt;

struct bitmap {
	s64 size;
	u8 *bm;
	u8 padding[4];		/* Unused: padding to 64 bit. */
};

struct llcn_t {
	s64 lcn;	/* last used LCN for a "special" file/attr type */
	s64 inode;	/* inode using it */
};

typedef struct {
	ntfs_inode *ni;		     /* inode being processed */
	ntfs_attr_search_ctx *ctx;   /* inode attribute being processed */
	s64 inuse;		     /* num of clusters in use */
	int multi_ref;		     /* num of clusters referenced many times */
	int outsider;		     /* num of clusters outside the volume */
	int show_outsider;	     /* controls showing the above information */
	int flags;
	struct bitmap lcn_bitmap;
} ntfsck_t;

typedef struct {
	ntfs_volume *vol;
	ntfs_inode *ni;		     /* inode being processed */
	s64 new_volume_size;	     /* in clusters; 0 = --info w/o --size */
	MFT_REF mref;                /* mft reference */
	MFT_RECORD *mrec;            /* mft record */
	ntfs_attr_search_ctx *ctx;   /* inode attribute being processed */
	u64 relocations;	     /* num of clusters to relocate */
	s64 inuse;		     /* num of clusters in use */
	runlist mftmir_rl;	     /* $MFTMirr AT_DATA's new position */
	s64 mftmir_old;		     /* $MFTMirr AT_DATA's old LCN */
	int dirty_inode;	     /* some inode data got relocated */
	int shrink;		     /* shrink = 1, enlarge = 0 */
	s64 badclusters;	     /* num of physically dead clusters */
	VCN mft_highest_vcn;	     /* used for relocating the $MFT */
        progress *prog;
	struct bitmap lcn_bitmap;
	/* Temporary statistics until all case is supported */
	struct llcn_t last_mft;
	struct llcn_t last_mftmir;
	struct llcn_t last_multi_mft;
	struct llcn_t last_sparse;
	struct llcn_t last_compressed;
	struct llcn_t last_lcn;
	s64 last_unsupp;	     /* last unsupported cluster */
} ntfs_resize_t;

/* FIXME: This, lcn_bitmap and pos from find_free_cluster() will make a cluster
   allocation related structure, attached to ntfs_resize_t */
s64 max_free_cluster_range = 0;

#define NTFS_MBYTE (1000 * 1000)

/* WARNING: don't modify the text, external tools grep for it */
#define ERR_PREFIX   "ERROR: "

#define DIRTY_NONE		(0)
#define DIRTY_INODE		(1)
#define DIRTY_ATTRIB		(2)

#define NTFS_MAX_CLUSTER_SIZE 	(65536)

static int progressPercentages[] = {
  25, // RSZPCNT_CHECK
  2,  // RSZPCNT_ACCOUNTING
  23, // RSZPCNT_COLRSZCONST
  25, // RSZPCNT_VOLFIXUP
  3,  // RSZPCNT_RELOCATIONS
  3,  // RSZPCNT_BADCLUST
  3,  // RSZPCNT_TRUNCBMP
  3,  // RSZPCNT_UPDBOOTSECT
  3   // RSZPCNT_SYNC
};

#define RSZPCNT_CHECK        0
#define RSZPCNT_ACCOUNTING   1
#define RSZPCNT_COLRSZCONST  2
#define RSZPCNT_VOLFIXUP     3
#define RSZPCNT_RELOCATIONS  4
#define RSZPCNT_BADCLUST     5
#define RSZPCNT_TRUNCBMP     6
#define RSZPCNT_UPDBOOTSECT  7
#define RSZPCNT_SYNC         8

#define CHECK_CANCEL() do {                   \
  if (resize->prog && resize->prog->cancel)   \
    { free(resize); return (ERR_CANCELLED); } \
} while (0)

static s64 rounded_up_division(s64 numer, s64 denom)
{
	return (numer + (denom - 1)) / denom;
}

static void progress_message(ntfs_resize_t *resize, const char *fmt, ...)
		__attribute__((format(printf, 2, 3)));
static void progress_message(ntfs_resize_t *resize, const char *fmt, ...)
{
	va_list ap;
	char tmp[PROGRESS_MAX_MESSAGELEN];

	va_start(ap, fmt);
	vsprintf(tmp, fmt, ap);
	va_end(ap);

	if (resize->prog && (lockGet(&(resize->prog->lock)) >= 0))
	  {
	    strncpy(resize->prog->statusMessage, tmp, PROGRESS_MAX_MESSAGELEN);
	    lockRelease(&(resize->prog->lock));
	  }

	Dprintf("%s\n", tmp);
}

static void _err_printf(ntfs_resize_t *resize, const char *function,
			const char *fmt, ...)
		__attribute__((format(printf, 3, 4)));
static void _err_printf(ntfs_resize_t *resize, const char *function,
			const char *fmt, ...)
{
	va_list ap;
	char tmp[PROGRESS_MAX_MESSAGELEN];

	strcpy(tmp, ERR_PREFIX);

	if (function)
	  sprintf((tmp + strlen(tmp)), "%s: ", function);

	va_start(ap, fmt);
	vsprintf((tmp + strlen(tmp)), fmt, ap);
	va_end(ap);

	if (resize->prog && (lockGet(&(resize->prog->lock)) >= 0))
	  {
	    strncpy(resize->prog->statusMessage, tmp, PROGRESS_MAX_MESSAGELEN);
	    resize->prog->confirmError = 0;
	    resize->prog->error = 1;

	    lockRelease(&(resize->prog->lock));

	    while (!(resize->prog->confirmError))
	      multitaskerYield();
	  }

	Dprintf("%s\n", tmp);
}

#ifdef DEBUG
#define err_printf(r, f, a...) _err_printf(r, __FUNCTION__, f, ##a)
#else
#define err_printf(r, f, a...) _err_printf(r, NULL, f, ##a)
#endif

static void rl_set(runlist *rl, VCN vcn, LCN lcn, s64 len)
{
	rl->vcn = vcn;
	rl->lcn = lcn;
	rl->length = len;
}

static int rl_items(runlist *rl)
{
	int i = 0;

	while (rl[i++].length)
		;

	return i;
}

/**
 * nr_clusters_to_bitmap_byte_size
 *
 * Take the number of clusters in the volume and calculate the size of $Bitmap.
 * The size must be always a multiple of 8 bytes.
 */
static s64 nr_clusters_to_bitmap_byte_size(s64 nr_clusters)
{
	s64 bm_bsize;

	bm_bsize = rounded_up_division(nr_clusters, 8);
	bm_bsize = (bm_bsize + 7) & ~7;

	return bm_bsize;
}

static int str2unicode(ntfs_resize_t *resize, const char *aname, ntfschar **ustr, int *len)
{
	if (aname && ((*len = ntfs_mbstoucs(aname, ustr, 0)) == -1)) {
		err_printf(resize, "Couldn't convert string to Unicode");
		return -1;
	}

	if (!*ustr || !*len) {
		*ustr = AT_UNNAMED;
		*len = 0;
	}

	return 0;
}

static int has_bad_sectors(ntfs_resize_t *resize, int is_inode)
{
	int len, ret = 0;
	ntfschar *ustr = NULL;
	ATTR_RECORD *a = resize->ctx->attr;

	if (is_inode) {
		if (resize->ni->mft_no != FILE_BadClus)
			return 0;
	} else {
		if (resize->mref != FILE_BadClus)
			return 0;
	}

	if (str2unicode(resize, "$Bad", &ustr, &len) == -1)
		return -1;

	if (ustr && ntfs_names_are_equal(ustr, len,
			(ntfschar*)((u8*)a + le16_to_cpu(a->name_offset)),
			a->name_length, 0, NULL, 0))
		ret = 1;

	if (ustr != AT_UNNAMED)
		free(ustr);

	return ret;
}

static int collect_resize_constraints(ntfs_resize_t *resize, runlist *rl)
{
	s64 inode, last_lcn;
	ATTR_FLAGS flags;
	ATTR_TYPES atype;
	struct llcn_t *llcn = NULL;
	int ret, supported = 0;

	last_lcn = rl->lcn + (rl->length - 1);

	inode = resize->ni->mft_no;
	flags = resize->ctx->attr->flags;
	atype = resize->ctx->attr->type;

	if ((ret = has_bad_sectors(resize, 1)) != 0) {
		if (ret == -1)
		  {
			err_printf(resize, "Can't read $Bad clusters file");
			return (-1);
		  }
		if (NInoAttrList(resize->ni))
		  {
			err_printf(resize, "Hopelessly many bad sectors!  Not supported.");
			return (-1);
		  }
		if (resize->badclusters == 0)
			progress_message(resize, "WARNING: The disk has bad sector! "
			       "This can cause reliability problems!");
		resize->badclusters += last_lcn - rl->lcn + 1;
		progress_message(resize, "Bad clusters: %8lld - %lld", rl->lcn, last_lcn);
		return (0);
	}
	
	if (inode == FILE_Bitmap) {
		llcn = &resize->last_lcn;
		if (atype == AT_DATA && NInoAttrList(resize->ni))
		  {
		    err_printf(resize, "Highly fragmented $Bitmap isn't supported yet.");
		    return (-1);
		  }

		supported = 1;

	} else if (inode == FILE_MFT) {
		llcn = &resize->last_mft;
		/*
		 *  First run of $MFT AT_DATA isn't supported yet.
		 */
		if (atype != AT_DATA || rl->vcn)
			supported = 1;

	} else if (NInoAttrList(resize->ni)) {
		llcn = &resize->last_multi_mft;

		if (inode != FILE_MFTMirr)
			supported = 1;

	} else if (flags & ATTR_IS_SPARSE) {
		llcn = &resize->last_sparse;
		supported = 1;

	} else if (flags & ATTR_IS_COMPRESSED) {
		llcn = &resize->last_compressed;
		supported = 1;

	} else if (inode == FILE_MFTMirr) {
		llcn = &resize->last_mftmir;
		supported = 1;

		/* Fragmented $MFTMirr DATA attribute isn't supported yet */
		if (atype == AT_DATA)
			if (rl[1].length != 0 || rl->vcn)
				supported = 0;
	} else {
		llcn = &resize->last_lcn;
		supported = 1;
	}

	if (llcn->lcn < last_lcn) {
		llcn->lcn = last_lcn;
		llcn->inode = inode;
	}

	if (supported)
		return (0);

	if (resize->last_unsupp < last_lcn)
		resize->last_unsupp = last_lcn;
	return (0);
}


static int collect_relocation_info(ntfs_resize_t *resize, runlist *rl)
{
	s64 lcn, lcn_length, start, len, inode;
	s64 new_vol_size;	/* (last LCN on the volume) + 1 */

	lcn = rl->lcn;
	lcn_length = rl->length;
	inode = resize->ni->mft_no;
	new_vol_size = resize->new_volume_size;

	if (lcn + lcn_length <= new_vol_size)
		return (0);

	if (inode == FILE_Bitmap && resize->ctx->attr->type == AT_DATA)
		return (0);
	
	start = lcn;
	len = lcn_length;

	if (lcn < new_vol_size) {
		start = new_vol_size;
		len = lcn_length - (new_vol_size - lcn);

		if (!opt.info && (inode == FILE_MFTMirr)) {
			err_printf(resize, "$MFTMirr can't be split up yet.  "
				   "Please try a different size.");
			return (-1);
		}
	}

	resize->relocations += len;

	if (!opt.info || !resize->new_volume_size)
		return (0);

	progress_message(resize, "Relocation needed for inode %lld", (long long)inode);
	return (0);
}

/**
 * build_lcn_usage_bitmap
 *
 * lcn_bitmap has one bit for each cluster on the disk.  Initially, lcn_bitmap
 * has no bits set.  As each attribute record is read the bits in lcn_bitmap are
 * checked to ensure that no other file already references that cluster.
 *
 * This serves as a rudimentary "chkdsk" operation.
 */
static int build_lcn_usage_bitmap(ntfs_resize_t *resize, ntfs_volume *vol, ntfsck_t *fsck)
{
	s64 inode;
	ATTR_RECORD *a;
	runlist *rl;
	int i, j;
	struct bitmap *lcn_bitmap = &fsck->lcn_bitmap;

	a = fsck->ctx->attr;
	inode = fsck->ni->mft_no;

	if (!a->non_resident)
		return (0);

	if (!(rl = ntfs_mapping_pairs_decompress(vol, a, NULL))) {
		if (errno == EIO)
			err_printf(resize, "ntfs_decompress_mapping_pairs: %s", corrupt_volume_msg);
		return (-1);
	}


	for (i = 0; rl[i].length; i++) {
		s64 lcn = rl[i].lcn;
		s64 lcn_length = rl[i].length;

		/* CHECKME: LCN_RL_NOT_MAPPED check isn't needed */
		if (lcn == LCN_HOLE || lcn == LCN_RL_NOT_MAPPED)
			continue;

		/* FIXME: ntfs_mapping_pairs_decompress should return error */
		if (lcn < 0 || lcn_length <= 0)
		  {
			err_printf(resize, "Corrupt runlist in inode %lld attr %x LCN "
				 "%llx length %llx", inode,
				 (unsigned int)le32_to_cpu(a->type), lcn,
				 lcn_length);
			return (-1);
		  }

		for (j = 0; j < lcn_length; j++) {

			u64 k = (u64)lcn + j;

			if (k >= (u64)vol->nr_clusters) {

				long long outsiders = lcn_length - j;

				fsck->outsider += outsiders;

				if (++fsck->show_outsider <= 10)
					progress_message(resize, "Outside of the volume reference"
					       " for inode %lld at %lld:%lld",
					       inode, (long long)k, outsiders);

				break;
			}

			if (ntfs_bit_get_and_set(lcn_bitmap->bm, k, 1)) {
				if (++fsck->multi_ref <= 10)
					progress_message(resize, "Cluster %lld is referenced "
					       "multiple times!",
					       (long long)k);
				continue;
			}
		}
		fsck->inuse += lcn_length;
	}
	free(rl);
	return (0);
}


static ntfs_attr_search_ctx *attr_get_search_ctx(ntfs_resize_t *resize, ntfs_inode *ni, MFT_RECORD *mrec)
{
	ntfs_attr_search_ctx *ret;

	if ((ret = ntfs_attr_get_search_ctx(ni, mrec)) == NULL)
		err_printf(resize, "ntfs_attr_get_search_ctx failed");

	return ret;
}

/**
 * walk_attributes
 *
 * For a given MFT Record, iterate through all its attributes.  Any non-resident
 * data runs will be marked in lcn_bitmap.
 */
static int walk_attributes(ntfs_resize_t *resize, ntfs_volume *vol, ntfsck_t *fsck)
{
	if (!(fsck->ctx = attr_get_search_ctx(resize, fsck->ni, NULL)))
		return -1;

	while (!ntfs_attrs_walk(fsck->ctx)) {
		if (fsck->ctx->attr->type == AT_END)
			break;
		if (build_lcn_usage_bitmap(resize, vol, fsck) != 0)
		  return -1;
	}

	ntfs_attr_put_search_ctx(fsck->ctx);
	return 0;
}

/**
 * progress_update
 *
 * Update the progress bar and tell the user.
 */
static void progress_update(progress *prog, int myPercentIndex, u64 current,
			    u64 total)
{
  unsigned finished = 0;
  int count;

  if (prog && (lockGet(&(prog->lock)) >= 0))
    {
      for (count = 0; count < myPercentIndex; count ++)
	finished += progressPercentages[count];

      finished +=
	(unsigned) ((current * progressPercentages[myPercentIndex]) / total);
      if (finished >= 100)
	finished = 99;

      prog->finished = finished;
      prog->percentFinished = finished;

      lockRelease(&(prog->lock));
    }
}

/**
 * compare_bitmaps
 *
 * Compare two bitmaps.  In this case, $Bitmap as read from the disk and
 * lcn_bitmap which we built from the MFT Records.
 */
static int compare_bitmaps(ntfs_resize_t *resize, ntfs_volume *vol, struct bitmap *a)
{
	s64 i, pos, count;
	int mismatch = 0;
	int backup_boot = 0;
	u8 *bm = NULL;

	progress_message(resize, "Accounting clusters");

	bm = calloc(NTFS_BUF_SIZE, 1);
	if (bm == NULL)
	  {
	    err_printf(resize, "Not enough memory");
	    return (-1);
	  }

	pos = 0;
	while (1) {
		count = ntfs_attr_pread(vol->lcnbmp_na, pos, NTFS_BUF_SIZE, bm);
		if (count == -1)
		  {
			err_printf(resize, "Couldn't get $Bitmap $DATA");
			free(bm);
			return (-1);
		  }

		if (count == 0) {
			if (a->size > pos)
			  {
				err_printf(resize, "$Bitmap size is smaller than expected"
					 " (%lld != %lld)", a->size, pos);
				free(bm);
				return (-1);
			  }
			break;
		}

		for (i = 0; i < count; i++, pos++) {
			s64 cl;  /* current cluster */

			progress_update(resize->prog, RSZPCNT_ACCOUNTING,
					pos, a->size);
			if (resize->prog && resize->prog->cancel)
			  {
			    free(bm);
			    return (0);
			  }
	
			if (a->size <= pos)
				goto done;

			if (a->bm[pos] == bm[i])
				continue;

			for (cl = pos * 8; cl < (pos + 1) * 8; cl++) {
				char bit;

				bit = ntfs_bit_get(a->bm, cl);
				if (bit == ntfs_bit_get(bm, i * 8 + cl % 8))
					continue;

				if (!mismatch && !bit && !backup_boot &&
						cl == vol->nr_clusters / 2) {
					/* FIXME: call also boot sector check */
					backup_boot = 1;
					progress_message(resize, "Found backup boot sector in "
					       "the middle of the volume.");
					continue;
				}

				if (++mismatch > 10)
					continue;

				progress_message(resize, "Cluster accounting failed at %lld "
						"(0x%llx): %s cluster in "
						"$Bitmap", (long long)cl,
						(unsigned long long)cl,
						bit ? "missing" : "extra");
			}
		}
	}
done:
	free(bm);
	if (mismatch) {
		err_printf(resize, "Filesystem check failed!  Total of %d "
			   "cluster accounting mismatches.  %s",
			   mismatch, corrupt_volume_msg);
		return (-1);
	}
	return (0);
}

static int inode_close(ntfs_resize_t *resize, ntfs_inode *ni)
{
	if (ntfs_inode_close(ni)) {
		err_printf(resize, "ntfs_inode_close for inode %llu failed",
			    (unsigned long long)ni->mft_no);
		return -1;
	}
	return 0;
}

/**
 * walk_inodes
 *
 * Read each record in the MFT, skipping the unused ones, and build up a bitmap
 * from all the non-resident attributes.
 */
static int build_allocation_bitmap(ntfs_resize_t *resize, ntfs_volume *vol, ntfsck_t *fsck)
{
	s64 nr_mft_records, inode = 0;
	ntfs_inode *ni;

	nr_mft_records = vol->mft_na->initialized_size >>
			vol->mft_record_size_bits;

	progress_message(resize, "Checking filesystem consistency");

	for (; inode < nr_mft_records; inode++) {

		progress_update(resize->prog, RSZPCNT_CHECK, inode,
				(nr_mft_records - 1));
		if (resize->prog && resize->prog->cancel)
		  return (0);

		if ((ni = ntfs_inode_open(vol, (MFT_REF)inode)) == NULL) {
			/* FIXME: continue only if it make sense, e.g.
			   MFT record not in use based on $MFT bitmap */
			if (errno == EIO || errno == ENOENT)
				continue;
			err_printf(resize, "Reading inode %lld failed", inode);
			return -1;
		}

		if (ni->mrec->base_mft_record)
			goto close_inode;

		fsck->ni = ni;
		if (walk_attributes(resize, vol, fsck) != 0) {
			inode_close(resize, ni);
			return -1;
		}
close_inode:
		if (inode_close(resize, ni) != 0)
			return -1;
	}
	return 0;
}

static int build_resize_constraints(ntfs_resize_t *resize)
{
	s64 i;
	runlist *rl;

	if (!resize->ctx->attr->non_resident)
		return (0);

	if (!(rl = ntfs_mapping_pairs_decompress(resize->vol,
						 resize->ctx->attr, NULL)))
	  {
		err_printf(resize, "ntfs_decompress_mapping_pairs failed");
		return (-1);
	  }

	for (i = 0; rl[i].length; i++) {
		/* CHECKME: LCN_RL_NOT_MAPPED check isn't needed */
		if (rl[i].lcn == LCN_HOLE || rl[i].lcn == LCN_RL_NOT_MAPPED)
			continue;

		if (collect_resize_constraints(resize, rl + i) < 0)
		  return (-1);
		if (resize->shrink)
			if (collect_relocation_info(resize, rl + i) != 0)
			  return (-1);
	}
	free(rl);
	return (0);
}

static int resize_constraints_by_attributes(ntfs_resize_t *resize)
{
	if (!(resize->ctx = attr_get_search_ctx(resize, resize->ni, NULL)))
	  return (-1);

	while (!ntfs_attrs_walk(resize->ctx)) {
		if (resize->ctx->attr->type == AT_END)
			break;
		if (build_resize_constraints(resize) != 0)
		  return (-1);
	}

	ntfs_attr_put_search_ctx(resize->ctx);
	return (0);
}

static int set_resize_constraints(ntfs_resize_t *resize)
{
	s64 nr_mft_records, inode;
	ntfs_inode *ni;

	progress_message(resize, "Collecting resizing constraints");

	nr_mft_records = resize->vol->mft_na->initialized_size >>
			resize->vol->mft_record_size_bits;

	for (inode = 0; inode < nr_mft_records; inode++) {

		ni = ntfs_inode_open(resize->vol, (MFT_REF)inode);
		if (ni == NULL) {
			if (errno == EIO || errno == ENOENT)
				continue;
			err_printf(resize, "Reading inode %lld failed", inode);
			return (-1);
		}

		if (ni->mrec->base_mft_record)
			goto close_inode;

		resize->ni = ni;
		if (resize_constraints_by_attributes(resize) != 0)
		  return (-1);
close_inode:
		if (inode_close(resize, ni) != 0)
			return (-1);

		progress_update(resize->prog, RSZPCNT_COLRSZCONST, inode,
				nr_mft_records);
		if (resize->prog && resize->prog->cancel)
		  return (0);
	}
	return (0);
}

static int rl_fixup(ntfs_resize_t *resize, runlist **rl)
{
	runlist *tmp = *rl;

	if (tmp->lcn == LCN_RL_NOT_MAPPED) {
		s64 unmapped_len = tmp->length;

		progress_message(resize, "Skip unmapped run at the beginning ...");

		if (!tmp->length)
		  {
			err_printf(resize, "Empty unmapped runlist!  Please report!");
			return (-1);
		  }
		(*rl)++;
		for (tmp = *rl; tmp->length; tmp++)
			tmp->vcn -= unmapped_len;
	}

	for (tmp = *rl; tmp->length; tmp++) {
		if (tmp->lcn == LCN_RL_NOT_MAPPED) {
			progress_message(resize, "Skip unmapped run at the end  ...");

			if (tmp[1].length)
			  {
				err_printf(resize, "Unmapped runlist in the middle!  "
					 "Please report!");
				return (-1);
			  }
			tmp->lcn = LCN_ENOENT;
			tmp->length = 0;
		}
	}
	return (0);
}

static int replace_attribute_runlist(ntfs_resize_t *resize, ntfs_volume *vol,
				      ntfs_attr_search_ctx *ctx,
				      runlist *rl)
{
	int mp_size, l;
	void *mp;
	ATTR_RECORD *a = ctx->attr;

	if (rl_fixup(resize, &rl) != 0)
	  return (-1);

	if ((mp_size = ntfs_get_size_for_mapping_pairs(vol, rl, 0)) == -1)
	  {
		err_printf(resize, "ntfs_get_size_for_mapping_pairs failed");
		return (-1);
	  }

	if (a->name_length) {
		u16 name_offs = le16_to_cpu(a->name_offset);
		u16 mp_offs = le16_to_cpu(a->mapping_pairs_offset);

		if (name_offs >= mp_offs)
		  {
			err_printf(resize, "Attribute name is after mapping pairs!  "
				 "Please report!");
			return (-1);
		  }
	}

	/* CHECKME: don't trust mapping_pairs is always the last item in the
	   attribute, instead check for the real size/space */
	l = (int)le32_to_cpu(a->length) - le16_to_cpu(a->mapping_pairs_offset);
	if (mp_size > l) {
		s64 remains_size;
		char *next_attr;

		progress_message(resize, "Enlarging attribute header ...");

		mp_size = (mp_size + 7) & ~7;

		next_attr = (char *)a + le16_to_cpu(a->length);
		l = mp_size - l;

		remains_size = le32_to_cpu(ctx->mrec->bytes_in_use);
		remains_size -= (next_attr - (char *)ctx->mrec);

		if (le32_to_cpu(ctx->mrec->bytes_in_use) + l >
				le32_to_cpu(ctx->mrec->bytes_allocated))
		  {
			err_printf(resize, "Extended record needed (%u > %u), not yet "
				 "supported!  Please try to free less space.",
				 (unsigned int)le32_to_cpu(ctx->mrec->
					bytes_in_use) + l,
				 (unsigned int)le32_to_cpu(ctx->mrec->
					bytes_allocated));
			return (-1);
		  }

		memmove(next_attr + l, next_attr, remains_size);
		ctx->mrec->bytes_in_use = cpu_to_le32(l +
				le32_to_cpu(ctx->mrec->bytes_in_use));
		a->length += l;
	}

	if (!(mp = calloc(1, mp_size)))
	  {
		err_printf(resize, "Couldn't get memory");
		return (-1);
	  }

	if (ntfs_mapping_pairs_build(vol, mp, mp_size, rl, 0, NULL))
	  {
		err_printf(resize, "ntfs_mapping_pairs_build failed");
		return (-1);
	  }

	memmove((u8*)a + le16_to_cpu(a->mapping_pairs_offset), mp, mp_size);

	free(mp);
	return (0);
}

static void set_bitmap_range(struct bitmap *bm, s64 pos, s64 length, u8 bit)
{
	while (length--)
		ntfs_bit_set(bm->bm, pos++, bit);
}

static void set_bitmap_clusters(struct bitmap *bm, runlist *rl, u8 bit)
{
	for (; rl->length; rl++)
		set_bitmap_range(bm, rl->lcn, rl->length, bit);
}

static void release_bitmap_clusters(struct bitmap *bm, runlist *rl)
{
	max_free_cluster_range = 0;
	set_bitmap_clusters(bm, rl, 0);
}

static void set_max_free_zone(s64 length, s64 end, runlist_element *rle)
{
	if (length > rle->length) {
		rle->lcn = end - length;
		rle->length = length;
	}
}

static int find_free_cluster(ntfs_resize_t *resize, struct bitmap *bm,
			     runlist_element *rle,
			     s64 nr_vol_clusters,
			     int hint)
{
	/* FIXME: get rid of this 'static' variable */
	static s64 pos = 0;
	s64 i, items = rle->length;
	s64 free_zone = 0;

	if (pos >= nr_vol_clusters)
		pos = 0;
	if (!max_free_cluster_range)
		max_free_cluster_range = nr_vol_clusters;
	rle->lcn = rle->length = 0;
	if (hint)
		pos = nr_vol_clusters / 2;
	i = pos;

	do {
		if (!ntfs_bit_get(bm->bm, i)) {
			if (++free_zone == items) {
				set_max_free_zone(free_zone, i + 1, rle);
				break;
			}
		} else {
			set_max_free_zone(free_zone, i, rle);
			free_zone = 0;
		}
		if (++i == nr_vol_clusters) {
			set_max_free_zone(free_zone, i, rle);
			i = free_zone = 0;
		}
		if (rle->length == max_free_cluster_range)
			break;
	} while (i != pos);

	if (i)
		set_max_free_zone(free_zone, i, rle);

	if (!rle->lcn) {
		errno = ENOSPC;
		return -1;
	}
	if (rle->length < items && rle->length < max_free_cluster_range) {
		max_free_cluster_range = rle->length;
		progress_message(resize, "Max free range: %lld",
				(long long)max_free_cluster_range);
	}
	pos = rle->lcn + items;
	if (pos == nr_vol_clusters)
		pos = 0;

	set_bitmap_range(bm, rle->lcn, rle->length, 1);
	return 0;
}

static runlist *alloc_cluster(ntfs_resize_t *resize, struct bitmap *bm,
			      s64 items,
			      s64 nr_vol_clusters,
			      int hint)
{
	runlist_element rle;
	runlist *rl = NULL;
	int rl_size, runs = 0;
	s64 vcn = 0;

	if (items <= 0) {
		errno = EINVAL;
		return NULL;
	}

	while (items > 0) {

		if (runs)
			hint = 0;
		rle.length = items;
		if (find_free_cluster(resize, bm, &rle, nr_vol_clusters, hint) == -1)
			return NULL;

		rl_size = (runs + 2) * sizeof(runlist_element);
		if (!(rl = (runlist *)realloc(rl, rl_size)))
			return NULL;

		rl_set(rl + runs, vcn, rle.lcn, rle.length);

		vcn += rle.length;
		items -= rle.length;
		runs++;
	}

	rl_set(rl + runs, vcn, -1LL, 0LL);

	return rl;
}

static int read_all(ntfs_resize_t *resize, struct ntfs_device *dev, void *buf, int count)
{
	int i;

	while (count > 0) {

		i = count;
		if (!NDevReadOnly(dev))
			i = dev->d_ops->read(dev, buf, count);

		if (i < 0) {
			if (errno != EAGAIN && errno != EINTR)
				return -1;
		} else if (i > 0) {
			count -= i;
			buf = i + (char *)buf;
		} else {
			err_printf(resize, "Unexpected end of file!");
			return (-1);
		}
	}
	return 0;
}

static int write_all(struct ntfs_device *dev, void *buf, int count)
{
	int i;

	while (count > 0) {

		i = count;
		if (!NDevReadOnly(dev))
			i = dev->d_ops->write(dev, buf, count);

		if (i < 0) {
			if (errno != EAGAIN && errno != EINTR)
				return -1;
		} else {
			count -= i;
			buf = i + (char *)buf;
		}
	}
	return 0;
}

/**
 * write_mft_record
 *
 * Write an MFT Record back to the disk.  If the read-only command line option
 * was given, this function will do nothing.
 */
static int write_mft_record(ntfs_resize_t *resize, ntfs_volume *v, const MFT_REF mref, MFT_RECORD *buf)
{
	if (ntfs_mft_record_write(v, mref, buf))
	  {
		err_printf(resize, "ntfs_mft_record_write failed");
		return (-1);
	  }
	return 0;
}

static int lseek_to_cluster(ntfs_resize_t *resize, ntfs_volume *vol, s64 lcn)
{
	off_t pos;

	pos = (off_t)(lcn * vol->cluster_size);

	if (vol->dev->d_ops->seek(vol->dev, pos, SEEK_SET) == (off_t)-1)
	  {
		err_printf(resize, "Seek failed to position %lld", lcn);
		return (-1);
	  }
	return (0);
}

static int copy_clusters(ntfs_resize_t *resize, s64 dest, s64 src, s64 len)
{
	s64 i;
	char *buff = NULL; /* overflow checked at mount time */
	ntfs_volume *vol = resize->vol;

	if (!(buff = malloc(NTFS_MAX_CLUSTER_SIZE)))
	  return (-1);

	for (i = 0; i < len; i++) {

		if (lseek_to_cluster(resize, vol, src + i) < 0)
		    goto err_out;

		if (read_all(resize, vol->dev, buff, vol->cluster_size) == -1)
		  {
			err_printf(resize, "read_all failed");
			goto err_out;
		  }

		if (lseek_to_cluster(resize, vol, dest + i) < 0)
		  goto err_out;

		if (write_all(vol->dev, buff, vol->cluster_size) == -1)
		  {
			err_printf(resize, "write_all failed");
			goto err_out;
		  }

		resize->relocations++;
	}

	free(buff);
	return (0);
 err_out:
	free(buff);
	return (-1);
}

static int relocate_clusters(ntfs_resize_t *resize, runlist *dest_rl, s64 src_lcn)
{
	/* collect_shrink_constraints() ensured $MFTMir DATA is one run */
	if (resize->mref == FILE_MFTMirr && resize->ctx->attr->type == AT_DATA) {
		if (!resize->mftmir_old) {
			resize->mftmir_rl.lcn = dest_rl->lcn;
			resize->mftmir_rl.length = dest_rl->length;
			resize->mftmir_old = src_lcn;
		} else {
			err_printf(resize, "Multi-run $MFTMirr.  Please report!");
			return (-1);
		}
	}

	for (; dest_rl->length; src_lcn += dest_rl->length, dest_rl++)
	  {
	    if (copy_clusters(resize, dest_rl->lcn, src_lcn, dest_rl->length) < 0)
	      return (-1);
	  }
	return (0);
}

static int rl_split_run(ntfs_resize_t *resize, runlist **rl, int run, s64 pos)
{
	runlist *rl_new, *rle_new, *rle;
	int items, new_size, size_head, size_tail;
	s64 len_head, len_tail;

	items = rl_items(*rl);
	new_size = (items + 1) * sizeof(runlist_element);
	size_head = run * sizeof(runlist_element);
	size_tail = (items - run - 1) * sizeof(runlist_element);

	if (!(rl_new = (runlist *)malloc(new_size)))
	  {
		err_printf(resize, "malloc failed");
		return (-1);
	  }

	rle_new = rl_new + run;
	rle = *rl + run;

	memmove(rl_new, *rl, size_head);
	memmove(rle_new + 2, rle + 1, size_tail);

	len_tail = rle->length - (pos - rle->lcn);
	len_head = rle->length - len_tail;

	rl_set(rle_new, rle->vcn, rle->lcn, len_head);
	rl_set(rle_new + 1, rle->vcn + len_head, rle->lcn + len_head, len_tail);

	progress_message(resize, "Splitting run at cluster %lld:", (long long)pos);

	free(*rl);
	*rl = rl_new;
	return (0);
}

static int rl_insert_at_run(ntfs_resize_t *resize, runlist **rl, int run, runlist *ins)
{
	int items, ins_items;
	int new_size, size_tail;
	runlist *rle;
	s64 vcn;

	items  = rl_items(*rl);
	ins_items = rl_items(ins) - 1;
	new_size = ((items - 1) + ins_items) * sizeof(runlist_element);
	size_tail = (items - run - 1) * sizeof(runlist_element);

	if (!(*rl = (runlist *)realloc(*rl, new_size)))
	  {
		err_printf(resize, "realloc failed");
		return (-1);
	  }

	rle = *rl + run;

	memmove(rle + ins_items, rle + 1, size_tail);

	for (vcn = rle->vcn; ins->length; rle++, vcn += ins->length, ins++) {
		rl_set(rle, vcn, ins->lcn, ins->length);
	}

	return (0);
}

static int relocate_run(ntfs_resize_t *resize, runlist **rl, int run)
{
	s64 lcn, lcn_length;
	s64 new_vol_size;	/* (last LCN on the volume) + 1 */
	runlist *relocate_rl;	/* relocate runlist to relocate_rl */
	int hint;

	lcn = (*rl + run)->lcn;
	lcn_length = (*rl + run)->length;
	new_vol_size = resize->new_volume_size;

	if (lcn + lcn_length <= new_vol_size)
		return (0);

	if (lcn < new_vol_size) {
		if (rl_split_run(resize, rl, run, new_vol_size) < 0)
		  return (-1);
		return (0);
	}

	hint = (resize->mref == FILE_MFTMirr) ? 1 : 0;
	if (!(relocate_rl = alloc_cluster(resize, &resize->lcn_bitmap, lcn_length,
					  new_vol_size, hint)))
	  {
		err_printf(resize, "Cluster allocation failed for %llu:%lld",
			  resize->mref, lcn_length);
		return (-1);
	  }

	/* FIXME: check $MFTMirr DATA isn't multi-run (or support it) */
	progress_message(resize, "Relocate inode %llu:0x%x:%lld:0x%llx -> 0x%08llx",
			(unsigned long long)resize->mref,
			(unsigned int)le32_to_cpu(resize->ctx->attr->type),
			(long long)lcn_length, (unsigned long long)lcn,
			(unsigned long long)relocate_rl->lcn);

	if (relocate_clusters(resize, relocate_rl, lcn) < 0)
	    return (-1);

	if (rl_insert_at_run(resize, rl, run, relocate_rl) < 0)
	    return (-1);

	/* We don't release old clusters in the bitmap, that area isn't
	   used by the allocator and will be truncated later on */
	free(relocate_rl);

	resize->dirty_inode = DIRTY_ATTRIB;
	return (0);
}

static int relocate_attribute(ntfs_resize_t *resize)
{
	ATTR_RECORD *a;
	runlist *rl;
	int i;

	a = resize->ctx->attr;

	if (!a->non_resident)
		return (0);

	if (!(rl = ntfs_mapping_pairs_decompress(resize->vol, a, NULL)))
	  {
		err_printf(resize, "ntfs_decompress_mapping_pairs failed");
		return (-1);
	  }

	for (i = 0; rl[i].length; i++) {
		s64 lcn = rl[i].lcn;
		s64 lcn_length = rl[i].length;

		if (lcn == LCN_HOLE || lcn == LCN_RL_NOT_MAPPED)
			continue;

		/* FIXME: ntfs_mapping_pairs_decompress should return error */
		if (lcn < 0 || lcn_length <= 0)
		  {
			err_printf(resize, "Corrupt runlist in MTF %llu attr %x LCN "
				 "%llx length %llx", resize->mref,
				 (unsigned int)le32_to_cpu(a->type),
				 lcn, lcn_length);
			return (-1);
		  }

		if (relocate_run(resize, &rl, i) < 0)
		  return (-1);
	}

	if (resize->dirty_inode == DIRTY_ATTRIB) {
	  if (replace_attribute_runlist(resize, resize->vol, resize->ctx, rl) < 0)
	    return (-1);
		resize->dirty_inode = DIRTY_INODE;
	}

	free(rl);
	return (0);
}

static int is_mftdata(ntfs_resize_t *resize)
{
	if (resize->ctx->attr->type != AT_DATA)
		return 0;

	if (resize->mref == 0)
		return 1;
	
	if (  MREF(resize->mrec->base_mft_record) == 0  &&
	    MSEQNO(resize->mrec->base_mft_record) != 0)
		return 1;
	
	return 0;
}

static int handle_mftdata(ntfs_resize_t *resize, int do_mftdata)
{
	ATTR_RECORD *attr = resize->ctx->attr;
	VCN highest_vcn, lowest_vcn;
	
	if (do_mftdata) {
		
		if (!is_mftdata(resize))
			return 0;
		
		highest_vcn = sle64_to_cpu(attr->highest_vcn);
		lowest_vcn  = sle64_to_cpu(attr->lowest_vcn);
		
		if (resize->mft_highest_vcn != highest_vcn)
			return 0;
		
		if (lowest_vcn == 0)
			resize->mft_highest_vcn = lowest_vcn;
		else
			resize->mft_highest_vcn = lowest_vcn - 1;

	} else if (is_mftdata(resize)) {
		
		highest_vcn = sle64_to_cpu(attr->highest_vcn);
		
		if (resize->mft_highest_vcn < highest_vcn)
			resize->mft_highest_vcn = highest_vcn;
		
		return 0;
	}

	return 1;
}

static int relocate_attributes(ntfs_resize_t *resize, int do_mftdata)
{
	int ret;

	if (!(resize->ctx = attr_get_search_ctx(resize, NULL, resize->mrec)))
	  return (-1);

	while (!ntfs_attrs_walk(resize->ctx)) {
		if (resize->ctx->attr->type == AT_END)
			break;
		
		if (handle_mftdata(resize, do_mftdata) == 0)
			continue;
			
		ret = has_bad_sectors(resize, 0);
		if (ret == -1)
		  {
			err_printf(resize, "Can't read $Bad clusters file");
			return (-1);
		  }
		else if (ret == 1)
			continue;

		if (resize->mref == FILE_Bitmap &&
		    resize->ctx->attr->type == AT_DATA)
			continue;

		if (relocate_attribute(resize) < 0)
		  return (-1);
	}

	ntfs_attr_put_search_ctx(resize->ctx);
	return (0);
}

static int relocate_inode(ntfs_resize_t *resize, MFT_REF mref, int do_mftdata)
{
	if (ntfs_file_record_read(resize->vol, mref, &resize->mrec, NULL)) {
		/* FIXME: continue only if it make sense, e.g.
		   MFT record not in use based on $MFT bitmap */
		err_printf(resize, "ntfs_file_record_read failed");
		return (-1);
	}

	if (!(resize->mrec->flags & MFT_RECORD_IN_USE))
		return (0);

	resize->mref = mref;
	resize->dirty_inode = DIRTY_NONE;

	if (relocate_attributes(resize, do_mftdata) != 0)
	  return (-1);

	if (resize->dirty_inode == DIRTY_INODE) {
		if (write_mft_record(resize, resize->vol, mref, resize->mrec))
		  {
			err_printf(resize, "Couldn't update inode %llu", mref);
			return (-1);
		  }
	}
	return (0);
}

static int relocate_inodes(ntfs_resize_t *resize)
{
	s64 nr_mft_records;
	MFT_REF mref;
	VCN highest_vcn;

	progress_message(resize, "Relocating needed data");

	resize->relocations = 0;

	resize->mrec = (MFT_RECORD *)malloc(resize->vol->mft_record_size);
	if (!resize->mrec)
	  {
		err_printf(resize, "malloc failed");
		return (-1);
	  }

	nr_mft_records = resize->vol->mft_na->initialized_size >>
			resize->vol->mft_record_size_bits;

   	for (mref = 0; mref < (MFT_REF)nr_mft_records; mref++)
	  if (relocate_inode(resize, mref, 0) != 0)
	    return (-1);

	while(1) {
		highest_vcn = resize->mft_highest_vcn;
		mref = nr_mft_records;
		do {
		  if (relocate_inode(resize, --mref, 1) != 0)
		    return (-1);
			if (resize->mft_highest_vcn == 0)
				goto done;
		} while (mref);

		if (highest_vcn == resize->mft_highest_vcn)
		  {
			err_printf(resize, "Sanity check failed!  Highest_vcn = %lld.  "
				 "Please report!", highest_vcn);
			return (-1);
		  }
	}
done:
	if (resize->mrec)
		free(resize->mrec);
	return (0);
}

static int rl_expand(ntfs_resize_t *resize, runlist **rl, const VCN last_vcn)
{
	int len;
	runlist *p = *rl;

	len = rl_items(p) - 1;
	if (len <= 0)
	  {
		err_printf(resize, "rl_expand: bad runlist length: %d", len);
		return (-1);
	  }

	if (p[len].vcn > last_vcn)
	  {
		err_printf(resize, "rl_expand: length is already more than requested "
			 "(%lld > %lld)", p[len].vcn, last_vcn);
		return (-1);
	  }

	if (p[len - 1].lcn == LCN_HOLE) {

		p[len - 1].length += last_vcn - p[len].vcn;
		p[len].vcn = last_vcn;

	} else if (p[len - 1].lcn >= 0) {

		p = realloc(*rl, (++len + 1) * sizeof(runlist_element));
		if (!p)
		  {
			err_printf(resize, "rl_expand: realloc failed");
			return (-1);
		  }

		p[len - 1].lcn = LCN_HOLE;
		p[len - 1].length = last_vcn - p[len - 1].vcn;
		rl_set(p + len, last_vcn, LCN_ENOENT, 0LL);
		*rl = p;

	} else {
		err_printf(resize, "rl_expand: bad LCN: %lld", p[len - 1].lcn);
		return (-1);
	}
	return (0);
}

static int rl_truncate(ntfs_resize_t *resize, runlist **rl, const VCN last_vcn)
{
	int len;
	VCN vcn;
	
	len = rl_items(*rl) - 1;
	if (len <= 0)
	  {
		err_printf(resize, "rl_truncate: bad runlist length: %d", len);
		return (-1);
	  }

	vcn = (*rl)[len].vcn;
	
	if (vcn < last_vcn)
	  {
	    if (rl_expand(resize, rl, last_vcn) < 0)
	      return (-1);
	  }
	else if (vcn > last_vcn)
		if (ntfs_rl_truncate(rl, last_vcn) == -1)
		  {
			err_printf(resize, "ntfs_rl_truncate failed");
			return (-1);
		  }
	return (0);
}

/**
 * bitmap_file_data_fixup
 *
 * $Bitmap can overlap the end of the volume. Any bits in this region
 * must be set. This region also encompasses the backup boot sector.
 */
static void bitmap_file_data_fixup(s64 cluster, struct bitmap *bm)
{
	for (; cluster < bm->size << 3; cluster++)
		ntfs_bit_set(bm->bm, (u64)cluster, 1);
}

/**
 * truncate_badclust_bad_attr
 *
 * The metadata file $BadClus needs to be shrunk.
 *
 * FIXME: this function should go away and instead using a generalized
 * "truncate_bitmap_data_attr()"
 */
static int truncate_badclust_bad_attr(ntfs_resize_t *resize)
{
	ATTR_RECORD *a;
	runlist *rl_bad;
	s64 nr_clusters = resize->new_volume_size;
	ntfs_volume *vol = resize->vol;

	a = resize->ctx->attr;
	if (!a->non_resident)
	  {
		/* FIXME: handle resident attribute value */
		err_printf(resize, "Resident attribute in $BadClust isn't supported!");
		return (-1);
	  }

	if (!(rl_bad = ntfs_mapping_pairs_decompress(vol, a, NULL)))
	  {
		err_printf(resize, "ntfs_mapping_pairs_decompress failed");
		return (-1);
	  }

	if (rl_truncate(resize, &rl_bad, nr_clusters) < 0)
	  return (-1);

	a->highest_vcn = cpu_to_le64(nr_clusters - 1LL);
	a->allocated_size = cpu_to_le64(nr_clusters * vol->cluster_size);
	a->data_size = cpu_to_le64(nr_clusters * vol->cluster_size);

	if (replace_attribute_runlist(resize, vol, resize->ctx, rl_bad) < 0)
	  return (-1);

	free(rl_bad);
	return (0);
}

/**
 * realloc_bitmap_data_attr
 *
 * Reallocate the metadata file $Bitmap.  It must be large enough for one bit
 * per cluster of the shrunken volume.  Also it must be a of 8 bytes in size.
 */
static int realloc_bitmap_data_attr(ntfs_resize_t *resize,
				     runlist **rl,
				     s64 nr_bm_clusters)
{
	s64 i;
	ntfs_volume *vol = resize->vol;
	ATTR_RECORD *a = resize->ctx->attr;
	s64 new_size = resize->new_volume_size;
	struct bitmap *bm = &resize->lcn_bitmap;

	if (!(*rl = ntfs_mapping_pairs_decompress(vol, a, NULL)))
	  {
		err_printf(resize, "ntfs_mapping_pairs_decompress failed");
		return (-1);
	  }

	release_bitmap_clusters(bm, *rl);
	free(*rl);

	for (i = vol->nr_clusters; i < new_size; i++)
		ntfs_bit_set(bm->bm, i, 0);

	if (!(*rl = alloc_cluster(resize, bm, nr_bm_clusters, new_size, 0)))
	  {
		err_printf(resize, "Couldn't allocate $Bitmap clusters");
		return (-1);
	  }
	return (0);
}

static int realloc_lcn_bitmap(ntfs_resize_t *resize, s64 bm_bsize)
{
	u8 *tmp;

	if (!(tmp = realloc(resize->lcn_bitmap.bm, bm_bsize)))
	  {
		err_printf(resize, "realloc failed");
		return (-1);
	  }

	resize->lcn_bitmap.bm = tmp;
	resize->lcn_bitmap.size = bm_bsize;
	bitmap_file_data_fixup(resize->new_volume_size, &resize->lcn_bitmap);
	return (0);
}

/**
 * truncate_bitmap_data_attr
 */
static int truncate_bitmap_data_attr(ntfs_resize_t *resize)
{
	ATTR_RECORD *a;
	runlist *rl;
	s64 bm_bsize, size;
	s64 nr_bm_clusters;
	ntfs_volume *vol = resize->vol;

	a = resize->ctx->attr;
	if (!a->non_resident)
	  {
		/* FIXME: handle resident attribute value */
		err_printf(resize, "Resident attribute in $Bitmap isn't supported!");
		return (-1);
	  }

	bm_bsize = nr_clusters_to_bitmap_byte_size(resize->new_volume_size);
	nr_bm_clusters = rounded_up_division(bm_bsize, vol->cluster_size);

	if (resize->shrink) {
	  if (realloc_bitmap_data_attr(resize, &rl, nr_bm_clusters) < 0)
	    return (-1);
	  if (realloc_lcn_bitmap(resize, bm_bsize) < 0)
	    return (-1);
	} else {
	  if (realloc_lcn_bitmap(resize, bm_bsize) < 0)
	    return (-1);
	  if (realloc_bitmap_data_attr(resize, &rl, nr_bm_clusters) < 0)
	    return (-1);
	}

	a->highest_vcn = cpu_to_le64(nr_bm_clusters - 1LL);
	a->allocated_size = cpu_to_le64(nr_bm_clusters * vol->cluster_size);
	a->data_size = cpu_to_le64(bm_bsize);
	a->initialized_size = cpu_to_le64(bm_bsize);

	if (replace_attribute_runlist(resize, vol, resize->ctx, rl) < 0)
	  return (-1);

	/*
	 * FIXME: update allocated/data sizes and timestamps in $FILE_NAME
	 * attribute too, for now chkdsk will do this for us.
	 */

	size = ntfs_rl_pwrite(vol, rl, 0, bm_bsize, resize->lcn_bitmap.bm);
	if (bm_bsize != size) {
	  if (size == -1)
	    err_printf(resize, "Couldn't write $Bitmap");
	  else
	    err_printf(resize, "Couldn't write full $Bitmap file (%lld from "
		       "%lld)", (long long)size, (long long)bm_bsize);
	  return (-1);
	}

	free(rl);
	return (0);
}

/**
 * lookup_data_attr
 *
 * Find the $DATA attribute (with or without a name) for the given MFT reference
 * (inode number).
 */
static int lookup_data_attr(ntfs_resize_t *resize, ntfs_volume *vol,
			     MFT_REF mref,
			     const char *aname,
			     ntfs_attr_search_ctx **ctx)
{
	ntfs_inode *ni;
	ntfschar *ustr = NULL;
	int len = 0;

	if (!(ni = ntfs_inode_open(vol, mref)))
	  {
		err_printf(resize, "ntfs_open_inode failed");
		return (-1);
	  }

	if (!(*ctx = attr_get_search_ctx(resize, ni, NULL)))
	  {
		return (-1);
	  }

	if (str2unicode(resize, aname, &ustr, &len) == -1)
	  {
		return (-1);
	  }

	if (ntfs_attr_lookup(AT_DATA, ustr, len, 0, 0, NULL, 0, *ctx))
	  {
		err_printf(resize, "ntfs_lookup_attr failed");
		return (-1);
	  }

	if (ustr != AT_UNNAMED)
		free(ustr);

	return (0);
}

/**
 * truncate_badclust_file
 *
 * Shrink the $BadClus file to match the new volume size.
 */
static int truncate_badclust_file(ntfs_resize_t *resize)
{
	progress_message(resize, "Updating $BadClust file");

	if (lookup_data_attr(resize, resize->vol, FILE_BadClus, "$Bad", &resize->ctx) != 0)
	  return (-1);

	/* FIXME: sanity_check_attr(ctx->attr); */
	if (truncate_badclust_bad_attr(resize) < 0)
	  return (-1);

	if (write_mft_record(resize, resize->vol, resize->ctx->ntfs_ino->mft_no,
			     resize->ctx->mrec))
	  {
		err_printf(resize, "Couldn't update $BadClust");
		return (-1);
	  }

	ntfs_attr_put_search_ctx(resize->ctx);
	return (0);
}

/**
 * truncate_bitmap_file
 *
 * Shrink the $Bitmap file to match the new volume size.
 */
static int truncate_bitmap_file(ntfs_resize_t *resize)
{
	progress_message(resize, "Updating $Bitmap file");

	if (lookup_data_attr(resize, resize->vol, FILE_Bitmap, NULL, &resize->ctx) != 0)
	  return (-1);

	if (truncate_bitmap_data_attr(resize) < 0)
	  return (-1);

	if (write_mft_record(resize, resize->vol, resize->ctx->ntfs_ino->mft_no,
			     resize->ctx->mrec))
	  {
		err_printf(resize, "Couldn't update $Bitmap");
		return (-1);
	  }

	ntfs_attr_put_search_ctx(resize->ctx);
	return (0);
}

/**
 * setup_lcn_bitmap
 *
 * Allocate a block of memory with one bit for each cluster of the disk.
 * All the bits are set to 0, except those representing the region beyond the
 * end of the disk.
 */
static int setup_lcn_bitmap(ntfs_resize_t *resize, struct bitmap *bm,
			    s64 nr_clusters)
{
	progress_message(resize, "Set up LCN bitmap");

	/* Determine lcn bitmap byte size and allocate it. */
	bm->size = rounded_up_division(nr_clusters, 8);

	if (!(bm->bm = (unsigned char *)calloc(1, bm->size)))
		return -1;

	bitmap_file_data_fixup(nr_clusters, bm);
	return 0;
}

/**
 * update_bootsector
 *
 * FIXME: should be done using ntfs_* functions
 */
static int update_bootsector(ntfs_resize_t *resize)
{
	NTFS_BOOT_SECTOR bs;
	s64  bs_size = sizeof(NTFS_BOOT_SECTOR);
	ntfs_volume *vol = resize->vol;

	progress_message(resize, "Updating Boot record");

	if (vol->dev->d_ops->seek(vol->dev, 0, SEEK_SET) == (off_t)-1)
	  {
		err_printf(resize, "lseek failed");
		return (-1);
	  }

	if (vol->dev->d_ops->read(vol->dev, &bs, bs_size) == -1)
	  {
		err_printf(resize, "read() error");
		return (-1);
	  }

	bs.number_of_sectors = cpu_to_sle64(resize->new_volume_size *
			bs.bpb.sectors_per_cluster);

	if (resize->mftmir_old) {
	  if (copy_clusters(resize, resize->mftmir_rl.lcn, resize->mftmir_old,
			    resize->mftmir_rl.length) < 0)
	    return (-1);
		bs.mftmirr_lcn = cpu_to_le64(resize->mftmir_rl.lcn);
	}

	if (vol->dev->d_ops->seek(vol->dev, 0, SEEK_SET) == (off_t)-1)
	  {
		err_printf(resize, "lseek failed");
		return (-1);
	  }

	if (vol->dev->d_ops->write(vol->dev, &bs, bs_size) == -1)
	  {
	    err_printf(resize, "write() error");
	    return (-1);
	  }
	return (0);
}

/**
 * mount_volume
 *
 * First perform some checks to determine if the volume is already mounted, or
 * is dirty (Windows wasn't shutdown properly).  If everything is OK, then mount
 * the volume (load the metadata into memory).
 */
static ntfs_volume *mount_volume(ntfs_resize_t *resize)
{
	unsigned long mntflag;
	ntfs_volume *vol = NULL;

	progress_message(resize, "Mounting volume");

	if (ntfs_check_if_mounted(opt.volume, &mntflag)) {
		err_printf(resize, "Failed to check '%s' mount state", opt.volume);
		return (NULL);
	}
	if (mntflag & NTFS_MF_MOUNTED) {
		if (!(mntflag & NTFS_MF_READONLY))
		  err_printf(resize, "Device '%s' is mounted read-write.  "
			     "You must unmount it first.", opt.volume);
		else
		  err_printf(resize, "Device '%s' is mounted.  "
			     "You must unmount it first.", opt.volume);
		return (NULL);
	}

	if (!(vol = ntfs_mount(opt.volume, 0))) {

		int err = errno;

		static char *ERRMESS = "Opening '%s' as NTFS failed.  %s";
		if (err == EINVAL)
			err_printf(resize, ERRMESS, opt.volume,
				   invalid_ntfs_msg);
		else if (err == EIO)
			err_printf(resize, ERRMESS, opt.volume,
				   corrupt_volume_msg);
		else if (err == EPERM)
			err_printf(resize, ERRMESS, opt.volume,
				   hibernated_volume_msg);
        	else if (err == EOPNOTSUPP)
			err_printf(resize, ERRMESS, opt.volume,
				   unclean_journal_msg);
		else
		  err_printf(resize, ERRMESS, opt.volume, "Unknown error.");
		return (NULL);
	}

	if (vol->flags & VOLUME_IS_DIRTY)
		if (opt.force-- <= 0)
		  {
			err_printf(resize, "Volume is scheduled for check.  Run chkdsk /f"
				 " and please try again, or see option -f.");
			return (NULL);
		  }

	if (NTFS_MAX_CLUSTER_SIZE < vol->cluster_size)
	  {
		err_printf(resize, "Cluster size %u is too large!",
			(unsigned int)vol->cluster_size);
		return (NULL);
	  }

	progress_message(resize, "Device name: %s", opt.volume);
	progress_message(resize, "NTFS volume version: %d.%d", vol->major_ver, vol->minor_ver);
	if (ntfs_version_is_supported(vol))
	  {
		err_printf(resize, "Unknown NTFS version");
		return (NULL);
	  }

	progress_message(resize, "Cluster size: %u bytes",
			(unsigned int)vol->cluster_size);

	return vol;
}

/**
 * prepare_volume_fixup
 *
 * Set the volume's dirty flag and wipe the filesystem journal.  When Windows
 * boots it will automatically run chkdsk to check for any problems.  If the
 * read-only command line option was given, this function will do nothing.
 */
static int prepare_volume_fixup(ntfs_resize_t *resize, ntfs_volume *vol)
{
	u16 flags;

	flags = vol->flags | VOLUME_IS_DIRTY;
	if (vol->major_ver >= 2)
		flags |= VOLUME_MOUNTED_ON_NT4;

	progress_message(resize, "Schedule chkdsk for NTFS consistency check "
			 "at Windows boot time");

	if (ntfs_volume_write_flags(vol, flags))
	  {
		err_printf(resize, "Failed to set $Volume dirty");
		return (-1);
	  }

	if (vol->dev->d_ops->sync(vol->dev) == -1)
	  {
		err_printf(resize, "Failed to sync device");
		return (-1);
	  }

	progress_message(resize, "Resetting $LogFile (this might take a while)");

	if (ntfs_logfile_reset(vol))
	  {
		err_printf(resize, "Failed to reset $LogFile");
		return (-1);
	  }

	if (vol->dev->d_ops->sync(vol->dev) == -1)
	  {
		err_printf(resize, "Failed to sync device");
		return (-1);
	  }
	return (0);
}


static void set_disk_usage_constraint(ntfs_resize_t *resize)
{
	/* last lcn for a filled up volume (no empty space) */
	s64 last = resize->inuse - 1;

	if (resize->last_unsupp < last)
		resize->last_unsupp = last;
}

static int check_resize_constraints(ntfs_resize_t *resize)
{
	s64 new_size = resize->new_volume_size;

	progress_message(resize, "Checking resize constraints");

	if (resize->badclusters) {
		progress_message(resize, "%sThe NTFS volume has at least %lld bad sector%s.",
		       !opt.badsectors ? ERR_PREFIX : "",
		       resize->badclusters,
		       resize->badclusters  - 1 ? "s" : "");
		if (!opt.badsectors) {
			progress_message(resize, "%s", bad_sectors_warning_msg);
			return (-1);
		}
	}
		
	/* FIXME: resize.shrink true also if only -i is used */
	if (!resize->shrink)
		return (0);

	if (resize->inuse == resize->vol->nr_clusters)
	  {
		err_printf(resize, "Volume is full.  To shrink it, "
			 "delete unused files.");
		return (-1);
	  }

	if (opt.info)
		return (0);

	/* FIXME: reserve some extra space so Windows can boot ... */
	if (new_size < resize->inuse)
	  {
		err_printf(resize, "New size can't be less than the space "
			   "already occupied by data.  You either need to "
			   "delete unused files.");
		return (-1);
	  }

	if (new_size <= resize->last_unsupp)
	  {
		err_printf(resize, "The fragmentation type you have isn't "
			   "supported yet.  The requested size is less than "
			   "the smallest shrunken volume size supported.");
		return (-1);
	  }
	return (0);
}


static int get_minblocks(ntfs_resize_t *resize, disk *theDisk,
			 unsigned *minBlocks)
{
	s64 new_b;
  
	/* Take the next supported cluster (free or relocatable)
	   plus reserve a cluster for the backup boot sector */
	resize->last_unsupp += 2;

	if (resize->last_unsupp > resize->vol->nr_clusters) {
		err_printf(resize, "Very rare, unsupported fragmentation type "
			   "detected.  Try to defragment your NTFS and retry "
			   "the operation");
		return (-1);
	}

	new_b = (resize->last_unsupp * resize->vol->cluster_size);
	*minBlocks = (unsigned) (new_b / theDisk->sectorSize);
	if (new_b % theDisk->sectorSize)
	  *minBlocks += 1;
	return (0);
}


static int _resize(const char *diskName, unsigned blocks, progress *prog,
		   int info, unsigned *minBlocks, unsigned *maxBlocks)
{
	int status = 0;
	disk theDisk;
	ntfsck_t fsck;
	ntfs_resize_t *resize = NULL;
	s64 new_size = 0;	/* in clusters; 0 = --info w/o --size */
	s64 device_size;        /* in bytes */
	ntfs_volume *vol = NULL;

	// Check params
	if (diskName == NULL)
	  return (status = errno = ERR_NULLPARAMETER);

	resize = malloc(sizeof(ntfs_resize_t));
	if (resize == NULL)
	  return (status = errno);

	// Try to get the disk structure.
	status = diskGet(diskName, &theDisk);
	if (status < 0)
	  {
	    errno = status;
	    goto err_out;
	  }

	/*
	  int force;         // Force to progress
	  int info;          // Info only, no resize
	  int badsectors;    // Support disks having bad sectors
	  s64 bytes;         // Number of bytes to resize to
	  char *volume;      // Disk name
	*/
	
	bzero(&opt, sizeof(opt));

	opt.force++;
	opt.info = info;
	resize->prog = prog;
	opt.badsectors++;
	opt.bytes = ((s64) blocks * (s64) theDisk.sectorSize);
	opt.volume = theDisk.name;

	if (resize->prog && (lockGet(&(resize->prog->lock)) >= 0))
	  {
	    bzero((void *) resize->prog, sizeof(progress));
	    resize->prog->total = 100;
	    resize->prog->canCancel = 1;
	    lockRelease(&(resize->prog->lock));
	  }

	if ((vol = mount_volume(resize)) == NULL)
	  {
		err_printf(resize, "Couldn't open volume '%s'!", opt.volume);
		goto err_out;
	  }

	CHECK_CANCEL();

	device_size = ntfs_device_size_get(vol->dev, vol->sector_size);
	device_size *= vol->sector_size;
	if (device_size <= 0)
	  {
		err_printf(resize, "Couldn't get device size (%lld)!",
			   device_size);
		goto err_out;
	  }

	CHECK_CANCEL();

	if (device_size < vol->nr_clusters * vol->cluster_size)
	  {
	    err_printf(resize, "Current NTFS volume size is bigger than the "
		       "device size!  Corrupt partition table or incorrect "
		       "device partitioning?");
		goto err_out;
	  }

	if (!opt.bytes && !opt.info)
		opt.bytes = device_size;

	/* Take the integer part: don't make the volume bigger than
	   requested */
	new_size = opt.bytes / vol->cluster_size;

	/* Backup boot sector at the end of device isn't counted in NTFS
	   volume size thus we have to reserve space for it. */
	if (new_size)
		--new_size;

	if (!opt.info) {
	  if (device_size < opt.bytes)
	    {
	      err_printf(resize, "New size can't be bigger than the device "
			 "size.  If you want to enlarge NTFS then first "
			 "enlarge the device size by e.g. fdisk.");
	      goto err_out;
	    }
	}

	if (!opt.info && (new_size == vol->nr_clusters ||
			  (opt.bytes == device_size &&
			   new_size == vol->nr_clusters - 1))) {
		progress_message(resize, "Nothing to do: NTFS volume size is "
				 "already OK.");
		goto out;
	}

	memset(&fsck, 0, sizeof(fsck));

	if (setup_lcn_bitmap(resize, &fsck.lcn_bitmap, vol->nr_clusters) != 0)
	  {
		err_printf(resize, "Failed to setup allocation bitmap");
		goto err_out;
	  }

	CHECK_CANCEL();

	if (build_allocation_bitmap(resize, vol, &fsck) != 0)
		goto err_out;

	CHECK_CANCEL();

	if (fsck.outsider || fsck.multi_ref)
	  {
	    char tmpChar[160];
	    strcat(tmpChar, "Filesystem check failed!  ");
	    if (fsck.outsider)
	      sprintf((tmpChar + strlen(tmpChar)), "%d clusters are "
		      "referenced outside of the volume.  ", fsck.outsider);
	    if (fsck.multi_ref)
	      sprintf((tmpChar + strlen(tmpChar)), "%d clusters are "
		      "referenced multiple times.  ", fsck.multi_ref);
	    sprintf((tmpChar + strlen(tmpChar)), "%s", corrupt_volume_msg);
	    err_printf(resize, tmpChar);
	    goto err_out;
	  }

	if (compare_bitmaps(resize, vol, &fsck.lcn_bitmap) != 0)
	  goto err_out;

	CHECK_CANCEL();

	bzero(resize, sizeof(ntfs_resize_t));
	resize->new_volume_size = new_size;
	resize->inuse = fsck.inuse;
	resize->lcn_bitmap = fsck.lcn_bitmap;
	resize->vol = vol;
	resize->prog = prog;

	/* This is also true if --info was used w/o --size (new_size = 0) */
	if (new_size < vol->nr_clusters)
		resize->shrink = 1;

	if (set_resize_constraints(resize) != 0)
	  goto err_out;

	CHECK_CANCEL();

	set_disk_usage_constraint(resize);

	CHECK_CANCEL();

	if (check_resize_constraints(resize) != 0)
	  goto err_out;

	if (opt.info) {
		if (get_minblocks(resize, &theDisk, minBlocks) < 0)
		  goto err_out;
		*maxBlocks = theDisk.numSectors;
		goto out;
	}

	CHECK_CANCEL();

	if (resize->prog && (lockGet(&(resize->prog->lock)) >= 0))
	  {
	    resize->prog->canCancel = 0;
	    lockRelease(&(resize->prog->lock));
	  }

	/* FIXME: performance - relocate logfile here if it's needed */
	if (prepare_volume_fixup(resize, vol) < 0)
	  goto err_out;
	progress_update(resize->prog, RSZPCNT_VOLFIXUP, 1, 1);

	if (resize->relocations)
	  if (relocate_inodes(resize) != 0)
	    goto err_out;
	progress_update(resize->prog, RSZPCNT_RELOCATIONS, 1, 1);

	if (truncate_badclust_file(resize) != 0)
	  goto err_out;
	progress_update(resize->prog, RSZPCNT_BADCLUST, 1, 1);

	if (truncate_bitmap_file(resize) != 0)
	  goto err_out;
	progress_update(resize->prog, RSZPCNT_TRUNCBMP, 1, 1);

	if (update_bootsector(resize) < 0)
	  goto err_out;
	progress_update(resize->prog, RSZPCNT_UPDBOOTSECT, 1, 1);

	/* We don't create backup boot sector because we don't know where the
	   partition will be split. The scheduled chkdsk will fix it */

	/* WARNING: don't modify the texts, external tools grep for them */
	progress_message(resize, "Syncing device");
	if (vol->dev->d_ops->sync(vol->dev) == -1)
	  {
		err_printf(resize, "fsync failed");
		goto err_out;
	  }
	progress_update(resize->prog, RSZPCNT_SYNC, 1, 1);

	if (resize->prog && (lockGet(&(resize->prog->lock)) >= 0))
	  {
	    resize->prog->finished = resize->prog->total;
	    resize->prog->percentFinished = 100;
	    lockRelease(&(resize->prog->lock));
	  }

	progress_message(resize, "Successfully resized NTFS on device '%s'.",
			 vol->dev->d_name);

 out:
	free(resize);
	return (0);
 err_out:
	free(resize);
	return (-1);
}


int ntfsGetResizeConstraints(const char *diskName, unsigned *minBlocks,
			     unsigned *maxBlocks)
{
  return (_resize(diskName, 0, NULL, 1, minBlocks, maxBlocks));
}


int ntfsResize(const char *diskName, unsigned blocks, progress *prog)
{
  return (_resize(diskName, blocks, prog, 0, NULL, NULL));
}
