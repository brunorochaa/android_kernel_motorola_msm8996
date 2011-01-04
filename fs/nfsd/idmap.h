/*
 *  Mapping of UID to name and vice versa.
 *
 *  Copyright (c) 2002, 2003 The Regents of the University of
 *  Michigan.  All rights reserved.
> *
 *  Marius Aamodt Eriksen <marius@umich.edu>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the University nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LINUX_NFSD_IDMAP_H
#define LINUX_NFSD_IDMAP_H

#include <linux/in.h>
#include <linux/sunrpc/svc.h>

/* XXX from linux/nfs_idmap.h */
#define IDMAP_NAMESZ 128

#ifdef CONFIG_NFSD_V4
int nfsd_idmap_init(void);
void nfsd_idmap_shutdown(void);
#else
static inline int nfsd_idmap_init(void)
{
	return 0;
}
static inline void nfsd_idmap_shutdown(void)
{
}
#endif

int nfsd_map_name_to_uid(struct svc_rqst *, const char *, size_t, __u32 *);
int nfsd_map_name_to_gid(struct svc_rqst *, const char *, size_t, __u32 *);
int nfsd_map_uid_to_name(struct svc_rqst *, __u32, char *);
int nfsd_map_gid_to_name(struct svc_rqst *, __u32, char *);

#endif /* LINUX_NFSD_IDMAP_H */
