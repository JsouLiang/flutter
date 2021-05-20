#!/usr/bin/env bash

gitCid=`git log -1 --pretty=%H`
gitUser=`git log -1 --pretty=%an`
gitMessage=`git log -1 --pretty=%B`
gitDate=`git log -1 --pretty=%ad`
echo "commit is $gitCid"
echo "user is $gitUser"
echo "gitMessage is $gitMessage"

source $(cd "$(dirname "$0")";pwd)/utils.sh

cd ..

jcount=$1
if [ ! $jcount ]
then
    jcount=4
fi

isFast=$2

# 现在fast和非fast相同
if [ $isFast = 'fast' ]; then
    platforms=('arm' 'arm64' 'x64' 'x86')
    dynamics=('normal' 'dynamicart')
else
    platforms=('arm' 'x64' 'x86' 'arm64')
    dynamics=('normal' 'dynamicart')
fi

tosDir=$(git rev-parse HEAD)

liteModeArg=$3
liteModes=(${liteModeArg//,/ })
if [ ${#liteModes[@]} == 0 ];then
    liteModes=('normal')
fi
echo "Android build modes: ${liteModes[@]}"

function checkResult() {
    if [ $? -ne 0 ]; then
        echo "Host debug compile failed !"
        exit 1
    fi
}

cd ..
cd ..

cacheDir=out/tt_android_cache
rm -rf $cacheDir
mkdir $cacheDir

if [ "$(uname)" == "Darwin" ]
then
  ./flutter/tools/gn --no-lto --full-dart-sdk
  ninja -C out/host_debug -j $jcount
  checkResult

  # flutter_patched_sdk.zip
  rm -f $cacheDir/flutter_patched_sdk.zip
  cd out/host_debug
  zip -rq ../../$cacheDir/flutter_patched_sdk.zip flutter_patched_sdk
  cd ..
  cd ..
  bd_upload $cacheDir/flutter_patched_sdk.zip flutter/framework/$tosDir/flutter_patched_sdk.zip


  # dart-sdk-darwin-x64.zip
  cd out/host_debug
  zip -rq ../../$cacheDir/dart-sdk-darwin-x64.zip dart-sdk
  cd ..
  cd ..
  bd_upload $cacheDir/dart-sdk-darwin-x64.zip flutter_infra/flutter/$tosDir/dart-sdk-darwin-x64.zip

  # font-subset.zip
  cd out/host_debug
  zip -rq ../../$cacheDir/font-subset.zip font-subset
  cd ..
  cd ..
  bd_upload $cacheDir/font-subset.zip flutter/framework/$tosDir/darwin-x64/font-subset.zip

  # flutter_web_sdk.zip
  cd out/host_debug
  zip -rq ../../$cacheDir/flutter-web-sdk-darwin-x64.zip flutter_web_sdk
  cd ..
  cd ..
  bd_upload $cacheDir/flutter-web-sdk-darwin-x64.zip flutter/framework/$tosDir/flutter-web-sdk-darwin-x64.zip

  # FlutterMacOS.framework.zip
  cd out/host_debug
  zip -rq ../../$cacheDir/FlutterMacOS.framework.zip FlutterMacOS.framework
  cd ..
  cd ..
  bd_upload $cacheDir/FlutterMacOS.framework.zip flutter/framework/$tosDir/darwin-x64/FlutterMacOS.framework.zip
  # darwin-x64-profile/FlutterMacOS.framework
  bd_upload $cacheDir/FlutterMacOS.framework.zip flutter/framework/$tosDir/darwin-x64-profile/FlutterMacOS.framework.zip

  ./flutter/tools/gn --runtime-mode=release --no-lto
  ninja -C out/host_release -j $jcount
  checkResult

  # flutter_patched_sdk_product.zip
  rm -f $cacheDir/flutter_patched_sdk_product.zip
  rm -rf $cacheDir/flutter_patched_sdk_product
  cp -r out/host_release/flutter_patched_sdk $cacheDir/flutter_patched_sdk_product
  cd $cacheDir
  zip -rq flutter_patched_sdk_product.zip flutter_patched_sdk_product
  cd ..
  cd ..

  bd_upload $cacheDir/flutter_patched_sdk_product.zip flutter/framework/$tosDir/flutter_patched_sdk_product.zip

  # FlutterMacOS.framework.zip
  cd out/host_release
  zip -rq ../../$cacheDir/FlutterMacOS.framework.zip FlutterMacOS.framework
  cd ..
  cd ..
  bd_upload $cacheDir/FlutterMacOS.framework.zip flutter/framework/$tosDir/darwin-x64-release/FlutterMacOS.framework.zip

  # darwin-x64.zip
  modeDir=darwin-x64
  rm -rf $cacheDir/$modeDir
  mkdir $cacheDir/$modeDir
  cp out/host_release/gen/flutter/lib/snapshot/isolate_snapshot.bin $cacheDir/$modeDir/product_isolate_snapshot.bin
  cp out/host_release/gen/flutter/lib/snapshot/vm_isolate_snapshot.bin $cacheDir/$modeDir/product_vm_isolate_snapshot.bin
  zip -rjq $cacheDir/$modeDir/artifacts.zip out/host_debug/flutter_tester out/host_debug/gen/frontend_server.dart.snapshot \
  third_party/icu/flutter/icudtl.dat out/host_debug/gen/flutter/lib/snapshot/isolate_snapshot.bin \
  out/host_debug/gen/flutter/lib/snapshot/vm_isolate_snapshot.bin out/host_debug/gen/const_finder.dart.snapshot $cacheDir/$modeDir/product_isolate_snapshot.bin \
  $cacheDir/$modeDir/product_vm_isolate_snapshot.bin out/host_debug/gen_snapshot
  bd_upload $cacheDir/$modeDir/artifacts.zip flutter/framework/$tosDir/$modeDir/artifacts.zip
  # darwin-x64-profile
  bd_upload $cacheDir/$modeDir/artifacts.zip flutter/framework/$tosDir/darwin-x64-profile/artifacts.zip
  # darwin-x64-release
  bd_upload $cacheDir/$modeDir/artifacts.zip flutter/framework/$tosDir/darwin-x64-release/artifacts.zip

  rm -rf $cacheDir/pkg
  mkdir $cacheDir/pkg
  cp -rf out/host_debug/gen/dart-pkg/sky_engine $cacheDir/pkg/sky_engine
  rm -rf $cacheDir/pkg/sky_engine/packages
  cd $cacheDir/pkg
  zip -rq ../../../$cacheDir/pkg/sky_engine.zip sky_engine
  cd ..
  cd ..
  cd ..
  bd_upload $cacheDir/pkg/sky_engine.zip flutter/framework/$tosDir/sky_engine.zip
else
  ./flutter/tools/gn --no-lto --full-dart-sdk
  ninja -C out/host_debug -j $jcount
  checkResult

  # dart-sdk-linux-x64.zip
  cd out/host_debug
  zip -rq ../../$cacheDir/dart-sdk-linux-x64.zip dart-sdk
  cd ..
  cd ..
  bd_upload $cacheDir/dart-sdk-linux-x64.zip flutter_infra/flutter/$tosDir/dart-sdk-linux-x64.zip

  # font-subset.zip
  cd out/host_debug
  zip -rq ../../$cacheDir/font-subset.zip font-subset
  cd ..
  cd ..
  bd_upload $cacheDir/font-subset.zip flutter/framework/$tosDir/linux-x64/font-subset.zip

  # flutter_web_sdk.zip
  cd out/host_debug
  zip -rq ../../$cacheDir/flutter-web-sdk-linux-x64.zip flutter_web_sdk
  cd ..
  cd ..
  bd_upload $cacheDir/flutter-web-sdk-linux-x64.zip flutter/framework/$tosDir/flutter-web-sdk-linux-x64.zip

  ./flutter/tools/gn --runtime-mode=release --no-lto
  ninja -C out/host_release -j $jcount
  checkResult

  # linux-x64.zip
  modeDir=linux-x64
  rm -rf $cacheDir/$modeDir
  mkdir $cacheDir/$modeDir
  cp out/host_release/gen/flutter/lib/snapshot/isolate_snapshot.bin $cacheDir/$modeDir/product_isolate_snapshot.bin
  cp out/host_release/gen/flutter/lib/snapshot/vm_isolate_snapshot.bin $cacheDir/$modeDir/product_vm_isolate_snapshot.bin
  zip -rjq $cacheDir/$modeDir/artifacts.zip out/host_debug/flutter_tester out/host_debug/gen/frontend_server.dart.snapshot \
  third_party/icu/flutter/icudtl.dat out/host_debug/gen/flutter/lib/snapshot/isolate_snapshot.bin \
  out/host_debug/gen/flutter/lib/snapshot/vm_isolate_snapshot.bin out/host_debug/gen/const_finder.dart.snapshot $cacheDir/$modeDir/product_isolate_snapshot.bin \
  $cacheDir/$modeDir/product_vm_isolate_snapshot.bin out/host_debug/gen_snapshot
  bd_upload $cacheDir/$modeDir/artifacts.zip flutter/framework/$tosDir/$modeDir/artifacts.zip
fi