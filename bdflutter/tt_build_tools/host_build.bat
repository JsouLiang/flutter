@echo off
SetLocal EnableDelayedExpansion



for /f %%i in ('git log -1 "--pretty=%%H"')  do set gitCid=%%i
for /f %%i in ('git log -1 "--pretty=%%an"')  do set gitUser=%%i
git log -1 --pretty=%%B > %temp%\git.txt
for /f "Tokens=*" %%i in (%temp%\git.txt) do @set gitMessage=%%i
@REM for /f %%i in ('git log -1 "--pretty=%%B"')  do set gitMessage=%%i
for /f %%i in ('git log -1 "--pretty=%%ad"')  do set gitDate=%%i

echo "commit is %gitCid%"
echo "user is %gitUser%"
echo "gitMessage is %gitMessage%"
echo "gitDate is %gitDate%"

cd ..

set jcount=%1
IF [%jcount%] == [] (
    set jcount=4
)

set tosDir=%gitCid%

set cacheDir=out\tt_windows_host_cache
if exist %cacheDir% @RD /S /Q %cacheDir%
md %cacheDir%

set gn=.\flutter\tools\gn

set MODE=(debug, profile, release)
set CPU_ARCH=(x64, x86)


set build_tools_dir=.\flutter\bdflutter\tt_build_tools

for %%m in %MODE% do (
    for %%c in %CPU_ARCH% do (
        if %%c == x64 (
            if %%m == debug (
                call %gn% --runtime-mode %%m --no-lto --full-dart-sdk --windows-cpu=%%c --no-prebuilt-dart-sdk || goto :error
                call ninja.exe -C out\host_%%m -j %jcount% || goto :error
                
                REM flutter_patched_sdk.zip
                set zipPath=%cacheDir%\flutter_patched_sdk.zip
                if exist %zipPath% @RD /S /Q %zipPath%
                md %cacheDir%\flutter_patched_sdk
                call xcopy /S /e /v /i /Q out\host_debug\flutter_patched_sdk %cacheDir%\flutter_patched_sdk
                del /Q %cacheDir%\flutter_patched_sdk\platform_strong.dill.o
                cd %cacheDir%
                call zip -rq flutter_patched_sdk.zip flutter_patched_sdk
                cd ..
                cd ..
                call %build_tools_dir%\upload.bat %cd%\%cacheDir%\flutter_patched_sdk.zip flutter/framework/%tosDir%/flutter_patched_sdk.zip

                REM dart-sdk-windows-x64.zip
                cd out/host_debug
                set zipPath=%cacheDir%\dart-sdk-windows-x64.zip
                call zip -rq ..\..\%cacheDir%\dart-sdk-windows-x64.zip dart-sdk
                cd ..
                cd ..
                call %build_tools_dir%\upload.bat %cd%\%cacheDir%\dart-sdk-windows-x64.zip flutter_infra/flutter/%tosDir%/dart-sdk-windows-x64.zip


                REM font-subset.zip
                md %cacheDir%\windows-x64\font-subset
                call xcopy /Q out\host_debug\font-subset.exe %cacheDir%\windows-x64\font-subset\
                call xcopy /Q out\host_debug\gen\const_finder.dart.snapshot %cacheDir%\windows-x64\font-subset\
                call zip -rqj %cacheDir%\windows-x64\font-subset.zip %cacheDir%\windows-x64\font-subset\
                call %build_tools_dir%\upload.bat %cd%\%cacheDir%\windows-x64\font-subset.zip flutter/framework/%tosDir%/windows-x64/font-subset.zip

                REM flutter_web_sdk.zip
                cd out/host_debug
                call zip -rq ..\..\%cacheDir%\flutter-web-sdk-windows-x64.zip flutter_web_sdk
                cd ..
                cd ..
                call %build_tools_dir%\upload.bat %cd%\%cacheDir%\flutter-web-sdk-windows-x64.zip flutter/framework/%tosDir%/flutter-web-sdk-windows-x64.zip


                REM sky_engine
                if exist %cacheDir%\pkg @RD /S /Q %cacheDir%\pkg
                md %cacheDir%\pkg
                call xcopy /S /e /v /i /Q out\host_debug\gen\dart-pkg\sky_engine %cacheDir%\pkg\sky_engine
                cd %cacheDir%\pkg
                call zip -rq ..\..\..\%cacheDir%\sky_engine.zip sky_engine
                cd ..
                cd ..
                cd ..
                call %build_tools_dir%\upload.bat %cd%\%cacheDir%\sky_engine.zip flutter/framework/%tosDir%/sky_engine.zip

                REM windows-x64/artifacts.zip
                md %cacheDir%\windows-x64\artifacts
                call xcopy /Q out\host_debug\flutter_tester.exe %cacheDir%\windows-x64\artifacts\
                call xcopy /Q out\host_debug\frontend_server.dart.snapshot %cacheDir%\windows-x64\artifacts\
                call xcopy /Q out\host_debug\icudtl.dat %cacheDir%\windows-x64\artifacts\
                call xcopy /Q out\host_debug\gen\flutter\lib\snapshot\isolate_snapshot.bin %cacheDir%\windows-x64\artifacts\
                call xcopy /Q out\host_debug\gen\flutter\lib\snapshot\vm_isolate_snapshot.bin %cacheDir%\windows-x64\artifacts\
                call zip -rqj %cacheDir%\windows-x64\artifacts.zip %cacheDir%\windows-x64\artifacts
                call %build_tools_dir%\upload.bat %cd%\%cacheDir%\windows-x64\artifacts.zip flutter/framework/%tosDir%/windows-x64/artifacts.zip


                REM windows-x64/flutter-cpp-client-wrapper.zip
                cd out\host_debug
                call zip -rq ..\..\%cacheDir%\windows-x64\flutter-cpp-client-wrapper.zip cpp_client_wrapper
                cd ..
                cd ..
                call %build_tools_dir%\upload.bat %cd%\%cacheDir%\windows-x64\flutter-cpp-client-wrapper.zip flutter/framework/%tosDir%/windows-x64/flutter-cpp-client-wrapper.zip

                REM windows-x64/windows-x64-flutter.zip
                md %cacheDir%\windows-x64\windows-x64-flutter
                call xcopy /Q out\host_debug\flutter_windows.dll %cacheDir%\windows-x64\windows-x64-flutter\
                call xcopy /Q out\host_debug\flutter_windows.dll.pdb %cacheDir%\windows-x64\windows-x64-flutter\
                call xcopy /Q out\host_debug\flutter_windows.dll.exp %cacheDir%\windows-x64\windows-x64-flutter\
                call xcopy /Q out\host_debug\flutter_windows.dll.lib %cacheDir%\windows-x64\windows-x64-flutter\
                call xcopy /Q out\host_debug\flutter_export.h %cacheDir%\windows-x64\windows-x64-flutter\
                call xcopy /Q out\host_debug\flutter_messenger.h %cacheDir%\windows-x64\windows-x64-flutter\
                call xcopy /Q out\host_debug\flutter_plugin_registrar.h %cacheDir%\windows-x64\windows-x64-flutter\
                call xcopy /Q out\host_debug\flutter_texture_registrar.h %cacheDir%\windows-x64\windows-x64-flutter\
                call xcopy /Q out\host_debug\flutter_windows.h %cacheDir%\windows-x64\windows-x64-flutter\
                call zip -rqj %cacheDir%\windows-x64\windows-x64-flutter.zip %cacheDir%\windows-x64\windows-x64-flutter
                call %build_tools_dir%\upload.bat %cd%\%cacheDir%\windows-x64\windows-x64-flutter.zip flutter/framework/%tosDir%/windows-x64/windows-x64-flutter.zip


            ) else if %%m == release (
                call %gn% --runtime-mode %%m --no-lto --windows-cpu=%%c --no-prebuilt-dart-sdk || goto :error
                call ninja.exe -C out\host_%%m -j %jcount% || goto :error

                REM flutter_patched_sdk_product.zip
                set zipPath=%cacheDir%\flutter_patched_sdk_product.zip
                if exist %zipPath% @RD /S /Q %zipPath%
                md %cacheDir%\flutter_patched_sdk_product
                call xcopy /S /e /v /i /Q out\host_release\flutter_patched_sdk %cacheDir%\flutter_patched_sdk_product
                del /Q %cacheDir%\flutter_patched_sdk_product\platform_strong.dill.o
                cd %cacheDir%
                call zip -rq flutter_patched_sdk_product.zip flutter_patched_sdk_product
                cd ..
                cd ..
                call %build_tools_dir%\upload.bat %cd%\%cacheDir%\flutter_patched_sdk_product.zip flutter/framework/%tosDir%/flutter_patched_sdk_product.zip
                
                call %gn% --runtime-mode %%m --windows-cpu=%%c --no-prebuilt-dart-sdk || goto :error
            ) else (
                call %gn% --runtime-mode %%m --windows-cpu=%%c --no-prebuilt-dart-sdk || goto :error
            )
        ) else (
            call %gn% --runtime-mode %%m --windows-cpu=%%c --no-prebuilt-dart-sdk || goto :error
        )
        if %%c == x64 (
            REM windows-x64 release and profile
            if not %%m == debug (
                call ninja.exe -C out\host_%%m -j %jcount% || goto :error

                REM windows-x64-release[profile]/windows-x64-flutter.zip
                md %cacheDir%\windows-x64-%%m\windows-x64-flutter
                call xcopy /Q out\host_%%m\flutter_windows.dll %cacheDir%\windows-x64-%%m\windows-x64-flutter\
                call xcopy /Q out\host_%%m\flutter_windows.dll.pdb %cacheDir%\windows-x64-%%m\windows-x64-flutter\
                call xcopy /Q out\host_%%m\flutter_windows.dll.exp %cacheDir%\windows-x64-%%m\windows-x64-flutter\
                call xcopy /Q out\host_%%m\flutter_windows.dll.lib %cacheDir%\windows-x64-%%m\windows-x64-flutter\
                call xcopy /Q out\host_%%m\flutter_export.h %cacheDir%\windows-x64-%%m\windows-x64-flutter\
                call xcopy /Q out\host_%%m\flutter_messenger.h %cacheDir%\windows-x64-%%m\windows-x64-flutter\
                call xcopy /Q out\host_%%m\flutter_plugin_registrar.h %cacheDir%\windows-x64-%%m\windows-x64-flutter\
                call xcopy /Q out\host_%%m\flutter_texture_registrar.h %cacheDir%\windows-x64-%%m\windows-x64-flutter\
                call xcopy /Q out\host_%%m\flutter_windows.h %cacheDir%\windows-x64-%%m\windows-x64-flutter\
                call xcopy /Q out\host_%%m\gen_snapshot.exe %cacheDir%\windows-x64-%%m\windows-x64-flutter\
                call zip -rqjj %cacheDir%\windows-x64-%%m\windows-x64-flutter.zip %cacheDir%\windows-x64-%%m\windows-x64-flutter
                call %build_tools_dir%\upload.bat %cd%\%cacheDir%\windows-x64-%%m\windows-x64-flutter.zip flutter/framework/%tosDir%/windows-x64-%%m/windows-x64-flutter.zip
            )
        ) else (
            call ninja.exe -C out\host_%%m_%%c -j %jcount% || goto :error

            REM windows x86 arm64 release profile debug
            if %%m == debug (
                set windowsDir=windows-%%c
            ) else (
                set windowsDir=windows-%%c-%%m
            )
            set hostDir=host_%%m_%%c
            set windowsFlutterDir=windows-%%c-flutter
            md %cacheDir%\!windowsDir!\!windowsFlutterDir!
            call xcopy /Q out\!hostDir!\flutter_windows.dll %cacheDir%\!windowsDir!\!windowsFlutterDir!\
            call xcopy /Q out\!hostDir!\flutter_windows.dll.pdb %cacheDir%\!windowsDir!\!windowsFlutterDir!\
            call xcopy /Q out\!hostDir!\flutter_windows.dll.exp %cacheDir%\!windowsDir!\!windowsFlutterDir!\
            call xcopy /Q out\!hostDir!\flutter_windows.dll.lib %cacheDir%\!windowsDir!\!windowsFlutterDir!\
            call xcopy /Q out\!hostDir!\flutter_export.h %cacheDir%\!windowsDir!\!windowsFlutterDir!\
            call xcopy /Q out\!hostDir!\flutter_messenger.h %cacheDir%\!windowsDir!\!windowsFlutterDir!\
            call xcopy /Q out\!hostDir!\flutter_plugin_registrar.h %cacheDir%\!windowsDir!\!windowsFlutterDir!\
            call xcopy /Q out\!hostDir!\flutter_texture_registrar.h %cacheDir%\!windowsDir!\!windowsFlutterDir!\
            call xcopy /Q out\!hostDir!\flutter_windows.h %cacheDir%\!windowsDir!\!windowsFlutterDir!\
            if %%c == arm64 (
                call xcopy /Q out\!hostDir!\gen_snapshot.exe %cacheDir%\!windowsDir!\!windowsFlutterDir!\
            )
            call zip -rqjj %cacheDir%\!windowsDir!\!windowsFlutterDir!.zip %cacheDir%\!windowsDir!\!windowsFlutterDir!
            call %build_tools_dir%\upload.bat %cd%\%cacheDir%\!windowsDir!\!windowsFlutterDir!.zip flutter/framework/%tosDir%/!windowsDir!/!windowsFlutterDir!.zip
        )

    )
)



:error
if NOT !errorlevel! == 0 (
    echo "error is occur"
    exit /b !errorlevel!
) else (
    echo "[Done]"
)
