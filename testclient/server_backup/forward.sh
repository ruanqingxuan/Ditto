# 本sh文件用于重启后的启动tengine
sudo lsof -i:8443
sudo lsof -i:80
sudo pkill tengine
sudo pkill apache2
cd /home/qnwang/worknew/AR/tengine-install/sbin
sudo chown root tengine
sudo chmod u+s tengine
ldd tengine
./tengine -t
./tengine
cd /home/qnwang/worknew/AR/testclient
bash loss.sh
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/qnwang/worknew/AR/xquic/build
dd if=/dev/zero of=100M bs=1M count=100
sudo tc qdisc del dev eth0 root netem
sudo tc qdisc show dev eth0 