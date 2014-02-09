struct JKID_NAME_ELEM {
	int jkID;
	LPCTSTR name;
	struct COMPARE {
		bool operator()(const JKID_NAME_ELEM &l, const JKID_NAME_ELEM &r) { return l.jkID < r.jkID; }
	};
};

static const JKID_NAME_ELEM DEFAULT_JKID_NAME_TABLE[] = {
	// ニコニコ実況チャンネルリスト(2013-01-02時点)(jkIDでソート必須)
	{   1, TEXT("NHK 総合") },
	{   2, TEXT("Eテレ") },
	{   4, TEXT("日本テレビ") },
	{   5, TEXT("テレビ朝日") },
	{   6, TEXT("TBS テレビ") },
	{   7, TEXT("テレビ東京") },
	{   8, TEXT("フジテレビ") },
	{   9, TEXT("TOKYO MX") },
	{  10, TEXT("テレ玉") },
	{  11, TEXT("tvk") },
	{  12, TEXT("チバテレビ") },
	{ 101, TEXT("NHKBS-1") },
	{ 103, TEXT("NHK BSプレミアム") },
	{ 141, TEXT("BS 日テレ") },
	{ 151, TEXT("BS 朝日") },
	{ 161, TEXT("BS-TBS") },
	{ 171, TEXT("BSジャパン") },
	{ 181, TEXT("BSフジ") },
	{ 191, TEXT("WOWOWプライム") },
	{ 192, TEXT("WOWOWライブ") },
	{ 193, TEXT("WOWOWシネマ") },
	{ 200, TEXT("スターチャンネル1") },
	{ 201, TEXT("スターチャンネル2") },
	{ 202, TEXT("スターチャンネル3") },
	{ 211, TEXT("BSイレブン") },
	{ 222, TEXT("TwellV") },
	{ 231, TEXT("放送大学") },
	{ 234, TEXT("BSグリーンチャンネル") },
	{ 236, TEXT("BSアニマックス") },
	{ 238, TEXT("FOX bs 238") },
	{ 241, TEXT("BSスカパー!") },
	{ 242, TEXT("J Sports 1") },
	{ 243, TEXT("J Sports 2") },
	{ 244, TEXT("J Sports 3") },
	{ 245, TEXT("J Sports 4") },
	{ 251, TEXT("BS釣りビジョン") },
	{ 252, TEXT("IMAGICA BS") },
	{ 255, TEXT("BS日本映画専門チャンネル") },
	{ 256, TEXT("ディズニー・チャンネル") },
	{ 258, TEXT("Dlife") },
	{ 910, TEXT("SOLiVE24") },
};
