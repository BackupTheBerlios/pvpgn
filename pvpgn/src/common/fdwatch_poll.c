/*
  * Abstraction API/layer for the various ways PvPGN can inspect sockets state
  * 2003 (C) 
  *
  * Code is based on the ideas found in thttpd project.
  *
  * poll() based backend
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the GNU General Public License
  * as published by the Free Software Foundation; either version 2
  * of the License, or (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  */

#include "common/setup_before.h"
#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#ifdef HAVE_POLL_H
# include <poll.h>
#else
# ifdef HAVE_SYS_POLL_H
#  include <sys/poll.h>
# endif
#endif
#include "fdwatch.h"
#include "common/eventlog.h"
#include "common/setup_after.h"

#ifdef HAVE_POLL
static int sr;
static struct pollfd *fds = NULL, /* working set */
		     *tfds = NULL; /* template */
static int *fdw_ridx = NULL;
static unsigned nofds;

static int fdw_poll_init(int nfds);
static int fdw_poll_close(void);
static int fdw_poll_add_fd(int fd, t_fdwatch_type rw);
static int fdw_poll_del_fd(int fd);
static int fdw_poll_watch(long timeout_msecs);
static void fdw_poll_handle(void);

t_fdw_backend fdw_poll = {
    fdw_poll_init,
    fdw_poll_close,
    fdw_poll_add_fd,
    fdw_poll_del_fd,
    fdw_poll_watch,
    fdw_poll_handle
};

static int fdw_poll_init(int nfds)
{
    int i;

    fdw_ridx = malloc(sizeof(int) * nfds);
    fds = malloc(sizeof(struct pollfd) * nfds);
    tfds = malloc(sizeof(struct pollfd) * nfds);
    if (fdw_ridx == NULL || fds == NULL || tfds == NULL) {
	fdw_poll_close();
	return -1;
    }

    memset(tfds, 0, sizeof(struct pollfd) * nfds);
/* I would use a memset with 255 but that is dirty and doesnt gain us anything */
    for(i = 0; i < nfds; i++) fdw_ridx[i] = -1;
    nofds = sr = 0;

    eventlog(eventlog_level_info, __FUNCTION__, "fdwatch poll() based layer initialized (max %d sockets)", nfds);
    return 0;
}

static int fdw_poll_close(void)
{
    if (tfds) { free((void *)tfds); tfds = NULL; }
    if (fds) { free((void *)fds); fds = NULL; }
    if (fdw_ridx) { free((void *)fdw_ridx); fdw_ridx = NULL; }
    nofds = sr = 0;

    return 0;
}

static int fdw_poll_add_fd(int fd, t_fdwatch_type rw)
{
    static int ridx;

//    eventlog(eventlog_level_trace, __FUNCTION__, "called fd: %d rw: %d", fd, rw);
    if (fdw_ridx[fd] < 0) {
	ridx = nofds++;
	tfds[ridx].fd = fd;
	fdw_ridx[fd] = ridx;
//	eventlog(eventlog_level_trace, __FUNCTION__, "adding new fd on %d", ridx);
    } else {
	if (tfds[fdw_ridx[fd]].fd != fd) {
	    return -1;
	}
	ridx = fdw_ridx[fd];
//	eventlog(eventlog_level_trace, __FUNCTION__, "updating fd on %d", ridx);
    }

    tfds[ridx].events = 0;
    if (rw & fdwatch_type_read) tfds[ridx].events |= POLLIN;
    if (rw & fdwatch_type_write) tfds[ridx].events |= POLLOUT;

    return 0;
}

static int fdw_poll_del_fd(int fd)
{
//    eventlog(eventlog_level_trace, __FUNCTION__, "called fd: %d", fd);
    if (fdw_ridx[fd] < 0 || !nofds) return -1;
    if (sr > 0) 
	eventlog(eventlog_level_error, __FUNCTION__, "BUG: called while still handling sockets");

    /* move the last entry to the deleted one and decrement nofds count */
    nofds--;
    if (fdw_ridx[fd] < nofds) {
//	eventlog(eventlog_level_trace, __FUNCTION__, "not last, moving %d", tfds[nofds].fd);
	fdw_ridx[tfds[nofds].fd] = fdw_ridx[fd];
	memcpy(tfds + fdw_ridx[fd], tfds + nofds, sizeof(struct pollfd));
    }
    fdw_ridx[fd] = -1;

    return 0;
}

static int fdw_poll_watch(long timeout_msec)
{
    memcpy(fds, tfds, sizeof(struct pollfd) * nofds);
    return (sr = poll(fds, nofds, timeout_msec));
}

static void fdw_poll_handle(void)
{
    register unsigned i;
    int changed;

    for(i = 0; i < nofds && sr; i++) {
	changed = 0;

	if (fdw_rw[fds[i].fd] & fdwatch_type_read && 
	    fds[i].revents & (POLLIN  | POLLERR | POLLHUP | POLLNVAL))
	{
	    if (fdw_hnd[fds[i].fd](fdw_data[fds[i].fd], fdwatch_type_read) == -2) {
		sr--;
		continue;
	    }
	    changed = 1;
	}

	if (fdw_rw[fds[i].fd] & fdwatch_type_write && 
	    fds[i].revents & (POLLOUT  | POLLERR | POLLHUP | POLLNVAL))
	{
	    fdw_hnd[fds[i].fd](fdw_data[fds[i].fd], fdwatch_type_write);
	    changed = 1;
	}

	if (changed) sr--;
    }
}

#endif /* HAVE_POLL */
