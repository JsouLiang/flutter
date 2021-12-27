#!/usr/bin/env bash
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
    dynamics=('normal')
else
    platforms=('arm' 'x64' 'x86' 'arm64')
    dynamics=('normal')
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

platformResult="unknown"
mapPlatform() {
    if [ $1 == "arm" ]; then
       platformResult="armeabi_v7a"
    elif [ $1 == "arm64" ];then
       platformResult="arm64_v8a"
    elif [ $1 == "x64" ];then
       platformResult="x86_64"
    elif [ $1 == "x86" ];then
       platformResult="x86"
    else
       platformResult="unknown"
    fi
}

cd ..
cd ..

cacheDir=out/tt_android_cache
rm -rf $cacheDir
mkdir $cacheDir

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

for liteMode in ${liteModes[@]}; do
  if [ "$liteMode" != "normal" ]; then
     echo 'Warning: dynamicart dont compile lite mode for android'
     coutinue
  fi
  liteModeComdSuffix=''
  if [ ${liteMode} != 'normal' ]; then
      liteModeComdSuffix=--${liteMode}
  fi
  if [ $liteMode == 'lites' ];then
     echo 'lites is lite & share skia mode, now only for ios release !'
     continue
  fi
  for mode in 'debug' 'profile' 'release'; do
      for platform in ${platforms[@]}; do
          # x64和x86只打debug
          if [ $mode != 'debug' ]; then
              if [ $platform = 'x86' ]; then
                  continue
              fi
          fi
          if [ $mode != 'release' -a $liteMode != 'normal' ]; then
            echo 'lite mode only build for release!'
            continue
          fi
          for dynamic in ${dynamics[@]}; do
              modeDir=android-$platform
              # lite 不支持 dynamic
              if [ $liteMode != 'normal' ]; then
                  if [ $dynamic != 'normal' ]; then
                      echo 'lite can not support for dynamic!'
                      continue
                  fi
                  # lite 模式只支持 release 模式
                  if [ $mode == 'debug' ] || [ $mode == 'profile' ]; then
                      echo 'lite mode only build for release!'
                      continue
                  fi
                  # lite 模式不支持 x64 和 x86 模式
                  if [ $platform = 'x64' -o $platform = 'x86' ]; then
                      echo 'lite can not support for x86 and x64!'
                      continue
                  fi
              fi
              # arm不带后缀
              if [ $platform = 'arm' ]; then
                  platformPostFix=''
              else
                  platformPostFix=_${platform}
              fi

            # dynamicart只打release
            if [ $dynamic = 'dynamicart' ]; then
                if [ $mode = 'release' -o $mode = 'profile' ]; then
                    ./flutter/tools/gn --android --runtime-mode=$mode --android-cpu=$platform --dynamicart $liteModeComdSuffix
                    androidDir=out/android_${mode}${platformPostFix}_dynamicart
                    modeDir=$modeDir-dynamicart
                else
                    continue
                fi
            else
                ./flutter/tools/gn --android --runtime-mode=$mode --android-cpu=$platform $liteModeComdSuffix
                androidDir=out/android_${mode}${platformPostFix}
            fi

              if [ "$liteMode" != 'normal' ]; then
                  androidDir=${androidDir}_${liteMode}
              fi

			  ninja -C $androidDir -j $jcount
              checkResult

              if [ $mode != 'debug' ]; then
                  modeDir=$modeDir-$mode
              fi

              if [ "$liteMode" != 'normal' ]; then
                  modeDir=${modeDir}-${liteMode}
              fi

              rm -f $cacheDir/$modeDir
              mkdir $cacheDir/$modeDir

              # 非debug还要带上gen_snapshot
              if [ $mode != 'debug' ]; then
                  if [ -f "$androidDir/clang_x86/gen_snapshot" ];then
                      zip -rjq $cacheDir/$modeDir/linux-x64.zip $androidDir/clang_x86/gen_snapshot
                  else
                      zip -rjq $cacheDir/$modeDir/linux-x64.zip $androidDir/clang_x64/gen_snapshot
                  fi
                  bd_upload $cacheDir/$modeDir/linux-x64.zip flutter/framework/$tosDir/$modeDir/linux-x64.zip
              fi
          done
      done
  done
done

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
