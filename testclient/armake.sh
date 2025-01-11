cd /home/qnwang/worknew/AR/xquic
export SSL_TYPE_STR="babassl"
export SSL_PATH_STR="/home/qnwang/worknew/AR/xquic/third_party/babassl"
export SSL_INC_PATH_STR="/home/qnwang/worknew/AR/xquic/third_party/babassl/include"
export SSL_LIB_PATH_STR="/home/qnwang/worknew/AR/xquic/third_party/babassl/lib/libssl.a;/home/qnwang/worknew/AR/xquic/third_party/babassl/lib/libcrypto.a"
export XML_PATH="/home/qnwang/worknew/AR/xquic/third_party/libxml2"
export XML_LIB_PATH="/home/qnwang/worknew/AR/xquic/third_party/libxml2/lib/libxml2.la"
export XML_INC_PATH="/home/qnwang/worknew/AR/xquic/third_party/libxml2/include/libxml2"

rm -rf build
mkdir -p build
cd build
cmake -DGCOV=on -DCMAKE_BUILD_TYPE=Debug -DXQC_PRINT_SECRET=1 -DXQC_ENABLE_COPA=1 -DXQC_COMPAT_DUPLICATE=1 -DXQC_ENABLE_EVENT_LOG=1 -DXQC_ENABLE_RENO=1 -DXQC_SUPPORT_SENDMMSG_BUILD=1 -DXQC_ENABLE_TESTING=1 -DXQC_ENABLE_BBR2=1 -DXQC_DISABLE_RENO=0 -DSSL_TYPE=${SSL_TYPE_STR} -DSSL_PATH=${SSL_PATH_STR} -DSSL_INC_PATH=${SSL_INC_PATH_STR} -DSSL_LIB_PATH=${SSL_LIB_PATH_STR} ..
make
# cd ../scripts
# bash xquic_test.sh
# keyfile=server.key
# certfile=server.crt
# openssl req -newkey rsa:2048 -x509 -nodes -keyout "$keyfile" -new -out "$certfile" -subj /CN=test.xquic.com
# cd build
# killall test_server 2> /dev/null
# ./test_server -s 10240000 -l d -e -k 5 > /dev/null &
# ./test_client -l e -E -d 300 -k 5

cd /home/qnwang/worknew/AR
rm -rf tengine-install
cd tengine
make clean
./configure --prefix=/home/qnwang/worknew/AR/tengine-install --sbin-path=sbin/tengine --with-xquic-inc="../xquic/include" --with-xquic-lib="../xquic/build" --with-http_v2_module --without-http_rewrite_module --add-module=modules/ngx_http_xquic_module --with-openssl="../Tongsuo-8.3.2" --with-cc-opt="-I ../libxml2/include/libxml2" --with-ld-opt="-L ../libxml2/lib -lxml2 -lz -lm"
make
make install
cd ..
rsync -r -va -e "ssh" xquic qnwang@udpcc.dfshan.net:/home/qnwang/worknew/AR
rsync -r -va -e "ssh" tengine-install qnwang@udpcc.dfshan.net:/home/qnwang/worknew/AR


