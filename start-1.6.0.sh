# 本文档用于编译Ditto，目录在/home/qnwang/Ditto
# 1. 删除远程服务器上的旧版本
ssh qnwang@udpcc.dfshan.net <<EOF
    cd /home/qnwang/Ditto/tengine-install/sbin
    ./tengine -s stop
    cd ../..
    rm -rf tengine-install xquic
    exit
EOF

# 2. 编译 Tongsuo
cd Tongsuo-8.3.2
# ./config --prefix=/home/qnwang/Ditto/xquic/third_party/babassl
# make
# make install

# 3. 编译 libxml
cd ../
# cd libxml2-2.12.3
# ./configure --prefix=/home/qnwang/Ditto/xquic/third_party/libxml2
# make
# make install

# 4. xquic路径配置 babassl+libxml2
cd ../
export SSL_TYPE_STR="babassl"
export SSL_PATH_STR="/home/qnwang/Ditto/xquic/third_party/babassl"
export SSL_INC_PATH_STR="/home/qnwang/Ditto/xquic/third_party/babassl/include"
export SSL_LIB_PATH_STR="/home/qnwang/Ditto/xquic/third_party/babassl/lib/libssl.a;/home/qnwang/Ditto/xquic/third_party/babassl/lib/libcrypto.a"
export XML_PATH="/home/qnwang/Ditto/xquic/third_party/libxml2"
export XML_LIB_PATH="/home/qnwang/Ditto/xquic/third_party/libxml2/lib/libxml2.la"
export XML_INC_PATH="/home/qnwang/Ditto/xquic/third_party/libxml2/include/libxml2"

# 5. 编译 xquic
cd xquic
rm -rf build
mkdir -p build
cd build
cmake -DGCOV=on -DCMAKE_BUILD_TYPE=Debug -DXQC_PRINT_SECRET=1 -DXQC_ENABLE_COPA=1 -DXQC_COMPAT_DUPLICATE=1 -DXQC_ENABLE_EVENT_LOG=1 -DXQC_ENABLE_RENO=1 -DXQC_SUPPORT_SENDMMSG_BUILD=1 -DXQC_ENABLE_TESTING=1 -DXQC_ENABLE_BBR2=1 -DXQC_DISABLE_RENO=0 -DSSL_TYPE=${SSL_TYPE_STR} -DSSL_PATH=${SSL_PATH_STR} -DSSL_INC_PATH=${SSL_INC_PATH_STR} -DSSL_LIB_PATH=${SSL_LIB_PATH_STR} ..
make

# 6. 编译 tengine
cd ../..
rm -rf tengine-install
cd tengine
make clean
./configure --prefix=/home/qnwang/Ditto/tengine-install --sbin-path=sbin/tengine --with-xquic-inc="../xquic/include" --with-xquic-lib="../xquic/build" --with-http_v2_module --without-http_rewrite_module --add-module=modules/ngx_http_xquic_module --with-openssl="../Tongsuo-8.3.2" --with-cc-opt="-I ../xquic/third_party/libxml2/include/libxml2" --with-ld-opt="-L ../xquic/third_party/libxml2/lib -lxml2 -lz -lm"
make
make install
cd ..

# 7. 将编译好的文件传输到远程服务器
rsync -r -va -e "ssh" tengine-install qnwang@udpcc.dfshan.net:/home/qnwang/Ditto
rsync -r -va -e "ssh" xquic qnwang@udpcc.dfshan.net:/home/qnwang/Ditto

# 8. 在远程服务器上启动tengine
ssh root@udpcc.dfshan.net <<EOF
    export PATH=/usr/bin:$PATH
    cd /usr/local/lib/
    rm -rf libxquic.so
    cp "/home/qnwang/Ditto/xquic/build/libxquic.so" /usr/local/lib/
EOF
ssh root@udpcc.dfshan.net <<EOF
    export PATH=/usr/bin:$PATH
    cd /home/qnwang/Ditto/tengine-install/sbin
    sudo chown root tengine
    sudo chmod u+s tengine
EOF
ssh qnwang@udpcc.dfshan.net <<EOF
    cd /home/qnwang/Ditto/tengine-install
    cd sbin
    ./tengine -c conf/nginx.conf
EOF
#sudo lsof -i:8443
# python3 client.py https://udpcc.dfshan.net:8443/10M