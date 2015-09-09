#! /bin/bash
echo "================================================start at `date`==========================================" >> dfa.log
execfile="./test-tool"
cd $execfile
./install 
./ztool --dev=/dev/shannon_cdev mpt -u -c 2400G
./ztool --dev=/dev/shannon_cdev1 mpt -u -c 2400G
rmmod shannon_cdev
sleep 5

insmod shannon.ko
./shannon-format -fy -s 2400G /dev/scta
./shannon-format -fy -s 2400G /dev/sctb
rmmod shannon
sleep 5

./install
./ztool --dev=/dev/shannon_cdev super-erase 0 4
./ztool --dev=/dev/shannon_cdev1  mpt -fG -c 2400G
rmmod shannon_cdev
sleep 5

insmod shannon.ko
./fio fio_dfa_config >> dfa.log 
while [ 1 ];do
sleep 5 
ps | grep fio > /dev/null
[ $? -ne 0 ] && break
done
./iocheck --target=/dev/dfa --file-size=auto-C --num-threads=16 --max-time=28800 --set-logbz=3584 --block-size=[3584,1075200] --ignore-save >> dfa.log
echo "=============================================================end at `date`========================" >> dfa.log
./shanon-format -fcy -s 2400G /dev/scta
rmmod shannon 
sleep 5

./install
./ztool --dev=/dev/shannon_cdev1 super-erase 0 4
./ztool --dev=/dev/shannon_cdev  mpt -fG -c 2400G
rmmod shannon_cdev
insmod shannon.ko
echo "================================================start at `date`==========================================" >> dfb.log
./fio fio_dfb_config >> dfb.log
while [ 1 ];do
sleep 5 
ps | grep fio > /dev/null
[ $? -ne 0 ] && break
done
./iocheck --target=/dev/dfb --file-size=auto-C --num-threads=16 --max-time=28800 --set-logbz=3584 --block-size=[3584,1075200] --ignore-save >> dfb.log
echo "=============================================================end at `date`========================" >> dfb.log
./shanon-format -fcy -s 2400G /dev/sctb
rmmod shannon 

