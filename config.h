#define CMDLENGTH 50
#define LEADING_DELIMITER
#define CLICKABLE_BLOCKS 1
#define BLOCK(cmd, interval, signal) {"echo \"$(" cmd ")\"", interval, signal},

static const Block blocks[] = {
	/* Command			Update Interval	Update Signal*/
	BLOCK("sb-clock",				30,		1)
	BLOCK("sb-disk",				9000,		2)
	BLOCK("sb-battery",				10,		3)
	BLOCK("sb-internet",			10,		4)
	BLOCK("sb-mailbox",				0,		5)
	BLOCK("sb-moonphase",			18000,		6)
	BLOCK("sb-forecast",			18000,		7)
	BLOCK("sb-volume",				0,		8)
//	BLOCK("sb-price btc Bitcoin 💰",		9000,		21)
//	BLOCK("sb-price eth Ethereum 🍸",		9000,		23)
//	BLOCK("sb-price xmr \"Monero\" 🔒",		9000,		24)
//	BLOCK("sb-price link \"Chainlink\" 🔗",	300,		25)
//{ "sb-BLOCK(ice bat \"Basic Attention Token\" 🦁",9000,		20)
//	BLOCK("sb-price lbc \"LBRY Token\" 📚",	9000,		22)
//	BLOCK("sb-cpu",				10,		18)
//	BLOCK("sb-kbselect",			0,		30)
//	BLOCK("sb-memory",				10,		14)
//	BLOCK("sb-torrent",				20,		7)
//	BLOCK("sb-crypto",				0,		13)
//	BLOCK("sb-help-icon",			0,		15)
//	BLOCK("sb-nettraf",				1,		16)
//	BLOCK("sb-news",				0,		6)
//	BLOCK("sb-xbpsup",				18000,		8)
	BLOCK("sb-pacpackages",			0,		9)
	BLOCK("sb-sync",				0,		10)
//	BLOCK("sb-mpc",				0,		26)
	BLOCK("sb-music",				0,		11)
	BLOCK("sb-tasks",				10,		12)
	BLOCK("sb-notes",				0,		13)
	BLOCK("cat /tmp/recordingicon 2>/dev/null",	0,		14)
//	BLOCK("sb-count",				0,		21)
};

/* delimiter between status commands, NULL character ('\0') means no delimiter */
static const char delimiter[] = " ";

static const char *shell = "/bin/dash";
