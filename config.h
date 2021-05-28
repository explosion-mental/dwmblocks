//Modify this file to change what commands output to your statusbar, and recompile using the make command.
static const Block blocks[] = {
	/*Command*/			/*Update Interval*//*Update Signal*/
//	{ "sb-kbselect",			0,	30},
	{ "cat /tmp/recordingicon 2>/dev/null",	0,	9},
	{ "sb-notes",				0,	30},
	{ "sb-tasks",				10,	3},
	{ "sb-music",				0,	11},
//	{ "sb-mpc",				0,	26},
//	{ "sb-pacpackages",			0,	8},
	{ "sb-sync",				0,	21},
//	{ "sb-pacpackages",			0,	8},
//	{ "sb-xbpsup",				18000,	8},
//	{ "sb-news",				0,	6},
	{ "sb-volume",				0,	10},
	{ "sb-forecast",			18000,	5},
	{ "sb-moonphase",			18000,	17},
//	{ "sb-mailbox",				180,	12},
//	{ "sb-nettraf",				1,	16},
	{ "sb-battery",				10,	7},
	{ "sb-clock",				60,	1},
//	{ "sb-internet",			5,	4},
//	{ "sb-help-icon",			0,	15},
	/* {"",	"sb-crypto",	0,	13}, */
	/* {"",	"sb-price lbc \"LBRY Token\" 📚",			9000,	22}, */
	/* {"",	"sb-price bat \"Basic Attention Token\" 🦁",	9000,	20}, */
	/* {"",	"sb-price link \"Chainlink\" 🔗",			300,	25}, */
	/* {"",	"sb-price xmr \"Monero\" 🔒",			9000,	24}, */
	/* {"",	"sb-price eth Ethereum 🍸",	9000,	23}, */
	/* {"",	"sb-price btc Bitcoin 💰",				9000,	21}, */
//	{"",	"sb-torrent",	20,	7},
	/* {"",	"sb-memory",	10,	14}, */
	/* {"",	"sb-cpu",		10,	18}, */
};
/* Sets delimiter between status commands. NULL character ('\0') means no delimiter */
static char *delim = " ";
