#ifndef INCLUDED_HANDLE_SIGNAL_H
#define INCLUDED_HANDLE_SIGNAL_H

#ifndef WIN32
extern int handle_signal_init(void);
#else
extern void signal_quit_wrapper(void);
#endif

extern int handle_signal(void);

#endif
