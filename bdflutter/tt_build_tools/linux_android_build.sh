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

              # x86和x64要带上libflutter.so
              if [ $platform = 'x64' -o $platform = 'x86' ]; then
                  zip -rjq $cacheDir/$modeDir/artifacts.zip $androidDir/flutter.jar $androidDir/lib.stripped/libflutter.so
              else
                  zip -rjq $cacheDir/$modeDir/artifacts.zip $androidDir/flutter.jar
              fi
              bd_upload $cacheDir/$modeDir/artifacts.zip flutter/framework/$tosDir/$modeDir/artifacts.zip
              bd_upload $androidDir/libflutter.so flutter/framework/$tosDir/$modeDir/libflutter_symtab.so
              # get so BuildID
              hashcode=`file $androidDir/libflutter.so | sed 's/.*BuildID\[[0-9a-zA-Z\/]*\]=\([0-9a-zA-Z]*\),.*/\1/g'`
              resultFile=$androidDir/$hashcode
              # get so build mode
              echo mode=$modeDir >> $resultFile
              # get git commit id
              echo cid=$gitCid >> $resultFile
              echo user=$gitUser >> $resultFile
              echo msg=$gitMessage >> $resultFile
              echo time=$gitDate >> $resultFile
              bd_upload $resultFile flutter/framework/buildid/$hashcode
              rm $resultFile
              #upload file for flutter.gradle dependencies
              mapPlatform $platform
              echo $platformResult
              if [ $liteMode != 'normal' ]; then
                bd_upload $androidDir/${platformResult}_${mode}_${liteMode}.jar flutter/framework/io/flutter/${platformResult}_${mode}_${liteMode}/1.0.0-${tosDir}/${platformResult}_${mode}_${liteMode}-1.0.0-${tosDir}.jar
                bd_upload $androidDir/${platformResult}_${mode}_${liteMode}.pom flutter/framework/io/flutter/${platformResult}_${mode}_${liteMode}/1.0.0-${tosDir}/${platformResult}_${mode}_${liteMode}-1.0.0-${tosDir}.pom
                if [ $platform = 'arm' ]; then
                    bd_upload $androidDir/flutter_embedding_${mode}_${liteMode}.jar flutter/framework/io/flutter/flutter_embedding_${mode}_${liteMode}/1.0.0-${tosDir}/flutter_embedding_${mode}_${liteMode}-1.0.0-${tosDir}.jar
                    bd_upload $androidDir/flutter_embedding_${mode}_${liteMode}.pom flutter/framework/io/flutter/flutter_embedding_${mode}_${liteMode}/1.0.0-${tosDir}/flutter_embedding_${mode}_${liteMode}-1.0.0-${tosDir}.pom
                fi
              elif [ $dynamic = 'dynamicart' ]; then
                bd_upload $androidDir/${platformResult}_dynamicart_${mode}.jar flutter/framework/io/flutter/${platformResult}_dynamicart_${mode}/1.0.0-${tosDir}/${platformResult}_dynamicart_${mode}-1.0.0-${tosDir}.jar
                bd_upload $androidDir/${platformResult}_dynamicart_${mode}.pom flutter/framework/io/flutter/${platformResult}_dynamicart_${mode}/1.0.0-${tosDir}/${platformResult}_dynamicart_${mode}-1.0.0-${tosDir}.pom
                if [ $platform = 'arm' ]; then
                    bd_upload $androidDir/flutter_embedding_dynamicart_${mode}.jar flutter/framework/io/flutter/flutter_embedding_dynamicart_${mode}/1.0.0-${tosDir}/flutter_embedding_dynamicart_${mode}-1.0.0-${tosDir}.jar
                    bd_upload $androidDir/flutter_embedding_dynamicart_${mode}.pom flutter/framework/io/flutter/flutter_embedding_dynamicart_${mode}/1.0.0-${tosDir}/flutter_embedding_dynamicart_${mode}-1.0.0-${tosDir}.pom
                fi
              else
                bd_upload $androidDir/${platformResult}_${mode}.jar flutter/framework/io/flutter/${platformResult}_${mode}/1.0.0-${tosDir}/${platformResult}_${mode}-1.0.0-${tosDir}.jar
                bd_upload $androidDir/${platformResult}_${mode}.pom flutter/framework/io/flutter/${platformResult}_${mode}/1.0.0-${tosDir}/${platformResult}_${mode}-1.0.0-${tosDir}.pom
                if [ $platform = 'arm' ]; then
                    bd_upload $androidDir/flutter_embedding_${mode}.jar flutter/framework/io/flutter/flutter_embedding_${mode}/1.0.0-${tosDir}/flutter_embedding_${mode}-1.0.0-${tosDir}.jar
                    bd_upload $androidDir/flutter_embedding_${mode}.pom flutter/framework/io/flutter/flutter_embedding_${mode}/1.0.0-${tosDir}/flutter_embedding_${mode}-1.0.0-${tosDir}.pom
                fi
              fi
          done
      done
  done
done