#!/bin/bash

source $(cd "$(dirname "$0")";pwd)/utils.sh

upload_dsym_to_slardar() {
	echo "Start upload dSYM to HMD server"
	STATUS=$(curl "http://symbolicate.byted.org/slardar_ios_upload" -F "file=@${1}" -F "aid=13" -H "Content-Type: multipart/form-data" -w %{http_code} -v)
	echo "HMD cn server response: ${STATUS}"
	STATUS=$(curl "http://symbolicateus.byted.org/slardar_ios_upload" -F "file=@${1}" -F "aid=1342" -H "Content-Type: multipart/form-data" -w %{http_code} -v)
	echo "HMD us server response: ${STATUS}"
}

dSYMInfoPlistPath=$(pwd)"/Info.plist"
cd ..
cd ..

jcount=$1

if [ ! $jcount ]
then
    jcount=4
fi

tosDir=$2
if [ ! $tosDir ]
then 
	tosDir=$(git rev-parse HEAD)
fi

if [ ! $tosDir ]
then 
	tosDir='default'
fi

liteModeArg=$3
liteModes=(${liteModeArg//,/ })
if [ ${#liteModes[@]} == 0 ];then
    liteModes=('normal')
fi
echo "IOS build modes: ${liteModes[@]}"

function checkResult() {
    if [ $? -ne 0 ]; then
        echo "Host debug compile failed !"
        exit 1
    fi
}

cd ..
for liteMode in ${liteModes[@]}; do
	for mode in 'debug' 'profile' 'release' 'release_dynamicart' 'profile_dynamicart'; do
		# lite only build for release mode
		if [ $mode == 'debug' ] || [ $mode == 'profile' ] || [[ $mode == *"profile_dynamicart"* ]]; then
		  if [ $liteMode != 'normal' ]; then
		    echo 'lite mode only build for release!'
		    continue
		  fi
		fi
		iOSArm64Dir=out/ios_${mode}
		iOSArmV7Dir=out/ios_${mode}_arm
		real_mode=${mode%_dynamicart}
		echo "iOS build mode = ${mode} liteMode = ${liteMode}"

    	# dynamicart与lite互斥
		if [ "$mode" == "release_dynamicart" -o "$mode" == "profile_dynamicart" ]; then
		   iOSArmV7Dir=out/ios_${real_mode}_arm_dynamicart
		fi
		# only build for release_dynamicart
		if [ "$liteMode" != "normal" ]; then
			if [ "$mode" != "release_dynamicart" ] && [ "$mode" != "release" ]; then
				echo 'lite mode only build for release_dynamicart & release!'
				continue
			fi
		fi
		iOSSimDir=out/ios_debug_sim
		cacheDir=out/tt_ios_${mode}
		echo "iOS build start mode = ${mode} liteMode = ${liteMode}"

    	modeSuffix=''
		if [ "$liteMode" != "normal" ]; then
        	iOSArm64Dir=${iOSArm64Dir}_${liteMode}
        	iOSArmV7Dir=${iOSArmV7Dir}_${liteMode}
        	iOSSimDir=${iOSSimDir}_${liteMode}
        	cacheDir=${cacheDir}_${liteMode}
        	modeSuffix=--${liteMode}
    	fi

		[ -d $cacheDir ] && rm -rf $cacheDir
		mkdir $cacheDir

#		./flutter/tools/gn --runtime-mode=$mode
#		ninja -C $hostDir -j $jcount

		# 编译各种架构引擎
        if [ "$mode" == "release_dynamicart" -o "$mode" == "profile_dynamicart" ]
        then
            ./flutter/tools/gn --ios --runtime-mode=${real_mode} --dynamicart $modeSuffix
            ninja -C $iOSArm64Dir -j $jcount
            checkResult

            ./flutter/tools/gn --ios --runtime-mode=${real_mode} --ios-cpu=arm --dynamicart $modeSuffix
            ninja -C $iOSArmV7Dir -j $jcount
            checkResult
        else
            ./flutter/tools/gn --ios --runtime-mode=$mode $modeSuffix $modeSuffix
            ninja -C $iOSArm64Dir -j $jcount
            checkResult

            ./flutter/tools/gn --ios --runtime-mode=$mode --ios-cpu=arm $modeSuffix
            ninja -C $iOSArmV7Dir -j $jcount
            checkResult
        fi

	    ./flutter/tools/gn --ios --runtime-mode=debug --simulator $modeSuffix
	    ninja -C $iOSSimDir -j $jcount
	    checkResult

		# 多种引擎架构合成一个
		lipo -create $iOSArm64Dir/Flutter.framework/Flutter $iOSArmV7Dir/Flutter.framework/Flutter $iOSSimDir/Flutter.framework/Flutter -output $cacheDir/Flutter

		# release模式引擎裁剪符号表
		if [ "$mode" == "release" -o "$mode" == "release_dynamicart" ]
		then
			echo "Generate dSYM"
			cd $cacheDir
			xcrun dsymutil -o Flutter.dSYM Flutter
			cp ${dSYMInfoPlistPath} Flutter.dSYM/Contents/Info.plist
			zip -rq Flutter.dSYM.zip Flutter.dSYM
			[ -e Flutter.dSYM ] && rm -rf Flutter.dSYM
			xcrun strip -x -S Flutter
			cd -
		fi

		cp -r $iOSArm64Dir/Flutter.framework $cacheDir/Flutter.framework
		mv $cacheDir/Flutter $cacheDir/Flutter.framework/Flutter

		cp $iOSArm64Dir/clang_x64/gen_snapshot $cacheDir/gen_snapshot_arm64
		cp $iOSArmV7Dir/clang_x64/gen_snapshot $cacheDir/gen_snapshot_armv7

		cp $iOSArm64Dir/Flutter.podspec $cacheDir/Flutter.podspec
		cp flutter/lib/snapshot/snapshot.dart $cacheDir/snapshot.dart

		cd $cacheDir

		cd Flutter.framework
		zip -rq Flutter.framework.zip Flutter Headers icudtl.dat Info.plist Modules
		cd ..
		mv Flutter.framework/Flutter.framework.zip Flutter.framework.zip
		[ -d Flutter.framework ] && rm -rf Flutter.framework
		if [ ! -e Flutter.framework.zip -o ! -e gen_snapshot_arm64 -o ! -e gen_snapshot_armv7 -o ! -e Flutter.podspec ]; then
		    echo "Compile failed !"
            exit 1
        fi
		zip -rq artifacts.zip Flutter.framework.zip gen_snapshot_arm64 gen_snapshot_armv7 Flutter.podspec snapshot.dart
		[ -e Flutter.framework.zip ] && rm -rf Flutter.framework.zip
		[ -e gen_snapshot_arm64 ] && rm -rf gen_snapshot_arm64
		[ -e gen_snapshot_armv7 ] && rm -rf gen_snapshot_armv7
		[ -e Flutter.podspec ] && rm -rf Flutter.podspec
		[ -e snapshot.dart ] && rm -rf snapshot.dart

		cd ..
		cd ..

		modeDir=ios
		if [ "$mode" == "profile" ]
		then
			modeDir=ios-profile
		elif [ "$mode" == "release" ]
		then
			modeDir=ios-release
		elif [ "$mode" == "release_dynamicart" -o "$mode" == "profile_dynamicart" ]
		then
			modeDir=ios-dynamicart-${real_mode}
		else
			modeDir=ios
		fi

		if [ "$liteMode" != "normal" ]; then
        	modeDir=${modeDir}-${liteMode}
    	fi

		bd_upload $cacheDir/artifacts.zip flutter/framework/$tosDir/$modeDir/artifacts.zip

		if [ "$mode" == "release" -o "$mode" == "release_dynamicart" ]
		then
			bd_upload $cacheDir/Flutter.dSYM.zip flutter/framework/$tosDir/$modeDir/Flutter.dSYM.zip
			upload_dsym_to_slardar "${cacheDir}/Flutter.dSYM.zip"
		fi
		echo "iOS build success mode = ${mode} liteMode = ${liteMode}"
	done
done
