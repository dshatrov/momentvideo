EXTRA_DIST = \
	myplayer.html

MyPlayer.swf: MyPlayer.as
	/opt/flex_4.0.0/bin/mxmlc -headless-server=true -static-link-runtime-shared-libraries=true -target-player=10.0 $^
#	/opt/flex/bin/mxmlc -headless-server=true -static-link-runtime-shared-libraries=true $^

myplayerdir = $(datadir)/moment/myplayer
myplayer_DATA = index.tpl		\
		MyPlayer.swf            \
		playlist.json.tpl	\
		splash.png		\
		buffering.png		\
		connecting.png		\
		error.png		\
		horizontal.png		\
		fullscreen.png		\
		playlist.png		\
		favicon.ico

