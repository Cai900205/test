#! /bin/sh 
# [$1]: STRING Device Name
# [$2]: INTERGER OFFSET
# [$3]: INTERGER LENGTH
# [$5]: INTERGER  Test times

times=0;
first=1;

while [ $times -ne $5 ]
do
    times=`expr $times + 1`
    if [ "$times" -eq "$first" ]
    then
        mtd_debug read $1 $2 $3 $4
        mv $4 begin.file 
    else
        mtd_debug read $1 $2 $3 $4
        diff begin.file $4
        if [ $? -ne 0 ]
        then 
            echo "SPI FLASH READ failed"
            exit 1
        fi
        rm -rf $4
    fi
done
echo "[SPI-TEST-`date +'%Y-%m-%d %T'`]: $4-Bytes READ $5 times success."
rm -rf begin.file
