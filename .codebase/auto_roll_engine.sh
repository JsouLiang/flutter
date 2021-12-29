#!/bin/bash
if [ ! -d "./flutter" ];then
  git clone git@code.byted.org:tech_client/flutter.git
fi
# framework分支为bd_2.8
cd flutter
git checkout -b bd_2.8 origin/bd_2.8

#engine分支为bd_2.8_pre
cd ../
git checkout -b bd_2.8_pre origin/bd_2.8_pre

git log --pretty=format:"%h :%ae  %s  %ad" > a.text
cur_engine=`head -c 10 a.text`
old_engine=`head -c 10 ./flutter/bin/internal/ttengine.version`

echo ${cur_engine}
echo ${old_engine}

#查找出当前engineVersion所在行数
cur_line=`grep -n ${cur_engine} a.text | cut -d ":" -f 1`
old_line=`grep -n ${old_engine} a.text | cut -d ":" -f 1`
if [ $cur_line -lt $old_line ]
then
  #更换ttengine.version为最新
  complete_cur_engine=`git log -1 --pretty=format:"%H"`
  echo "${complete_cur_engine}"> ./flutter/bin/internal/ttengine.version
  echo "engine change commits:" >b.txt
  # mac版本
  #sed -n "${cur_line},$[old_line-1] p" a.text >>b.txt
  #linux版本
  sed -n "${cur_line},$((old_line - 1))p" a.text >>b.txt
  IFS=
  message="$(cat b.txt)"
  echo ${message}

  cd ./flutter
  git add ./bin/internal/ttengine.version
  git commit -m "feat: roll engine version to ${complete_cur_engine}" -m "${message}"
  git push https://ci_flutter:hs7y4aZwT3Ggi9uzx3-X@code.byted.org/tech_client/flutter.git bd_2.8
fi

