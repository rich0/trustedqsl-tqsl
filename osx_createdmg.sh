#!/bin/sh

TQSLVER=`cat apps/tqslversion.ver|sed -e 's/\.0$//'`
TQSLLIBPATH=`pwd`/src/libtqsllib.dylib
WORKDIR=`mktemp -d /tmp/tqsl.XXXXX` || exit 1
WINHELPFILE=$WORKDIR/TrustedQSL/$app.app/Contents/Resources/Help/tqslapp.chm
IMGNAME="tqsl"

file apps/tqsl.app/Contents/MacOS/tqsl | grep -q x86_64 || IMGNAME="tqsl-legacy"

/bin/echo -n "Copying files to image directory... "

cp apps/ChangeLog.txt $WORKDIR/ChangeLog.txt
cp LICENSE.txt $WORKDIR/
cp apps/quick "$WORKDIR/Quick Start.txt"
mkdir $WORKDIR/TrustedQSL
cp -r apps/tqsl.app $WORKDIR/TrustedQSL

/bin/echo "done"

/bin/echo -n "Installing the libraries and tweaking the binaries to look for them... "

for app in tqsl
do
    cp $TQSLLIBPATH $WORKDIR/TrustedQSL/$app.app/Contents/MacOS
    [ -f $WINHELPFILE ] && rm $WINHELPFILE
    install_name_tool -change $TQSLLIBPATH @executable_path/libtqsllib.dylib $WORKDIR/TrustedQSL/$app.app/Contents/MacOS/$app
    cp src/config.xml $WORKDIR/TrustedQSL/$app.app/Contents/Resources
    cp apps/ca-bundle.crt $WORKDIR/TrustedQSL/$app.app/Contents/Resources
    cp apps/languages.dat $WORKDIR/TrustedQSL/$app.app/Contents/Resources
    cp apps/cab_modes.dat $WORKDIR/TrustedQSL/$app.app/Contents/Resources
    for lang in de es fi fr hi_IN it ja pl_PL pt zh ru sv_SE tr_TR
    do
	mkdir $WORKDIR/TrustedQSL/$app.app/Contents/Resources/$lang.lproj
	cp apps/lang/$lang/tqslapp.mo $WORKDIR/TrustedQSL/$app.app/Contents/Resources/$lang.lproj
	cp apps/lang/$lang/wxstd.mo $WORKDIR/TrustedQSL/$app.app/Contents/Resources/$lang.lproj
    done
# Make an empty 'en.lproj' folder so wx knows it's default
    mkdir $WORKDIR/TrustedQSL/$app.app/Contents/Resources/en.lproj
done

/bin/echo "done"

/bin/echo -n "Installing the help... "

cp -r apps/help/tqslapp $WORKDIR/TrustedQSL/tqsl.app/Contents/Resources/Help

/bin/echo "done"

/bin/echo "Creating the disk image..."

#hdiutil uses dots to show progress
hdiutil create -ov -srcfolder $WORKDIR -volname "TrustedQSL v$TQSLVER" "$IMGNAME-$TQSLVER.dmg"

/bin/echo -n "Cleaning up temporary files.. "
rm -r $WORKDIR
/bin/echo "Finished!"
