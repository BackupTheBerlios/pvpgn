#ifndef INCLUDED_HANDLE_SIGNAL_H
#define INCLUDED_HANDLE_SIGNAL_H

#ifndef WIN32
 extern int d2dbs_handle_signal_init(void);
#else
 extern void d2dbs_signal_quit_wrapper(void);
#endif

extern int d2dbs_handle_signal(void);

#endif
