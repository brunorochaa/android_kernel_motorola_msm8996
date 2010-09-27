/*
 * include/net/9p/9p.h
 *
 * 9P protocol definitions.
 *
 *  Copyright (C) 2005 by Latchesar Ionkov <lucho@ionkov.net>
 *  Copyright (C) 2004 by Eric Van Hensbergen <ericvh@gmail.com>
 *  Copyright (C) 2002 by Ron Minnich <rminnich@lanl.gov>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 *  Free Software Foundation
 *  51 Franklin Street, Fifth Floor
 *  Boston, MA  02111-1301  USA
 *
 */

#ifndef NET_9P_H
#define NET_9P_H

/**
 * enum p9_debug_flags - bits for mount time debug parameter
 * @P9_DEBUG_ERROR: more verbose error messages including original error string
 * @P9_DEBUG_9P: 9P protocol tracing
 * @P9_DEBUG_VFS: VFS API tracing
 * @P9_DEBUG_CONV: protocol conversion tracing
 * @P9_DEBUG_MUX: trace management of concurrent transactions
 * @P9_DEBUG_TRANS: transport tracing
 * @P9_DEBUG_SLABS: memory management tracing
 * @P9_DEBUG_FCALL: verbose dump of protocol messages
 * @P9_DEBUG_FID: fid allocation/deallocation tracking
 * @P9_DEBUG_PKT: packet marshalling/unmarshalling
 * @P9_DEBUG_FSC: FS-cache tracing
 *
 * These flags are passed at mount time to turn on various levels of
 * verbosity and tracing which will be output to the system logs.
 */

enum p9_debug_flags {
	P9_DEBUG_ERROR = 	(1<<0),
	P9_DEBUG_9P = 		(1<<2),
	P9_DEBUG_VFS =		(1<<3),
	P9_DEBUG_CONV =		(1<<4),
	P9_DEBUG_MUX =		(1<<5),
	P9_DEBUG_TRANS =	(1<<6),
	P9_DEBUG_SLABS =      	(1<<7),
	P9_DEBUG_FCALL =	(1<<8),
	P9_DEBUG_FID =		(1<<9),
	P9_DEBUG_PKT =		(1<<10),
	P9_DEBUG_FSC =		(1<<11),
};

#ifdef CONFIG_NET_9P_DEBUG
extern unsigned int p9_debug_level;

#define P9_DPRINTK(level, format, arg...) \
do {  \
	if ((p9_debug_level & level) == level) {\
		if (level == P9_DEBUG_9P) \
			printk(KERN_NOTICE "(%8.8d) " \
			format , task_pid_nr(current) , ## arg); \
		else \
			printk(KERN_NOTICE "-- %s (%d): " \
			format , __func__, task_pid_nr(current) , ## arg); \
	} \
} while (0)

#else
#define P9_DPRINTK(level, format, arg...)  do { } while (0)
#endif

#define P9_EPRINTK(level, format, arg...) \
do { \
	printk(level "9p: %s (%d): " \
		format , __func__, task_pid_nr(current), ## arg); \
} while (0)

/**
 * enum p9_msg_t - 9P message types
 * @P9_TLERROR: not used
 * @P9_RLERROR: response for any failed request for 9P2000.L
 * @P9_TSTATFS: file system status request
 * @P9_RSTATFS: file system status response
 * @P9_TSYMLINK: make symlink request
 * @P9_RSYMLINK: make symlink response
 * @P9_TMKNOD: create a special file object request
 * @P9_RMKNOD: create a special file object response
 * @P9_TLCREATE: prepare a handle for I/O on an new file for 9P2000.L
 * @P9_RLCREATE: response with file access information for 9P2000.L
 * @P9_TRENAME: rename request
 * @P9_RRENAME: rename response
 * @P9_TMKDIR: create a directory request
 * @P9_RMKDIR: create a directory response
 * @P9_TVERSION: version handshake request
 * @P9_RVERSION: version handshake response
 * @P9_TAUTH: request to establish authentication channel
 * @P9_RAUTH: response with authentication information
 * @P9_TATTACH: establish user access to file service
 * @P9_RATTACH: response with top level handle to file hierarchy
 * @P9_TERROR: not used
 * @P9_RERROR: response for any failed request
 * @P9_TFLUSH: request to abort a previous request
 * @P9_RFLUSH: response when previous request has been cancelled
 * @P9_TWALK: descend a directory hierarchy
 * @P9_RWALK: response with new handle for position within hierarchy
 * @P9_TOPEN: prepare a handle for I/O on an existing file
 * @P9_ROPEN: response with file access information
 * @P9_TCREATE: prepare a handle for I/O on a new file
 * @P9_RCREATE: response with file access information
 * @P9_TREAD: request to transfer data from a file or directory
 * @P9_RREAD: response with data requested
 * @P9_TWRITE: reuqest to transfer data to a file
 * @P9_RWRITE: response with out much data was transfered to file
 * @P9_TCLUNK: forget about a handle to an entity within the file system
 * @P9_RCLUNK: response when server has forgotten about the handle
 * @P9_TREMOVE: request to remove an entity from the hierarchy
 * @P9_RREMOVE: response when server has removed the entity
 * @P9_TSTAT: request file entity attributes
 * @P9_RSTAT: response with file entity attributes
 * @P9_TWSTAT: request to update file entity attributes
 * @P9_RWSTAT: response when file entity attributes are updated
 *
 * There are 14 basic operations in 9P2000, paired as
 * requests and responses.  The one special case is ERROR
 * as there is no @P9_TERROR request for clients to transmit to
 * the server, but the server may respond to any other request
 * with an @P9_RERROR.
 *
 * See Also: http://plan9.bell-labs.com/sys/man/5/INDEX.html
 */

enum p9_msg_t {
	P9_TLERROR = 6,
	P9_RLERROR,
	P9_TSTATFS = 8,
	P9_RSTATFS,
	P9_TLOPEN = 12,
	P9_RLOPEN,
	P9_TLCREATE = 14,
	P9_RLCREATE,
	P9_TSYMLINK = 16,
	P9_RSYMLINK,
	P9_TMKNOD = 18,
	P9_RMKNOD,
	P9_TRENAME = 20,
	P9_RRENAME,
	P9_TGETATTR = 24,
	P9_RGETATTR,
	P9_TSETATTR = 26,
	P9_RSETATTR,
	P9_TXATTRWALK = 30,
	P9_RXATTRWALK,
	P9_TXATTRCREATE = 32,
	P9_RXATTRCREATE,
	P9_TREADDIR = 40,
	P9_RREADDIR,
	P9_TFSYNC = 50,
	P9_RFSYNC,
	P9_TLOCK = 52,
	P9_RLOCK,
	P9_TLINK = 70,
	P9_RLINK,
	P9_TMKDIR = 72,
	P9_RMKDIR,
	P9_TVERSION = 100,
	P9_RVERSION,
	P9_TAUTH = 102,
	P9_RAUTH,
	P9_TATTACH = 104,
	P9_RATTACH,
	P9_TERROR = 106,
	P9_RERROR,
	P9_TFLUSH = 108,
	P9_RFLUSH,
	P9_TWALK = 110,
	P9_RWALK,
	P9_TOPEN = 112,
	P9_ROPEN,
	P9_TCREATE = 114,
	P9_RCREATE,
	P9_TREAD = 116,
	P9_RREAD,
	P9_TWRITE = 118,
	P9_RWRITE,
	P9_TCLUNK = 120,
	P9_RCLUNK,
	P9_TREMOVE = 122,
	P9_RREMOVE,
	P9_TSTAT = 124,
	P9_RSTAT,
	P9_TWSTAT = 126,
	P9_RWSTAT,
};

/**
 * enum p9_open_mode_t - 9P open modes
 * @P9_OREAD: open file for reading only
 * @P9_OWRITE: open file for writing only
 * @P9_ORDWR: open file for reading or writing
 * @P9_OEXEC: open file for execution
 * @P9_OTRUNC: truncate file to zero-length before opening it
 * @P9_OREXEC: close the file when an exec(2) system call is made
 * @P9_ORCLOSE: remove the file when the file is closed
 * @P9_OAPPEND: open the file and seek to the end
 * @P9_OEXCL: only create a file, do not open it
 *
 * 9P open modes differ slightly from Posix standard modes.
 * In particular, there are extra modes which specify different
 * semantic behaviors than may be available on standard Posix
 * systems.  For example, @P9_OREXEC and @P9_ORCLOSE are modes that
 * most likely will not be issued from the Linux VFS client, but may
 * be supported by servers.
 *
 * See Also: http://plan9.bell-labs.com/magic/man2html/2/open
 */

enum p9_open_mode_t {
	P9_OREAD = 0x00,
	P9_OWRITE = 0x01,
	P9_ORDWR = 0x02,
	P9_OEXEC = 0x03,
	P9_OTRUNC = 0x10,
	P9_OREXEC = 0x20,
	P9_ORCLOSE = 0x40,
	P9_OAPPEND = 0x80,
	P9_OEXCL = 0x1000,
};

/**
 * enum p9_perm_t - 9P permissions
 * @P9_DMDIR: mode bite for directories
 * @P9_DMAPPEND: mode bit for is append-only
 * @P9_DMEXCL: mode bit for excluse use (only one open handle allowed)
 * @P9_DMMOUNT: mode bite for mount points
 * @P9_DMAUTH: mode bit for authentication file
 * @P9_DMTMP: mode bit for non-backed-up files
 * @P9_DMSYMLINK: mode bit for symbolic links (9P2000.u)
 * @P9_DMLINK: mode bit for hard-link (9P2000.u)
 * @P9_DMDEVICE: mode bit for device files (9P2000.u)
 * @P9_DMNAMEDPIPE: mode bit for named pipe (9P2000.u)
 * @P9_DMSOCKET: mode bit for socket (9P2000.u)
 * @P9_DMSETUID: mode bit for setuid (9P2000.u)
 * @P9_DMSETGID: mode bit for setgid (9P2000.u)
 * @P9_DMSETVTX: mode bit for sticky bit (9P2000.u)
 *
 * 9P permissions differ slightly from Posix standard modes.
 *
 * See Also: http://plan9.bell-labs.com/magic/man2html/2/stat
 */
enum p9_perm_t {
	P9_DMDIR = 0x80000000,
	P9_DMAPPEND = 0x40000000,
	P9_DMEXCL = 0x20000000,
	P9_DMMOUNT = 0x10000000,
	P9_DMAUTH = 0x08000000,
	P9_DMTMP = 0x04000000,
/* 9P2000.u extensions */
	P9_DMSYMLINK = 0x02000000,
	P9_DMLINK = 0x01000000,
	P9_DMDEVICE = 0x00800000,
	P9_DMNAMEDPIPE = 0x00200000,
	P9_DMSOCKET = 0x00100000,
	P9_DMSETUID = 0x00080000,
	P9_DMSETGID = 0x00040000,
	P9_DMSETVTX = 0x00010000,
};

/**
 * enum p9_qid_t - QID types
 * @P9_QTDIR: directory
 * @P9_QTAPPEND: append-only
 * @P9_QTEXCL: excluse use (only one open handle allowed)
 * @P9_QTMOUNT: mount points
 * @P9_QTAUTH: authentication file
 * @P9_QTTMP: non-backed-up files
 * @P9_QTSYMLINK: symbolic links (9P2000.u)
 * @P9_QTLINK: hard-link (9P2000.u)
 * @P9_QTFILE: normal files
 *
 * QID types are a subset of permissions - they are primarily
 * used to differentiate semantics for a file system entity via
 * a jump-table.  Their value is also the most signifigant 16 bits
 * of the permission_t
 *
 * See Also: http://plan9.bell-labs.com/magic/man2html/2/stat
 */
enum p9_qid_t {
	P9_QTDIR = 0x80,
	P9_QTAPPEND = 0x40,
	P9_QTEXCL = 0x20,
	P9_QTMOUNT = 0x10,
	P9_QTAUTH = 0x08,
	P9_QTTMP = 0x04,
	P9_QTSYMLINK = 0x02,
	P9_QTLINK = 0x01,
	P9_QTFILE = 0x00,
};

/* 9P Magic Numbers */
#define P9_NOTAG	(u16)(~0)
#define P9_NOFID	(u32)(~0)
#define P9_MAXWELEM	16

/* ample room for Twrite/Rread header */
#define P9_IOHDRSZ	24

/* Room for readdir header */
#define P9_READDIRHDRSZ	24

/**
 * struct p9_str - length prefixed string type
 * @len: length of the string
 * @str: the string
 *
 * The protocol uses length prefixed strings for all
 * string data, so we replicate that for our internal
 * string members.
 */

struct p9_str {
	u16 len;
	char *str;
};

/**
 * struct p9_qid - file system entity information
 * @type: 8-bit type &p9_qid_t
 * @version: 16-bit monotonically incrementing version number
 * @path: 64-bit per-server-unique ID for a file system element
 *
 * qids are identifiers used by 9P servers to track file system
 * entities.  The type is used to differentiate semantics for operations
 * on the entity (ie. read means something different on a directory than
 * on a file).  The path provides a server unique index for an entity
 * (roughly analogous to an inode number), while the version is updated
 * every time a file is modified and can be used to maintain cache
 * coherency between clients and serves.
 * Servers will often differentiate purely synthetic entities by setting
 * their version to 0, signaling that they should never be cached and
 * should be accessed synchronously.
 *
 * See Also://plan9.bell-labs.com/magic/man2html/2/stat
 */

struct p9_qid {
	u8 type;
	u32 version;
	u64 path;
};

/**
 * struct p9_stat - file system metadata information
 * @size: length prefix for this stat structure instance
 * @type: the type of the server (equivilent to a major number)
 * @dev: the sub-type of the server (equivilent to a minor number)
 * @qid: unique id from the server of type &p9_qid
 * @mode: Plan 9 format permissions of type &p9_perm_t
 * @atime: Last access/read time
 * @mtime: Last modify/write time
 * @length: file length
 * @name: last element of path (aka filename) in type &p9_str
 * @uid: owner name in type &p9_str
 * @gid: group owner in type &p9_str
 * @muid: last modifier in type &p9_str
 * @extension: area used to encode extended UNIX support in type &p9_str
 * @n_uid: numeric user id of owner (part of 9p2000.u extension)
 * @n_gid: numeric group id (part of 9p2000.u extension)
 * @n_muid: numeric user id of laster modifier (part of 9p2000.u extension)
 *
 * See Also: http://plan9.bell-labs.com/magic/man2html/2/stat
 */

struct p9_wstat {
	u16 size;
	u16 type;
	u32 dev;
	struct p9_qid qid;
	u32 mode;
	u32 atime;
	u32 mtime;
	u64 length;
	char *name;
	char *uid;
	char *gid;
	char *muid;
	char *extension;	/* 9p2000.u extensions */
	u32 n_uid;		/* 9p2000.u extensions */
	u32 n_gid;		/* 9p2000.u extensions */
	u32 n_muid;		/* 9p2000.u extensions */
};

struct p9_stat_dotl {
	u64 st_result_mask;
	struct p9_qid qid;
	u32 st_mode;
	u32 st_uid;
	u32 st_gid;
	u64 st_nlink;
	u64 st_rdev;
	u64 st_size;
	u64 st_blksize;
	u64 st_blocks;
	u64 st_atime_sec;
	u64 st_atime_nsec;
	u64 st_mtime_sec;
	u64 st_mtime_nsec;
	u64 st_ctime_sec;
	u64 st_ctime_nsec;
	u64 st_btime_sec;
	u64 st_btime_nsec;
	u64 st_gen;
	u64 st_data_version;
};

#define P9_STATS_MODE		0x00000001ULL
#define P9_STATS_NLINK		0x00000002ULL
#define P9_STATS_UID		0x00000004ULL
#define P9_STATS_GID		0x00000008ULL
#define P9_STATS_RDEV		0x00000010ULL
#define P9_STATS_ATIME		0x00000020ULL
#define P9_STATS_MTIME		0x00000040ULL
#define P9_STATS_CTIME		0x00000080ULL
#define P9_STATS_INO		0x00000100ULL
#define P9_STATS_SIZE		0x00000200ULL
#define P9_STATS_BLOCKS		0x00000400ULL

#define P9_STATS_BTIME		0x00000800ULL
#define P9_STATS_GEN		0x00001000ULL
#define P9_STATS_DATA_VERSION	0x00002000ULL

#define P9_STATS_BASIC		0x000007ffULL /* Mask for fields up to BLOCKS */
#define P9_STATS_ALL		0x00003fffULL /* Mask for All fields above */

/**
 * struct p9_iattr_dotl - P9 inode attribute for setattr
 * @valid: bitfield specifying which fields are valid
 *         same as in struct iattr
 * @mode: File permission bits
 * @uid: user id of owner
 * @gid: group id
 * @size: File size
 * @atime_sec: Last access time, seconds
 * @atime_nsec: Last access time, nanoseconds
 * @mtime_sec: Last modification time, seconds
 * @mtime_nsec: Last modification time, nanoseconds
 */

struct p9_iattr_dotl {
	u32 valid;
	u32 mode;
	u32 uid;
	u32 gid;
	u64 size;
	u64 atime_sec;
	u64 atime_nsec;
	u64 mtime_sec;
	u64 mtime_nsec;
};

#define P9_LOCK_SUCCESS 0
#define P9_LOCK_BLOCKED 1
#define P9_LOCK_ERROR 2
#define P9_LOCK_GRACE 3

#define P9_LOCK_FLAGS_BLOCK 1
#define P9_LOCK_FLAGS_RECLAIM 2

/* struct p9_flock: POSIX lock structure
 * @type - type of lock
 * @flags - lock flags
 * @start - starting offset of the lock
 * @length - number of bytes
 * @proc_id - process id which wants to take lock
 * @client_id - client id
 */

struct p9_flock {
	u8 type;
	u32 flags;
	u64 start;
	u64 length;
	u32 proc_id;
	char *client_id;
};

/* Structures for Protocol Operations */
struct p9_tstatfs {
	u32 fid;
};

struct p9_rstatfs {
	u32 type;
	u32 bsize;
	u64 blocks;
	u64 bfree;
	u64 bavail;
	u64 files;
	u64 ffree;
	u64 fsid;
	u32 namelen;
};

struct p9_trename {
	u32 fid;
	u32 newdirfid;
	struct p9_str name;
};

struct p9_rrename {
};

struct p9_tversion {
	u32 msize;
	struct p9_str version;
};

struct p9_rversion {
	u32 msize;
	struct p9_str version;
};

struct p9_tauth {
	u32 afid;
	struct p9_str uname;
	struct p9_str aname;
	u32 n_uname;		/* 9P2000.u extensions */
};

struct p9_rauth {
	struct p9_qid qid;
};

struct p9_rerror {
	struct p9_str error;
	u32 errno;		/* 9p2000.u extension */
};

struct p9_tflush {
	u16 oldtag;
};

struct p9_rflush {
};

struct p9_tattach {
	u32 fid;
	u32 afid;
	struct p9_str uname;
	struct p9_str aname;
	u32 n_uname;		/* 9P2000.u extensions */
};

struct p9_rattach {
	struct p9_qid qid;
};

struct p9_twalk {
	u32 fid;
	u32 newfid;
	u16 nwname;
	struct p9_str wnames[16];
};

struct p9_rwalk {
	u16 nwqid;
	struct p9_qid wqids[16];
};

struct p9_topen {
	u32 fid;
	u8 mode;
};

struct p9_ropen {
	struct p9_qid qid;
	u32 iounit;
};

struct p9_tcreate {
	u32 fid;
	struct p9_str name;
	u32 perm;
	u8 mode;
	struct p9_str extension;
};

struct p9_rcreate {
	struct p9_qid qid;
	u32 iounit;
};

struct p9_tread {
	u32 fid;
	u64 offset;
	u32 count;
};

struct p9_rread {
	u32 count;
	u8 *data;
};

struct p9_twrite {
	u32 fid;
	u64 offset;
	u32 count;
	u8 *data;
};

struct p9_rwrite {
	u32 count;
};

struct p9_treaddir {
	u32 fid;
	u64 offset;
	u32 count;
};

struct p9_rreaddir {
	u32 count;
	u8 *data;
};


struct p9_tclunk {
	u32 fid;
};

struct p9_rclunk {
};

struct p9_tremove {
	u32 fid;
};

struct p9_rremove {
};

struct p9_tstat {
	u32 fid;
};

struct p9_rstat {
	struct p9_wstat stat;
};

struct p9_twstat {
	u32 fid;
	struct p9_wstat stat;
};

struct p9_rwstat {
};

/**
 * struct p9_fcall - primary packet structure
 * @size: prefixed length of the structure
 * @id: protocol operating identifier of type &p9_msg_t
 * @tag: transaction id of the request
 * @offset: used by marshalling routines to track currentposition in buffer
 * @capacity: used by marshalling routines to track total capacity
 * @sdata: payload
 *
 * &p9_fcall represents the structure for all 9P RPC
 * transactions.  Requests are packaged into fcalls, and reponses
 * must be extracted from them.
 *
 * See Also: http://plan9.bell-labs.com/magic/man2html/2/fcall
 */

struct p9_fcall {
	u32 size;
	u8 id;
	u16 tag;

	size_t offset;
	size_t capacity;

	uint8_t *sdata;
};

struct p9_idpool;

int p9_errstr2errno(char *errstr, int len);

struct p9_idpool *p9_idpool_create(void);
void p9_idpool_destroy(struct p9_idpool *);
int p9_idpool_get(struct p9_idpool *p);
void p9_idpool_put(int id, struct p9_idpool *p);
int p9_idpool_check(int id, struct p9_idpool *p);

int p9_error_init(void);
int p9_errstr2errno(char *, int);
int p9_trans_fd_init(void);
void p9_trans_fd_exit(void);
#endif /* NET_9P_H */
