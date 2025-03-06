ssh qnwang@udpcc.dfshan.net <<EOF
    cd /home/qnwang/worknew/AR/tengine-install/sbin
    ./tengine -s stop
    cd ../..
    rm -rf tengine-install xquic-1.6.0
    exit
EOF
# 放在/home/qnwang下
# cd worknew/AR
# 编译 Tongsuo
cd Tongsuo-8.3.2
# ./config --prefix=/home/qnwang/worknew/AR/xquic-1.6.0/third_party/babassl
# make
# make install
#on
export SSL_TYPE_STR="babassl"
export SSL_PATH_STR="/home/qnwang/worknew/AR/on/xquic-1.6.0/third_party/babassl"
export SSL_INC_PATH_STR="/home/qnwang/worknew/AR/on/xquic-1.6.0/third_party/babassl/include"
export SSL_LIB_PATH_STR="/home/qnwang/worknew/AR/on/xquic-1.6.0/third_party/babassl/lib/libssl.a;/home/qnwang/worknew/AR/on/xquic-1.6.0/third_party/babassl/lib/libcrypto.a"
#AR
export SSL_TYPE_STR="babassl"
export SSL_PATH_STR="/home/qnwang/worknew/AR/xquic/third_party/babassl"
export SSL_INC_PATH_STR="/home/qnwang/worknew/AR/xquic/third_party/babassl/include"
export SSL_LIB_PATH_STR="/home/qnwang/worknew/AR/xquic/third_party/babassl/lib/libssl.a;/home/qnwang/worknew/AR/xquic/third_party/babassl/lib/libcrypto.a"

cd ../
# cd libxml2-2.12.3
# ./configure --prefix=/home/qnwang/worknew/AR/xquic-1.6.0/third_party/libxml2
# make
# make install
# export LD_TYPE_STR="libxml2"
# export LD_LIBRARY_PATH="/home/qnwang/worknew/AR/xquic-1.6.0/third_party/libxml2/lib"
# export PKG_CONFIG_PATH="/home/qnwang/worknew/AR/xquic-1.6.0/third_party/libxml2/lib/pkgconfig"
#on
export XML_PATH="/home/qnwang/worknew/AR/on/xquic-1.6.0/third_party/libxml2"
export XML_LIB_PATH="/home/qnwang/worknew/AR/on/xquic-1.6.0/third_party/libxml2/lib/libxml2.la"
export XML_INC_PATH="/home/qnwang/worknew/AR/on/xquic-1.6.0/third_party/libxml2/include/libxml2"
#AR
export XML_PATH="/home/qnwang/worknew/AR/xquic/third_party/libxml2"
export XML_LIB_PATH="/home/qnwang/worknew/AR/xquic/third_party/libxml2/lib/libxml2.la"
export XML_INC_PATH="/home/qnwang/worknew/AR/xquic/third_party/libxml2/include/libxml2"
# cd ../
cd xquic-1.6.0
cd xquic
rm -rf build
mkdir -p build
cd build
cmake -DGCOV=on -DCMAKE_BUILD_TYPE=Debug -DXQC_PRINT_SECRET=1 -DXQC_ENABLE_COPA=1 -DXQC_COMPAT_DUPLICATE=1 -DXQC_ENABLE_EVENT_LOG=1 -DXQC_ENABLE_RENO=1 -DXQC_SUPPORT_SENDMMSG_BUILD=1 -DXQC_ENABLE_TESTING=1 -DXQC_ENABLE_BBR2=1 -DXQC_DISABLE_RENO=0 -DSSL_TYPE=${SSL_TYPE_STR} -DSSL_PATH=${SSL_PATH_STR} -DSSL_INC_PATH=${SSL_INC_PATH_STR} -DSSL_LIB_PATH=${SSL_LIB_PATH_STR} ..

# cmake -DXQC_SUPPORT_SENDMMSG_BUILD=1 -DXQC_ENABLE_BBR2=1 -DXQC_DISABLE_RENO=1 -DXQC_ENABLE_COPA=1 -DSSL_TYPE=${SSL_TYPE_STR} -DSSL_PATH=${SSL_PATH_STR} -DSSL_INC_PATH=${SSL_INC_PATH_STR} -DSSL_LIB_PATH=${SSL_LIB_PATH_STR} ..

make
cd ../..
rm -rf tengine-install
cd tengine
make clean
#on
./configure --prefix=/home/qnwang/worknew/AR/on/tengine-install --sbin-path=sbin/tengine --with-xquic-inc="../xquic-1.6.0/include" --with-xquic-lib="../xquic-1.6.0/build" --with-http_v2_module --without-http_rewrite_module --add-module=modules/ngx_http_xquic_module --with-openssl="../Tongsuo-8.3.2"
#AR
./configure --prefix=/home/qnwang/worknew/AR/tengine-install --sbin-path=sbin/tengine --with-xquic-inc="../xquic/include" --with-xquic-lib="../xquic/build" --with-http_v2_module --without-http_rewrite_module --add-module=modules/ngx_http_xquic_module --with-openssl="../Tongsuo-8.3.2" --with-cc-opt="-I ../libxml2/include/libxml2" --with-ld-opt="-L ../libxml2/lib -lxml2 -lz -lm"
make
make install
cd ..

# cd tengine-install
# cd conf
# rm -rf nginx.conf
# cp ../../../nginx.conf
# cd ..
# cd ../..
# on
rsync -r -va -e "ssh" tengine-install qnwang@udpcc.dfshan.net:/home/qnwang/worknew/AR/on
rsync -r -va -e "ssh" xquic-1.6.0 qnwang@udpcc.dfshan.net:/home/qnwang/worknew/AR/on
#AR
rsync -r -va -e "ssh" tengine-install qnwang@udpcc.dfshan.net:/home/qnwang/worknew/AR
rsync -r -va -e "ssh" xquic qnwang@udpcc.dfshan.net:/home/qnwang/worknew/AR
ssh root@udpcc.dfshan.net <<EOF
    export PATH=/usr/bin:$PATH
    cd /usr/local/lib/
    rm -rf libxquic.so
    cp "/home/qnwang/worknew/AR/xquic-1.6.0/build/libxquic.so" /usr/local/lib/
EOF
ssh root@udpcc.dfshan.net <<EOF
    export PATH=/usr/bin:$PATH
    cd /home/qnwang/worknew/AR/tengine-install/sbin
    sudo chown root tengine
    sudo chmod u+s tengine
EOF
ssh qnwang@udpcc.dfshan.net <<EOF
    cd /home/qnwang/worknew/AR/tengine-install
    dd if=/dev/zero of=test1 bs=1M count=1
    cd sbin
    ./tengine -c conf/nginx.conf
EOF
#sudo lsof -i:8443
