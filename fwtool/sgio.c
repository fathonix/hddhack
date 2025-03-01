//This code is copied from idle3ctl.
/* sgio.c - by Mark Lord (C) 2007 -- freely distributable */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <scsi/scsi.h>
#include <scsi/sg.h>

#include "sgio.h"

#include <linux/hdreg.h>

extern int verbose;
extern int prefer_ata12;

/*
 * Taskfile layout for SG_ATA_16 cdb:
 *
 * LBA48:
 * cdb[ 3] = hob_feat
 * cdb[ 5] = hob_nsect
 * cdb[ 7] = hob_lbal
 * cdb[ 9] = hob_lbam
 * cdb[11] = hob_lbah
 *
 * LBA28/LBA48:
 * cdb[ 4] = feat
 * cdb[ 6] = nsect
 * cdb[ 8] = lbal
 * cdb[10] = lbam
 * cdb[12] = lbah
 * cdb[13] = device
 * cdb[14] = command
 *
 * Taskfile layout for SG_ATA_12 cdb:
 *
 * cdb[ 3] = feat
 * cdb[ 4] = nsect
 * cdb[ 5] = lbal
 * cdb[ 6] = lbam
 * cdb[ 7] = lbah
 * cdb[ 8] = device
 * cdb[ 9] = command
 *
 * dxfer_direction choices:
 *	SG_DXFER_TO_DEV, SG_DXFER_FROM_DEV, SG_DXFER_NONE
 */

static inline int needs_lba48 (__u8 ata_op, __u64 lba, unsigned int nsect)
{
	switch (ata_op) {
		case ATA_OP_DSM:
		case ATA_OP_READ_PIO_EXT:
		case ATA_OP_READ_DMA_EXT:
		case ATA_OP_WRITE_PIO_EXT:
		case ATA_OP_WRITE_DMA_EXT:
		case ATA_OP_READ_VERIFY_EXT:
		case ATA_OP_WRITE_UNC_EXT:
		case ATA_OP_READ_NATIVE_MAX_EXT:
		case ATA_OP_SET_MAX_EXT:
		case ATA_OP_FLUSHCACHE_EXT:
			return 1;
	}
	if (lba >= lba28_limit)
		return 1;
	if (nsect) {
		if (nsect > 0xff)
			return 1;
		if ((lba + nsect - 1) >= lba28_limit)
			return 1;
	}
	return 0;
}

static inline int is_dma (__u8 ata_op)
{
	switch (ata_op) {
		case ATA_OP_DSM:
		case ATA_OP_READ_DMA_EXT:
		case ATA_OP_READ_FPDMA:
		case ATA_OP_WRITE_DMA_EXT:
		case ATA_OP_WRITE_FPDMA:
		case ATA_OP_READ_DMA:
		case ATA_OP_WRITE_DMA:
			return SG_DMA;
		default:
			return SG_PIO;
	}
}

void tf_init (struct ata_tf *tf, __u8 ata_op, __u64 lba, unsigned int nsect)
{
	memset(tf, 0, sizeof(*tf));
	tf->command  = ata_op;
	tf->dev      = ATA_USING_LBA;
	tf->lob.lbal = lba;
	tf->lob.lbam = lba >>  8;
	tf->lob.lbah = lba >> 16;
	tf->lob.nsect = nsect;
	if (needs_lba48(ata_op, lba, nsect)) {
		tf->is_lba48 = 1;
		tf->hob.nsect = nsect >> 8;
		tf->hob.lbal = lba >> 24;
		tf->hob.lbam = lba >> 32;
		tf->hob.lbah = lba >> 40;
	} else {
		tf->dev |= (lba >> 24) & 0x0f;
	}
}

#ifdef SG_IO

__u64 tf_to_lba (struct ata_tf *tf)
{
	__u32 lba24, lbah;
	__u64 lba64;

	lba24 = (tf->lob.lbah << 16) | (tf->lob.lbam << 8) | (tf->lob.lbal);
	if (tf->is_lba48)
		lbah = (tf->hob.lbah << 16) | (tf->hob.lbam << 8) | (tf->hob.lbal);
	else
		lbah = (tf->dev & 0x0f);
	lba64 = (((__u64)lbah) << 24) | (__u64)lba24;
	return lba64;
}

enum {
	SG_CDB2_TLEN_NODATA	= 0 << 0,
	SG_CDB2_TLEN_FEAT	= 1 << 0,
	SG_CDB2_TLEN_NSECT	= 2 << 0,

	SG_CDB2_TLEN_BYTES	= 0 << 2,
	SG_CDB2_TLEN_SECTORS	= 1 << 2,

	SG_CDB2_TDIR_TO_DEV	= 0 << 3,
	SG_CDB2_TDIR_FROM_DEV	= 1 << 3,

	SG_CDB2_CHECK_COND	= 1 << 5,
};

static void dump_bytes (const char *prefix, unsigned char *p, int len)
{
	int i;

	if (prefix)
		fprintf(stderr, "%s: ", prefix);
	for (i = 0; i < len; ++i)
		fprintf(stderr, " %02x", p[i]);
	fprintf(stderr, "\n");
}

int sg16 (int fd, int rw, int dma, struct ata_tf *tf,
	void *data, unsigned int data_bytes, unsigned int timeout_secs)
{
	unsigned char cdb[SG_ATA_16_LEN];
	unsigned char sb[32], *desc;
	struct scsi_sg_io_hdr io_hdr;

	memset(&cdb, 0, sizeof(cdb));
	memset(&sb,     0, sizeof(sb));
	memset(&io_hdr, 0, sizeof(struct scsi_sg_io_hdr));

	if (dma) {
		//cdb[1] = data ? (rw ? SG_ATA_PROTO_UDMA_OUT : SG_ATA_PROTO_UDMA_IN) : SG_ATA_PROTO_NON_DATA;
		cdb[1] = data ? SG_ATA_PROTO_DMA : SG_ATA_PROTO_NON_DATA;
	} else {
		cdb[1] = data ? (rw ? SG_ATA_PROTO_PIO_OUT : SG_ATA_PROTO_PIO_IN) : SG_ATA_PROTO_NON_DATA;
	}
	cdb[ 2] = SG_CDB2_CHECK_COND;
	if (data) {
		cdb[2] |= SG_CDB2_TLEN_NSECT | SG_CDB2_TLEN_SECTORS;
		cdb[2] |= rw ? SG_CDB2_TDIR_TO_DEV : SG_CDB2_TDIR_FROM_DEV;
	}

	if (!prefer_ata12 || tf->is_lba48) {
		cdb[ 0] = SG_ATA_16;
		cdb[ 4] = tf->lob.feat;
		cdb[ 6] = tf->lob.nsect;
		cdb[ 8] = tf->lob.lbal;
		cdb[10] = tf->lob.lbam;
		cdb[12] = tf->lob.lbah;
		cdb[13] = tf->dev;
		cdb[14] = tf->command;
		if (tf->is_lba48) {
			cdb[ 1] |= SG_ATA_LBA48;
			cdb[ 3]  = tf->hob.feat;
			cdb[ 5]  = tf->hob.nsect;
			cdb[ 7]  = tf->hob.lbal;
			cdb[ 9]  = tf->hob.lbam;
			cdb[11]  = tf->hob.lbah;
		}
		io_hdr.cmd_len = SG_ATA_16_LEN;
	} else {
		cdb[ 0] = SG_ATA_12;
		cdb[ 3] = tf->lob.feat;
		cdb[ 4] = tf->lob.nsect;
		cdb[ 5] = tf->lob.lbal;
		cdb[ 6] = tf->lob.lbam;
		cdb[ 7] = tf->lob.lbah;
		cdb[ 8] = tf->dev;
		cdb[ 9] = tf->command;
		io_hdr.cmd_len = SG_ATA_12_LEN;
	}

	io_hdr.interface_id	= 'S';
	io_hdr.mx_sb_len	= sizeof(sb);
	io_hdr.dxfer_direction	= data ? (rw ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV) : SG_DXFER_NONE;
	io_hdr.dxfer_len	= data ? data_bytes : 0;
	io_hdr.dxferp		= data;
	io_hdr.cmdp		= cdb;
	io_hdr.sbp		= sb;
	io_hdr.pack_id		= tf_to_lba(tf);
	io_hdr.timeout		= (timeout_secs ? timeout_secs : 5) * 1000; /* msecs */

	if (verbose) {
		dump_bytes("outgoing cdb", cdb, sizeof(cdb));
		if (data)
			dump_bytes("data", data, 16);
	}

	if (ioctl(fd, SG_IO, &io_hdr) == -1) {
		if (verbose)
			perror("ioctl(fd,SG_IO)");
		return -1;	/* SG_IO not supported */
	}

	if (verbose)
		fprintf(stderr, "SG_IO: ATA_%u status=0x%x, host_status=0x%x, driver_status=0x%x\n",
			io_hdr.cmd_len, io_hdr.status, io_hdr.host_status, io_hdr.driver_status);

	if (io_hdr.host_status || io_hdr.driver_status != SG_DRIVER_SENSE
	 || (io_hdr.status && io_hdr.status != SG_CHECK_CONDITION))
	{
		if (verbose)
			fprintf(stderr, "SG_IO: bad response (not CHECK_CONDITION)\n");
	  	errno = EBADE;
		return -1;
	}

	desc = sb + 8;
	if (sb[0] != 0x72 || sb[7] < 14 || desc[0] != 0x09 || desc[1] < 0x0c) {
		if (verbose)
			dump_bytes("SG_IO: bad/missing sense data, sb[]", sb, sizeof(sb));
		errno = EBADE;
		return -1;
	}

	if (verbose)
		dump_bytes("SG_IO: sb[]", sb, sizeof(sb));

	if (verbose) {
		int len = desc[1], maxlen = sizeof(sb) - 8 - 2;
		if (len > maxlen)
			len = maxlen;
		dump_bytes("SG_IO: desc[]", desc, len);
	}

	tf->is_lba48  = desc[ 2] & 1;
	tf->error     = desc[ 3];
	tf->lob.nsect = desc[ 5];
	tf->lob.lbal  = desc[ 7];
	tf->lob.lbam  = desc[ 9];
	tf->lob.lbah  = desc[11];
	tf->dev       = desc[12];
	tf->status    = desc[13];
	tf->hob.feat  = 0;
	if (tf->is_lba48) {
		tf->hob.nsect = desc[ 4];
		tf->hob.lbal  = desc[ 6];
		tf->hob.lbam  = desc[ 8];
		tf->hob.lbah  = desc[10];
	} else {
		tf->hob.nsect = 0;
		tf->hob.lbal  = 0;
		tf->hob.lbam  = 0;
		tf->hob.lbah  = 0;
	}

	if (verbose)
		fprintf(stderr, "      ATA_%u stat=%02x err=%02x nsect=%02x lbal=%02x lbam=%02x lbah=%02x dev=%02x\n",
				io_hdr.cmd_len, tf->status, tf->error, tf->lob.nsect, tf->lob.lbal, tf->lob.lbam, tf->lob.lbah, tf->dev);

	if (tf->status & (ATA_STAT_ERR | ATA_STAT_DRQ)) {
		if (verbose) {
			fprintf(stderr, "I/O error, ata_op=0x%02x ata_status=0x%02x ata_error=0x%02x\n",
				tf->command, tf->status, tf->error);
		}
		errno = EIO;
		return -1;
	}
	return 0;
}

#endif /* SG_IO */

int do_drive_cmd (int fd, unsigned char *args)
{
#ifdef SG_IO

	struct ata_tf tf;
	void *data = NULL;
	unsigned int data_bytes = 0;
	int rc;

	if (args == NULL)
		goto use_legacy_ioctl;
	/*
	 * Reformat and try to issue via SG_IO:
	 */
	if (args[3]) {
		data_bytes = args[3] * 512;
		data       = args + 4;
	}
	tf_init(&tf, args[0], 0, args[1]);
	tf.lob.feat = args[2];
	if (tf.command == ATA_OP_SMART) {
		tf.lob.nsect = args[3];
		tf.lob.lbal  = args[1];
		tf.lob.lbam  = 0x4f;
		tf.lob.lbah  = 0xc2;
	}

	rc = sg16(fd, SG_READ, is_dma(tf.command), &tf, data, data_bytes, 0);
	if (rc == -1) {
		if (errno == EINVAL || errno == ENODEV || errno == EBADE)
			goto use_legacy_ioctl;
	}

	if (rc == 0 || errno == EIO) {
		args[0] = tf.status;
		args[1] = tf.error;
		args[2] = tf.lob.nsect;
	}
	return rc;

use_legacy_ioctl:
#endif /* SG_IO */
	if (verbose) {
		if (args)
			fprintf(stderr, "Trying legacy HDIO_DRIVE_CMD\n");
	}
	return ioctl(fd, HDIO_DRIVE_CMD, args);
}

int do_taskfile_cmd (int fd, struct hdio_taskfile *r, unsigned int timeout_secs)
{
	int rc;
#ifdef SG_IO
	struct ata_tf tf;
	void *data = NULL;
	unsigned int data_bytes = 0;
	int rw = SG_READ;
	/*
	 * Reformat and try to issue via SG_IO:
	 */
	tf_init(&tf, 0, 0, 0);
#if 1 /* debugging */
	if (verbose) {
		printf("oflags.lob_all=0x%02x, flags={", r->oflags.lob_all);
		if (r->oflags.lob.feat)	printf(" feat");
		if (r->oflags.lob.nsect)printf(" nsect");
		if (r->oflags.lob.lbal)	printf(" lbal");
		if (r->oflags.lob.lbam)	printf(" lbam");
		if (r->oflags.lob.lbah)	printf(" lbah");
		if (r->oflags.lob.dev)	printf(" dev");
		if (r->oflags.lob.command) printf(" command");
		printf(" }\n");
		printf("oflags.hob_all=0x%02x, flags={", r->oflags.hob_all);
		if (r->oflags.hob.feat)	printf(" feat");
		if (r->oflags.hob.nsect)printf(" nsect");
		if (r->oflags.hob.lbal)	printf(" lbal");
		if (r->oflags.hob.lbam)	printf(" lbam");
		if (r->oflags.hob.lbah)	printf(" lbah");
		printf(" }\n");
	}
#endif
	if (r->oflags.lob.feat)		tf.lob.feat  = r->lob.feat;
	if (r->oflags.lob.lbal)		tf.lob.lbal  = r->lob.lbal;
	if (r->oflags.lob.nsect)	tf.lob.nsect = r->lob.nsect;
	if (r->oflags.lob.lbam)		tf.lob.lbam  = r->lob.lbam;
	if (r->oflags.lob.lbah)		tf.lob.lbah  = r->lob.lbah;
	if (r->oflags.lob.dev)		tf.dev       = r->lob.dev;
	if (r->oflags.lob.command)	tf.command   = r->lob.command;
	if (needs_lba48(tf.command,0,0) || r->oflags.hob_all || r->iflags.hob_all) {
		tf.is_lba48 = 1;
		if (r->oflags.hob.feat)	tf.hob.feat  = r->hob.feat;
		if (r->oflags.hob.lbal)	tf.hob.lbal  = r->hob.lbal;
		if (r->oflags.hob.nsect)tf.hob.nsect = r->hob.nsect;
		if (r->oflags.hob.lbam)	tf.hob.lbam  = r->hob.lbam;
		if (r->oflags.hob.lbah)	tf.hob.lbah  = r->hob.lbah;
		if (verbose)
			fprintf(stderr, "using LBA48 taskfile\n");
	}
	switch (r->cmd_req) {
		case TASKFILE_CMD_REQ_OUT:
		case TASKFILE_CMD_REQ_RAW_OUT:
			data_bytes = r->obytes;
			data       = r->data;
			rw         = SG_WRITE;
			break;
		case TASKFILE_CMD_REQ_IN:
			data_bytes = r->ibytes;
			data       = r->data;
			break;
	}

	rc = sg16(fd, rw, is_dma(tf.command), &tf, data, data_bytes, timeout_secs);
	if (rc == -1) {
		if (errno == EINVAL || errno == ENODEV || errno == EBADE)
			goto use_legacy_ioctl;
	}

	if (rc == 0 || errno == EIO) {
		if (r->iflags.lob.feat)		r->lob.feat  = tf.error;
		if (r->iflags.lob.lbal)		r->lob.lbal  = tf.lob.lbal;
		if (r->iflags.lob.nsect)	r->lob.nsect = tf.lob.nsect;
		if (r->iflags.lob.lbam)		r->lob.lbam  = tf.lob.lbam;
		if (r->iflags.lob.lbah)		r->lob.lbah  = tf.lob.lbah;
		if (r->iflags.lob.dev)		r->lob.dev   = tf.dev;
		if (r->iflags.lob.command)	r->lob.command = tf.status;
		if (r->iflags.hob.feat)		r->hob.feat  = tf.hob.feat;
		if (r->iflags.hob.lbal)		r->hob.lbal  = tf.hob.lbal;
		if (r->iflags.hob.nsect)	r->hob.nsect = tf.hob.nsect;
		if (r->iflags.hob.lbam)		r->hob.lbam  = tf.hob.lbam;
		if (r->iflags.hob.lbah)		r->hob.lbah  = tf.hob.lbah;
	}
	return rc;

use_legacy_ioctl:
#else
	timeout_secs = 0;	/* keep compiler happy */
#endif /* SG_IO */
	if (verbose)
		fprintf(stderr, "trying legacy HDIO_DRIVE_TASKFILE\n");
	errno = 0;

	rc = ioctl(fd, HDIO_DRIVE_TASKFILE, r);
	if (verbose) {
		int err = errno;
		fprintf(stderr, "rc=%d, errno=%d, returned ATA registers: ", rc, err);
		if (r->iflags.lob.feat)		fprintf(stderr, " er=%02x", r->lob.feat);
		if (r->iflags.lob.nsect)	fprintf(stderr, " ns=%02x", r->lob.nsect);
		if (r->iflags.lob.lbal)		fprintf(stderr, " ll=%02x", r->lob.lbal);
		if (r->iflags.lob.lbam)		fprintf(stderr, " lm=%02x", r->lob.lbam);
		if (r->iflags.lob.lbah)		fprintf(stderr, " lh=%02x", r->lob.lbah);
		if (r->iflags.lob.dev)		fprintf(stderr, " dh=%02x", r->lob.dev);
		if (r->iflags.lob.command)	fprintf(stderr, " st=%02x", r->lob.command);
		if (r->iflags.hob.feat)		fprintf(stderr, " err=%02x", r->hob.feat);
		if (r->iflags.hob.nsect)	fprintf(stderr, " err=%02x", r->hob.nsect);
		if (r->iflags.hob.lbal)		fprintf(stderr, " err=%02x", r->hob.lbal);
		if (r->iflags.hob.lbam)		fprintf(stderr, " err=%02x", r->hob.lbam);
		if (r->iflags.hob.lbah)		fprintf(stderr, " err=%02x", r->hob.lbah);
		fprintf(stderr, "\n");
		errno = err;
	}
	if (rc == -1 && errno == EINVAL) {
		fprintf(stderr, "The running kernel lacks CONFIG_IDE_TASK_IOCTL support for this device.\n");
		errno = EINVAL;
	}
	return rc;
}

void init_hdio_taskfile (struct hdio_taskfile *r, __u8 ata_op, int rw, int force_lba48,
				__u64 lba, unsigned int nsect, int data_bytes)
{
	memset(r, 0, sizeof(struct hdio_taskfile) + data_bytes);
	if (!data_bytes) {
		r->dphase  = TASKFILE_DPHASE_NONE;
		r->cmd_req = TASKFILE_CMD_REQ_NODATA;
	} else if (rw == RW_WRITE) {
		r->dphase  = TASKFILE_DPHASE_PIO_OUT;
		r->cmd_req = TASKFILE_CMD_REQ_RAW_OUT;
		r->obytes  = data_bytes;
	} else { /* rw == RW_READ */
		r->dphase  = TASKFILE_DPHASE_PIO_IN;
		r->cmd_req = TASKFILE_CMD_REQ_IN;
		r->ibytes  = data_bytes;
	}
	r->lob.command      = ata_op;
	r->oflags.lob.command = 1;
	r->oflags.lob.dev     = 1;
	r->oflags.lob.lbal    = 1;
	r->oflags.lob.lbam    = 1;
	r->oflags.lob.lbah    = 1;
	r->oflags.lob.nsect   = 1;

	r->iflags.lob.command = 1;
	r->iflags.lob.feat    = 1;

	r->lob.nsect = nsect;
	r->lob.lbal  = lba;
	r->lob.lbam  = lba >>  8;
	r->lob.lbah  = lba >> 16;
	r->lob.dev   = 0xa0 | ATA_USING_LBA;

	if (needs_lba48(ata_op, lba, nsect) || force_lba48) {
		r->hob.nsect = nsect >>  8;
		r->hob.lbal  = lba   >> 24;
		r->hob.lbam  = lba   >> 32;
		r->hob.lbah  = lba   >> 40;
		r->oflags.hob.nsect = 1;
		r->oflags.hob.lbal  = 1;
		r->oflags.hob.lbam  = 1;
		r->oflags.hob.lbah  = 1;
	} else {
		r->lob.dev |= (lba >> 24) & 0x0f;
	}
}
