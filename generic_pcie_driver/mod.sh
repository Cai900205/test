#!/bin/sh
#
# [$1]: STRING	"Insert|Remove" to insmod the driver or rmmod the driver.
#

# Update the lib.
cp ./lib/lib*.so /lib

DEVICE="generic_pcie"

device="fpga_pcie"

case $1 in
	"Insert"|"insert"|"INSERT"|"-i")
	#echo "insert"
	module=`lsmod | grep -c "$DEVICE"`
	if [ $module -eq 1 ]
	then
		echo "The driver for $DEVICE has been inserted, exit now."
		exit 0
	else
		# Insmod the driver.
		insmod ./build/${DEVICE}.ko
		if [ $? -ne 0 ]
		then
			echo "Insmod the driver failed."
			exit 0
		fi

		# Get the major device number and created the device node.
		major=`cat /proc/devices | grep "$DEVICE" | awk '{print $1}'`
		minor=0

		mknod /dev/$device c $major $minor
	fi	
	;;
	"Remove"|"remove"|"REMOVE"|"-r")
	#echo "remove"
	module=`lsmod | grep -c "$DEVICE"`
	if [ $module -eq 0 ]
	then
		echo "The driver for $DEVICE has been removed, exit now."
		exit 0
	else
		# Remove the driver and the device nodes.
		rmmod ./build/${DEVICE}.ko

		rm -rf /dev/$device
	fi
	;;
	*)
	echo "help"
	;;
esac
