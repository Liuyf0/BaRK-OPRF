#!/bin/bash

# 定义端口 循环次数 网格大小 工人数目
start_port=2294
num_iterations=5
num_file_str=10000
counts=(2 4 6 8 10)

for count in "${counts[@]}"
do
    for ((i=2; i<=count; i++)); do
        cp ./cloudserver/1 ./cloudserver/$i.txt
    done
    # 循环执行命令
    for ((i = 0; i < num_iterations; i++)); do
        # 计算当前两个命令的端口
        port1=$((start_port + i))

        # 输出命令和端口信息
        echo "Executing command 1 with port $port1"
        echo "./Release/bOPRFmain.exe -r 1 -ip 127.0.0.1:$port1 $num_file_str $num_file_str $count"

        # 启动第一个命令，使用 port1
        ./Release/bOPRFmain.exe -r 1 -ip 127.0.0.1:$port1 $num_file_str $num_file_str $count &

        # 启动第二个命令，使用 port2
        ./Release/bOPRFmain.exe -r 0 -ip 127.0.0.1:$port1 $num_file_str $num_file_str $count &

        echo "wait..."
        echo ""
        wait
    done
    rm -rf ./cloudserver/*.txt
done