/*
 * Copyright (C) 2000 Alexey Belyaev (spider@omskart.ru)
 */

#ifndef INCLUDED_NEWS_TYPES
#define INCLUDED_NEWS_TYPES

typedef struct news_index
#ifdef NEWS_INTERNAL_ACCESS
{
	long	date;
	long	offset;
	unsigned short	size;
}
#endif
t_news_index;

#endif

/*****/
#ifndef JUST_NEED_TYPES
#ifndef NEWS_INTERNAL_ACCESS
#define NEWS_INTERNAL_ACCESS

extern int news_load(const char *filename);
extern int news_unload(void);

extern char const * news_get_body(t_news_index const * news);
extern int news_unget_body(char const * val);
extern unsigned int const news_get_date(t_news_index const * news);

extern t_list * newslist(void);

#endif
#endif
