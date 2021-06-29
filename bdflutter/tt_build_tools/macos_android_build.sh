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

releaseModeArg=$4
releaseModes=(${releaseModeArg//,/ })
if [ ${#releaseModes[@]} == 0 ];then
    releaseModes=('profile' 'release')
fi
echo "Android release modes: ${releaseModes[@]}"

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
  for mode in ${releaseModes[@]}; do
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
                if [ $mode = 'release' ]; then
                    ./flutter/tools/gn --android --runtime-mode=$mode --android-cpu=$platform --dynamicart $liteModeComdSuffix --only-gen-snapshot
                    androidDir=out/android_${mode}${platformPostFix}_dynamicart
                    modeDir=$modeDir-dynamicart
                else
                    continue
                fi
            else
                ./flutter/tools/gn --android --runtime-mode=$mode --android-cpu=$platform $liteModeComdSuffix --only-gen-snapshot
                androidDir=out/android_${mode}${platformPostFix}
            fi

              if [ "$liteMode" != 'normal' ]; then
                  androidDir=${androidDir}_${liteMode}
              fi
        androidDir=${androidDir}_gen_snapshot
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
                      zip -rjq $cacheDir/$modeDir/darwin-x64.zip $androidDir/clang_x86/gen_snapshot
                  else
                      zip -rjq $cacheDir/$modeDir/darwin-x64.zip $androidDir/clang_x64/gen_snapshot
                  fi
                  bd_upload $cacheDir/$modeDir/darwin-x64.zip flutter/framework/$tosDir/$modeDir/darwin-x64.zip
              fi
          done
      done
  done
done