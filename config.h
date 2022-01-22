#define POLL_INTERVAL 50
#define CMDLENGTH	60
#define LEADING_DELIMITER
#define INVERSED

/* delimiter between blocks */
static const char delimiter[] = " ";

/* default shell to use when running the commands */
static const char shell[] = "/bin/dash";

/* 1 means clickable blocks */
static const int clickable = 1;

static const Block blocks[] = {
	/* command			update interval	update signal*/
	{ "sb-clock",				30,		1},
	{ "sb-disk",				9000,		2},
	{ "sb-battery",				10,		3},
	{ "sb-internet",			5,		4},
	{ "sb-mailbox",				0,		5},
	{ "sb-moonphase",			18000,		6},
	{ "sb-forecast",			18000,		7},
	{ "sb-volume",				0,		8},
//	{ "sb-price btc Bitcoin 💰",		9000,		21},
//	{ "sb-price eth Ethereum 🍸",		9000,		23},
//	{ "sb-price xmr \"Monero\" 🔒",		9000,		24},
//	{ "sb-price link \"Chainlink\" 🔗",	300,		25},
//{ "sb-price bat \"Basic Attention Token\" 🦁",9000,		20},
//	{ "sb-price lbc \"LBRY Token\" 📚",	9000,		22},
//	{ "sb-cpu",				10,		18},
//	{ "sb-kbselect",			0,		30},
//	{ "sb-memory",				10,		14},
//	{ "sb-torrent",				20,		7},
//	{ "sb-crypto",				0,		13},
//	{ "sb-help-icon",			0,		15},
//	{ "sb-nettraf",				1,		16},
//	{ "sb-news",				0,		6},
//	{ "sb-xbpsup",				18000,		8},
	{ "sb-pacpackages",			0,		9},
	{ "sb-sync",				0,		10},
//	{ "sb-mpc",				0,		26},
	{ "sb-music",				0,		11},
//	{ "sb-tasks",				10,		12},
	{ "sb-notes",				0,		13},
	{ "cat /tmp/recordingicon 2>/dev/null",	0,		14},
//	{ "sb-count",				0,		21},
};
