#! /bin/bash
echo "================================================start at `date`==========================================" >> dfa.log
echo "================================================start at `date`==========================================" >> dfb.log
execfile="/home/root/shannon-package-fival-v1/shannon-package-fival-v1"
#fio fio_dfa_config >> dfa.log &
#fio fio_dfb_config >> dfb.log
#while [ 1 ];do
#sleep 5 
#ps | grep fio > /dev/null
#[ $? -ne 0 ] && break
#done

${execfile}/iocheck --target=/dev/dfa --file-size=100G --num-threads=16 --max-time=28800 --set-logbz=3584 --block-size=[3584,1075200] --ignore-save >> dfa.log
${execfile}/iocheck --target=/dev/dfb --file-size=100G --num-threads=16 --max-time=28800 --set-logbz=3584 --block-size=[3584,1075200] --ignore-save >> dfb.log
echo "=============================================================end at `date`========================" >> dfa.log
echo "=============================================================end at `date`========================" >> dfb.log

