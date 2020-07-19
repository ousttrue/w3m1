/* $Id: func.c,v 1.27 2003/09/26 17:59:51 ukai Exp $ */
/*
 * w3m func.c
 */

#include <stdio.h>


#include "fm.h"
#include "indep.h"
#include "rc.h"
#include "myctype.h"
#include "dispatcher.h"
#include "commands.h"
#include "symbol.h"


//
// mouse
//
static MouseAction default_mouse_action = {
	NULL,
	"<=UpDn",
	0,
	6,
	FALSE,
	0,
	0,
	{{movMs, NULL}, {backBf, NULL}, {menuMs, NULL}}, /* default */
	{{NULL, NULL}, {NULL, NULL}, {NULL, NULL}},		 /* anchor */
	{{followA, NULL}, {NULL, NULL}, {NULL, NULL}},	 /* active */
	{{tabMs, NULL}, {closeTMs, NULL}, {NULL, NULL}}, /* tab */
	{NULL, NULL, NULL},								 /* menu */
	{NULL, NULL, NULL}								 /* lastline */
};
static MouseActionMap default_lastline_action[6] = {
	{backBf, NULL},
	{backBf, NULL},
	{pgBack, NULL},
	{pgBack, NULL},
	{pgFore, NULL},
	{pgFore, NULL}};

static void
setMouseAction0(char **str, int *width, MouseActionMap **map, char *p)
{
	char *s;
	int b, w, x;

	s = getQWord(&p);
	if (!*s)
	{
		*str = NULL;
		width = 0;
		for (b = 0; b < 3; b++)
			map[b] = NULL;
		return;
	}
	w = *width;
	*str = s;
	*width = get_strwidth(s);
	if (*width >= LIMIT_MOUSE_MENU)
		*width = LIMIT_MOUSE_MENU;
	if (*width <= w)
		return;
	for (b = 0; b < 3; b++)
	{
		if (!map[b])
			continue;
		map[b] = New_Reuse(MouseActionMap, map[b], *width);
		for (x = w + 1; x < *width; x++)
		{
			map[b][x].func = NULL;
			map[b][x].data = NULL;
		}
	}
}

static void
setMouseAction1(MouseActionMap **map, int width, char *p)
{
	char *s;
	int x, x2;

	if (!*map)
	{
		*map = New_N(MouseActionMap, width);
		for (x = 0; x < width; x++)
		{
			(*map)[x].func = NULL;
			(*map)[x].data = NULL;
		}
	}
	s = getWord(&p);
	x = atoi(s);
	if (!(IS_DIGIT(*s) && x >= 0 && x < width))
		return; /* error */
	s = getWord(&p);
	x2 = atoi(s);
	if (!(IS_DIGIT(*s) && x2 >= 0 && x2 < width))
		return; /* error */
	s = getWord(&p);
	Command f = getFuncList(s);
	s = getQWord(&p);
	if (!*s)
		s = NULL;
	for (; x <= x2; x++)
	{
		(*map)[x].func = f;
		(*map)[x].data = s;
	}
}

static void
setMouseAction2(MouseActionMap *map, char *p)
{
	char *s;

	s = getWord(&p);
	Command f = getFuncList(s);
	s = getQWord(&p);
	if (!*s)
		s = NULL;
	map->func = f;
	map->data = s;
}

static void
interpret_mouse_action(FILE *mf)
{
	Str line;
	char *p, *s;
	int b;

	while (!feof(mf))
	{
		line = Strfgets(mf);
		Strchop(line);
		Strremovefirstspaces(line);
		if (line->length == 0)
			continue;
		p = conv_from_system(line->ptr);
		s = getWord(&p);
		if (*s == '#') /* comment */
			continue;
		if (!strcmp(s, "menu"))
		{
			setMouseAction0(&mouse_action.menu_str, &mouse_action.menu_width,
							mouse_action.menu_map, p);
			continue;
		}
		else if (!strcmp(s, "lastline"))
		{
			setMouseAction0(&mouse_action.lastline_str,
							&mouse_action.lastline_width,
							mouse_action.lastline_map, p);
			continue;
		}
		if (strcmp(s, "button"))
			continue; /* error */
		s = getWord(&p);
		b = atoi(s) - 1;
		if (!(b >= 0 && b <= 2))
			continue; /* error */
		SKIP_BLANKS(p);
		if (IS_DIGIT(*p))
			s = "menu";
		else
			s = getWord(&p);
		if (!strcasecmp(s, "menu"))
		{
			if (!mouse_action.menu_str)
				continue;
			setMouseAction1(&mouse_action.menu_map[b], mouse_action.menu_width,
							p);
		}
		else if (!strcasecmp(s, "lastline"))
		{
			if (!mouse_action.lastline_str)
				continue;
			setMouseAction1(&mouse_action.lastline_map[b],
							mouse_action.lastline_width, p);
		}
		else if (!strcasecmp(s, "default"))
			setMouseAction2(&mouse_action.default_map[b], p);
		else if (!strcasecmp(s, "anchor"))
			setMouseAction2(&mouse_action.anchor_map[b], p);
		else if (!strcasecmp(s, "active"))
			setMouseAction2(&mouse_action.active_map[b], p);
		else if (!strcasecmp(s, "tab"))
			setMouseAction2(&mouse_action.tab_map[b], p);
	}
}

void initMouseAction(void)
{
	FILE *mf;

	bcopy((void *)&default_mouse_action, (void *)&mouse_action,
		  sizeof(default_mouse_action));
	mouse_action.lastline_map[0] = New_N(MouseActionMap, 6);
	bcopy((void *)&default_lastline_action,
		  (void *)mouse_action.lastline_map[0],
		  sizeof(default_lastline_action));
	{
#ifdef USE_M17N
		int w = 0;
		char **symbol = get_symbol(DisplayCharset, &w);
#else
		char **symbol = get_symbol();
#endif
		mouse_action.lastline_str =
			Strnew_charp(symbol[N_GRAPH_SYMBOL + 13])->ptr;
	}

	if ((mf = fopen(confFile(MOUSE_FILE), "rt")) != NULL)
	{
		interpret_mouse_action(mf);
		fclose(mf);
	}
	if ((mf = fopen(rcFile(MOUSE_FILE), "rt")) != NULL)
	{
		interpret_mouse_action(mf);
		fclose(mf);
	}
}
