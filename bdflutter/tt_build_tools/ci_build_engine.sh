#!/usr/bin/env bash

# ninja
# brew install ninja

# ant
# brew install ant

# gclient
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

export PATH=`pwd`/depot_tools:$PATH

mkdir engine
(
cat <<EOF
solutions = [
  {
    "managed": False,
    "name": "src/flutter",
    "url": "git@code.byted.org:tech_client/flutter_engine.git",
    "custom_deps": {},
    "deps_file": "DEPS",
    "safesync_url": "",
  },
]
EOF
) > engine/.gclient

cd engine
if [ -d "src/" ];then
	cd src
    git checkout master
	git reset --hard head
	git clean -fd
	git pull
	cd ..
fi

if [ ! -d "src/flutter" ];then
    gclient sync -D -f
fi

cd src
git fetch
git checkout -f origin/master

cd flutter
git fetch
git reset --hard origin/$BRANCH
git clean -fd

gclient sync -D -f

cd bdflutter/tt_build_tools

[ -e upload.zip ] && rm -rf upload.zip
download_status=$(curl -o upload.zip "http://tosv.byted.org/obj/toutiao.ios.arch/flutter/upload.zip" -w %{http_code})
echo "上传脚本更新结果: ${download_status}"
unzip -oq upload.zip -d upload

LITEMODE='normal,lite'

bash android_build.sh $JCOUNT $MODE $LITEMODE
if [ $? -ne 0 ]; then
	echo "android_build Compile failed !"
	exit 1
fi

bash iOS_build.sh $JCOUNT '' $LITEMODE
if [ $? -ne 0 ]; then
	echo "iOS_build Compile failed !"
	exit 1
fi

liteModes=(${LITEMODE//,/ })
if [ ${#liteModes[@]} == 0 ];then
    liteModes=('normal')
fi
RUNTIME_MODE="debug profile release"
for runtime_mode in ${RUNTIME_MODE}; do
  for liteMode in ${liteModes[@]}; do
    if [ "${liteMode}" != 'normal' ]; then
      liteModeSuffix=_${liteMode}
    fi
    curl -F "uuid=default" -F "type=Native" -F "file=@../../out/android_${runtime_mode}${liteModeSuffix}/libflutter.so" http://symbolicate.byted.org/android_upload
    curl -F "uuid=default" -F "type=Native" -F "file=@../../out/android_${runtime_mode}_arm64${liteModeSuffix}/libflutter.so" http://symbolicate.byted.org/android_upload
    curl -F "uuid=default" -F "type=Native" -F "file=@../../out/android_${runtime_mode}_dynamicart${liteModeSuffix}/libflutter.so" http://symbolicate.byted.org/android_upload
    curl -F "uuid=default" -F "type=Native" -F "file=@../../out/android_${runtime_mode}_arm64_dynamicart${liteModeSuffix}/libflutter.so" http://symbolicate.byted.org/android_upload
  done
done
