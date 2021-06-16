@echo off
REM node out\tt_windows_host_cache\upload\upload.js %1 %2 || exit /b %errorlevel%
echo "[upload]: %2"
curl -X PUT -H "x-tos-access: MJMETJODXZF7FZLFY3VT" -H "Content-Type: application/zip" --data-binary @%1  http://toutiao.ios.arch.tos-cn-north.byted.org/%2