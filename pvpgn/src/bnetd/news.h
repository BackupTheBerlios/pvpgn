/*
 * Copyright (C) 2000 Alexey Belyaev (spider@omskart.ru)
 */

#ifndef INCLUDED_NEWS_TYPES
#define INCLUDED_NEWS_TYPES

typedef struct news_index
#ifdef NEWS_INTERNAL_ACCESS
{
	unsigned int 	date;
	char		*body;
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

extern unsigned int news_get_firstnews(void);
extern unsigned int news_get_lastnews(void);
extern char * news_get_body(t_news_index const * news);
extern unsigned int news_get_date(t_news_index const * news);

extern t_list * newslist(void);

#endif
#endif
