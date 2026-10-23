/* user and group to drop privileges to */
static const char *user  = "nobody";
static const char *group = "nogroup";

static const char *fontname = "monospace:size=14";
static const int promptwidth = 300;
static const char *colorname[NUMCOLS] = {
	[Background] = "#000000",
	[Field] = "#222222",
	[Text] = "#bbbbbb",
	[Accent] = "#666666",
	[SelectedText] = "#eeeeee",
	[Error] = "#e06c75",
};

/* seconds without input while locked, before turning the display off. */
static const int timeoff = 300;
