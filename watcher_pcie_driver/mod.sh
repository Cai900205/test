#!/bin/sh
#
# [$1]: STRING	"Insert|Remove" to insmod the driver or rmmod the driver.
#

# Update the lib.
cp ./lib/lib*.so /lib

DEVICE="generic_pcie"
DEVICE0="generic_pcie_0"
DEVICE1="generic_pcie_1"
DEVICE2="generic_pcie_2"

device0="fpga_pcie0"
device1="fpga_pcie1"
device2="fpga_pcie2"

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
		major=`cat /proc/devices | grep "$DEVICE0" | awk '{print $1}'`
		minor=0
		mknod /dev/$device0 c $major $minor
		major=`cat /proc/devices | grep "$DEVICE1" | awk '{print $1}'`
		minor=0
		if [ "$major" != "" ] 
		then
			mknod /dev/$device1 c $major $minor
		fi
		major=`cat /proc/devices | grep "$DEVICE2" | awk '{print $1}'`
		if [ "$major" != "" ] 
		then
			mknod /dev/$device2 c $major $minor
		fi
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

		rm -rf /dev/$device0
		rm -rf /dev/$device1
		rm -rf /dev/$device2
	fi
	;;
	*)
	echo "help"
	;;
esac
