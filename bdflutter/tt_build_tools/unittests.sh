./flutter/tools/gn --runtime-mode=debug --unopt --coverage

ninja -C out/host_debug_unopt -j $JCOUNT

cd out/host_debug_unopt
# 清除历史构建覆盖率数据
echo 'clean history coverage info'
lcov -q -z -d ./
# 生成初始覆盖率数据
echo 'generate coverage_base.info'
[ -e coverage_base.info ] && rm -rf coverage_base.info
lcov -q -c -i -d ./ -o coverage_base.info 
cd -

# 运行单元测试
bash flutter/testing/run_tests.sh

cd out/host_debug_unopt

# 生成单测完成后覆盖率数据
echo 'generate coverage_tests.info'
[ -e coverage_tests.info ] && rm -rf coverage_tests.info
lcov -q -c -d ./ -o coverage_tests.info

# 合并覆盖率数据
echo 'generate coverage_total.info'
[ -e coverage_total.info ] && rm -rf coverage_total.info
lcov -q -a coverage_base.info -a coverage_tests.info -o coverage_total.info 

# 过滤文件
echo 'generate coverage_final.info'
[ -e coverage_final.info ] && rm -rf coverage_final.info
lcov -q -r coverage_total.info '*third_party*' '*fixture*' '*.l' '*.rl' '*Xcode.app*' '*buildtools*' -o coverage_final.info

# 生成网页
if [ ! $COMMIT_ID ]
then
    echo '没有获取commit信息'
    exit 1
fi
echo 'commit_id:' $COMMIT_ID
[ -d coverage_html ] && rm -rf coverage_html
genhtml -q coverage_final.info -t $COMMIT_ID -o coverage_html

# 生成压缩文件
[ -e coverage_res.zip ] && rm -rf coverage_res.zip
zip -rq coverage_res.zip coverage_final.info coverage_html

# 下载tos工具
[ -e upload.zip ] && rm -rf upload.zip
download_status=$(curl -o upload.zip "http://tosv.byted.org/obj/toutiao.ios.arch/flutter/upload.zip" -w %{http_code})
echo "上传脚本更新结果: ${download_status}"
unzip -oq upload.zip -d upload

# 上传
if [ -e coverage_res.zip ]; then
    node upload/upload.js coverage_res.zip flutter/unittests/coverage/$COMMIT_ID/cxx/coverage_res.zip > upload_log
    cat upload_log
    echo '单测覆盖率报告下载地址:http://tosv.byted.org/obj/toutiao.ios.arch/flutter/unittests/coverage/'$COMMIT_ID'/cxx/coverage_res.zip'
fi

cd -
