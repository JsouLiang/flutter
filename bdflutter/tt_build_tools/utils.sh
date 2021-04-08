export BUILD_TOOLS_DIR=$(cd "$(dirname "$0")";pwd)

function bd_upload() {
	upload_log=${BUILD_TOOLS_DIR}/upload.log
	
	[ -e ${upload_log} ] && rm -rf ${upload_log}

	echo "开始上传${2}..."

	node ${BUILD_TOOLS_DIR}/upload/upload.js $1 $2 > ${upload_log}

	if [ $? -ne 0 ]; then
      /usr/local/bin/node ${BUILD_TOOLS_DIR}/upload/upload.js $1 $2 > ${upload_log}
	fi

	cat ${upload_log}
}