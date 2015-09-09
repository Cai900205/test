#! /bin/sh
#
# [$1]: INTEGER	Test times.
#

times=0
while [ $times -lt $1 ]
do
	verify=`./bin/Tcrypto --verify --crypto | grep -c "08 09 0a 0b 0c 0d 0e 0f"`
	if [ $verify -ne 1 ]
	then
		# Sleep for 10 seconds and try again.
		sleep 10
	    verify=`./bin/Tcrypto --verify --crypto | grep -c "08 09 0a 0b 0c 0d 0e 0f"`
		if [ $verify -ne 1 ]
		then
			# Sleep for 10 seconds and try again.
			sleep 10
	        verify=`./bin/Tcrypto --verify --crypto | grep -c "08 09 0a 0b 0c 0d 0e 0f"`
			if [ $verify -ne 1 ]
			then
				echo "[CRYPTOMEM-`date +'%Y-%m-%d %T'`]: Test the verify function for ${times}-nth times failed."
				exit 1
			fi
		fi
	fi
	
	times=`expr $times + 1`
done

echo "[CRYPTOMEM-`date +'%Y-%m-%d %T'`]: Test the cryptomem for $1 times success."
exit 0

