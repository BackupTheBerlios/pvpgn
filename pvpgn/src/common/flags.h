#ifndef INCLUDED_FLAGS_H
#define INCLUDED_FLAGS_H

#define FLAG_ZERO(var)	*(var) = 0
#define FLAG_SET(var,flag) *(var) |= flag
#define FLAG_CLEAR(var,flag) *(var) &= ~flag
#define FLAG_ISSET(var,flag) (var & flag)

#endif /* INCLUDED_FLAGS_H */
