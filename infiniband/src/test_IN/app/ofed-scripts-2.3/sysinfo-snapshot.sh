#!/bin/bash
#
# Copyright (c) 2012 Mellanox Technologies. All rights reserved.
#
# This Software is licensed under one of the following licenses:
#
# 1) under the terms of the "Common Public License 1.0" a copy of which is
#    available from the Open Source Initiative, see
#    http://www.opensource.org/licenses/cpl.php.
#
# 2) under the terms of the "The BSD License" a copy of which is
#    available from the Open Source Initiative, see
#    http://www.opensource.org/licenses/bsd-license.php.
#
# 3) under the terms of the "GNU General Public License (GPL) Version 2" a
#    copy of which is available from the Open Source Initiative, see
#    http://www.opensource.org/licenses/gpl-license.php.
#
# Licensee has the right to choose one of the above licenses.
#
# Redistributions of source code must retain the above copyright
# notice and one of the license notices.
#
# Redistributions in binary form must reproduce both the above copyright
# notice, one of the license notices in the documentation
# and/or other materials provided with the distribution.
# Capture system information on the local machine and the its IB fabric.
#
# Written by: Yair Ifergan <yairi@mellanox.com>,
# Maintained by: Ryanh Heath <ryanh@mellanox.com>




# default values
VERSION=2.0
PATH=/sbin:/usr/sbin:$PATH
HOST=$(hostname)
XDATE=$(date +%Y%m%d-%H%M)
TMPDIR=${TMPDIR:-"/tmp"}
OUT_DIR=$TMPDIR/sysinfo-snapshot-$HOST-$XDATE

NETDEVICES=$(LIST="" ; set -- `ls /sys/class/net`; while [ $# -ne 0 ];do [[ $1 == lo ]] && shift && continue; LIST="$LIST $1" ;shift;done ; echo $LIST)
export LANG=C

declare -a commands
declare -a xfiles xfile procfiles
declare -a xfiles

#------------------------------------------------------------------------------------------------------------------
function usage {
    echo "sysinfo-snapshot version: $VERSION usage:
	  The sysinfo-snapshot command gathers system information and place it into a tar file.
	  It is required to run this script as super user (root).
	  -h|--help - print this help
	  -d|--dir - set destination directory (default is $TMPDIR)"
    exit 0
}


if [[ -f /usr/bin/whoami ]] ; then
	if [[ `/usr/bin/whoami` != "root" ]] ; then
		echo "Runing as a none root user"
		echo "Please switch to root user (super user) and run again."
		exit 1
	fi
fi

while [[ ! -z "$1" ]]; do
	# echo "$1"
      case "$1" in
	-h | --help)
	    	usage
		shift
	;;
        -d | --dir)
            	OUT_DIR=$2/sysinfo_snapshot-$HOST-$XDATE
		shift 2
        ;;
	*)
		echo "error: unknown option $1"
	    	usage
	;;
	esac
done

OUT_FILE=$OUT_DIR/sysinfo-snapshot-${VERSION}-$HOST-$XDATE.html
if [ ! -d "$OUT_DIR" ]; then
    mkdir -p "$OUT_DIR"
fi

if [ ! -d "$OUT_DIR/stat" ]; then
    mkdir -p "$OUT_DIR/stat"
fi
echo "Destination directory is $OUT_DIR"
#------------------------------------------------------------------------------------------------------------------
#checks master SM is alive by sampling its activity count:
function sm-status {

	SmActivity_1=0
	NoSM=0

	for ((lo=0;lo<=3;lo++)) ; do
		sleep 3
		SmActivity=`sminfo |awk '{ print $10 }'`
		echo "SM activity on `date +%T` is $SmActivity"
		if [[ $SmActivity == $SmActivity_1 ]] ; then
			NoSM=1
		else
			NoSM=0
		fi
		SmActivity_1=$SmActivity
	done

	if [ $NoSM = 0 ] ; then
		echo "Master SM activity is progressing. SM is alive."
	else
		echo "ALERT: Master SM activity has not make any progress. CHECK master SM!"
	fi
}


#------------------------------------------------------------------------------------------------------------------
function zz_proc_net_bonding_files()
{

	find /proc/net/bonding/ |xargs grep ^

}


#------------------------------------------------------------------------------------------------------------------
function zz_sys_class_net_files()
{

	find /sys/class/net/ |xargs grep ^

}


#------------------------------------------------------------------------------------------------------------------
function Multicast_Information {

	echo "MLIDs list: "
	/usr/sbin/saquery -g
	echo ""
	echo "MLIDs members for each multicast group:"
	MLIDS=(`/usr/sbin/saquery -g |grep Mlid | sed 's/\./ /g'|awk '{print $2}'`)
	MLIDC=${#MLIDS[*]}

	for ((i = 0; i< $MLIDC ; i++)); do
	        echo "Members of MLID ${MLIDS[$i]} group:"
	        saquery -m ${MLIDS[$i]}
	        echo "============================================================"
	done
}


#------------------------------------------------------------------------------------------------------------------
function ib_switches_FW_scan() {

	lid=-1
	default_shaldag_fw="07.02.00"
	default_anafa_fw="01.00.05"

#	usage() {
#		echo    "usage : $0 [OPTIONS]"
#		echo    "Options"
#		echo    "[-u uniq_lid]		- Scan only uniq_lid"
#		echo    "[-f fw_version]		- Use user defined fw version"
#		echo    "[-t]			- Print output as a text (without colours)"
#		echo    "[-p]			- Print alarm entries only"
#		echo    "[-h]			- Show this help"
#		exit ;
#	}

	aprint_err_pc() {
	awk '
		function blue(s) {
			if (mono)
				printf s
			else
				printf "\033[1;034m" s "\033[0;39m"
		}
		function red(s) {
			if (mono)
				printf s
	  		else
				printf "\033[1;031m" s "\033[0;39m"
		}
		function green(s) {
			if (mono)
				printf s
		   else
				printf "\033[1;032m" s "\033[0;39m"
		}
		function print_title() {
			if (!(cnt_titles % 15))
				blue(title "\n")
				cnt_titles++
		}

		BEGIN {
			title = ("hw_dev_rev\thw_dev_id\tfw_version\tfw_build_id\tfw_date\t\tfw_psid")
			i_shaldag_alarm = 0
			fw_good = 0
			cnt_titles = 0
			mono = "'$mono'"
			supress_normal ="'$delp'"
			red("Scan Fabric\n")
			default_shaldag_fw="'$default_shaldag_fw'"
			default_anafa_fw="'$default_anafa_fw'"
			red("Default fw_versions are " default_shaldag_fw " for Shaldag and " default_anafa_fw " for Anafa\n")
			tb1="-----------------------------------------------------------------------------------------------"
     		blue(tb1 "\n")
		};

		/Hca/	{
			red($0 "\n")
			exit
		}
		/^Switch/ {
			i_shaldag++
			ind_shaldag = sprintf("%d ",i_shaldag)
			SWITCH = $0;next
		}

		{
		#	sub (/[\.\.\.]+/," ",$0)
		}
		/hw_dev_rev/ ||	/hw_dev_id/ || /fw_build_id/ {
			data[n++] = $NF "\t\t"
			next
		}
		/fw_version/ {
			if (( $NF == default_shaldag_fw )|| ( $NF == default_anafa_fw )) {
				fw_good = 1
			}
			data[n++] = $NF "\t"
			next
		}
		/fw_date/ || /fw_psid/ {
			data[n++] = $NF "\t"
			next
		}
		/sw_version/ {
			for (i = 0; i < n; i++)
				if (i in data) {
					table = (table data[i] )
				}
			if (fw_good == 1) {
				if (!supress_normal) {
					print_title()
					red(ind_shaldag)
					green(SWITCH "\n")
					green(table "\n")
					blue(tb1 "\n")
				}
			}
			else {
				print_title()
				red(ind_shaldag)
				red("--> ALERT "SWITCH " ALERT <--\n");
				red(table "\n")
				i_shaldag_alarm++
				blue(tb1 "\n")
			}
			fw_good = 0
			delete data
			table = ""
			n = 0
		}
		END {
			blue(title "\n")
			red("Default fw_versions are " default_shaldag_fw " for Shaldag and " default_anafa_fw " for Anafa\n")
			red("Total : CHIPs scanned : " i_shaldag ". Problems found : " i_shaldag_alarm "\n" )
		}';
	}

	get_topology_send_mad() {

	awk '
		#$1~/Switch/ && $2 == 24 {
		$1~/Switch/ && ($2 == 36 || $2 == 24) {
			lid = $(NF-2)
			sub (/#/,"\t", $0)
			print "echo " $0 "; vendstat -N", lid
			next
		}';
	}

	scan_all() {
		ibnetdiscover | get_topology_send_mad |sh |aprint_err_pc ;
		exit;
	}

	scan_one() {
		lid_l=$1
		echo START
		#madstat N $lid_l | \
		smpquery nodeinfo $lid_l | \
		awk -F "." '
		/NodeType/	{
			node_type = $NF
		}
		/LocalPort/	{
			localport = $NF
		}
		/NumPorts/	{
			nports    = $NF
		}
		/node_desc/	{
			node_desc = $NF
		}
		/Guid/	{
			node_guid = $NF
		}

		END	{
			if (node_type == "Channel Adapter") {
				printf("echo Could Not Read Hca firmware.\n")
				exit
			}
	   	printf("echo Switch nports %d localport %d %s 0x%s\n",nports ,localport, node_desc, node_guid)
 			print "vendstat N", '$lid_l'
		}' | sh | aprint_err_pc;
		exit;
	}

#--------- controlling logic for scan_one function ----------
	mono=1

	while getopts u:f:pht opt
		do
		case "$opt" in
	   	u) lid="$OPTARG";;
			f) defaultfw="$OPTARG";;
			t) mono=1;;
			p) delp=1;;
			h) usage;;
			\?) usage;;
  			esac
		done

	if [[ $lid -eq -1 ]];	then
		scan_all
	fi
	scan_one $lid
}

#------------------------------------------------------------------------------------------------------------------
function sm_version {

	echo "OpenSM installed packages: "
	rpm -qa |grep opensm

}


#------------------------------------------------------------------------------------------------------------------
function sm_master_is {

	MasterLID=(`/usr/sbin/sminfo |awk '{print $4}' `)
	echo "IB fabric SM master is: (`/usr/sbin/smpquery nodedesc $MasterLID`) "
	echo "All SMs in the fabric: "
	SMS=(`/usr/sbin/saquery -s |grep base_lid |head -1| sed 's/\./ /g'|awk '{print $2}'`)
	SMC=${#SMS[*]}

	for ((i = 0; i< $SMC ; i++)); do
	        echo ""
		echo ${SMS[$i]}
	         /usr/sbin/smpquery nodedesc ${SMS[$i]}
	         /usr/sbin/sminfo ${SMS[$i]}
	        echo ""
	done

}


#------------------------------------------------------------------------------------------------------------------
function ib-find-bad-ports {

	IBPATH=${IBPATH:-/usr/sbin}
	LIST=0
	SPEED=1
	WIDTH=1
	RESET=0
	echo ""

	abort_function() {
		if [[ "XXX$*" != "XXX" ]] ; then
			echo "$*"
		fi
		exit 1
	}

	trap 'abort_function "CTRL-C hit. Aborting."' 2

	count_1x=0
	checked_ports=0
	count_deg=0

	FILE="$TMPDIR/temp.$$"
	TEMPFILE="$TMPDIR/tempportinfo.$$"

	echo -en "Looking For Degraded Width (1X) Links .......\t"
	echo "done "
	echo -en "Looking For Degraded Speed Links ............\t"

	$IBPATH/ibnetdiscover -p | grep \( | grep -e "^SW" > $FILE

	exec < $FILE
	while read LINE
		do

		checked_ports=$((checked_ports+1))

		PORT="`echo $LINE |awk '{print $(3)}'`"
		GUID="`echo $LINE |awk '{print $(4)}'`"

		$IBPATH/ibportstate -G $GUID $PORT > $TEMPFILE

		ACTIVE_WIDTH="`cat $TEMPFILE | grep LinkWidthActive | head -1 | sed 's/.\.\./ /g' | awk '{print $(NF)}'`"
		ACTIVE_SPEED="`cat $TEMPFILE | grep LinkSpeedActive | head -1 | sed 's/.\.\./ /g' | awk '{print $2}'`"
		ENABLE_SPEED="`cat $TEMPFILE | grep LinkSpeedEnabled |head -1| sed 's/\.\./ /g' | awk '{print $(NF-1)}'`"

		if [ "$ACTIVE_WIDTH" == "1X" ] ; then
			count_1x=$((count_1x + 1))
			echo "GUID:$GUID PORT:$PORT run in 1X width"
		fi

		if [ "$ACTIVE_SPEED" != "$ENABLE_SPEED" ] ; then

			PEER_ENABLE_SPEED="`cat $TEMPFILE  | grep LinkSpeedEnabled |tail -1| sed 's/\.\./ /g' | awk '{print $(NF-1)}'`"

			if [ "$ACTIVE_SPEED" != "$PEER_ENABLE_SPEED" ] ; then

				count_deg=$((count_deg+1))
				echo "GUID:$GUID PORT:$PORT run in degraded speed"
				#ibportstate -G $GUID $PORT reset >/dev/null 2>&1
	        	#ibportstate -G $GUID $PORT enable >/dev/null 2>&1
			fi
		fi
	done

	CHECKED=$checked_ports
	rm -f $FILE $TEMPFILE

	echo -e "done "
	echo ""
	echo ""
	echo "## Summary: $CHECKED ports checked"
	echo "##	  $count_1x ports with 1x width found "
	echo "##        $count_deg ports with degraded speed found "
}

function ib-find-disabled-ports {
IBPATH=${IBPATH:-/usr/sbin}


checked_ports=0
count_disabled=0

FILE="$TMPDIR/temp.$$"

$IBPATH/ibnetdiscover -p | grep -v \( | grep -e "^SW" > $FILE

exec < $FILE
while read LINE
do

PORT="`echo $LINE |awk '{print $(3)}'`"
GUID="`echo $LINE |awk '{print $(4)}'`"

checked_ports=$((checked_ports+1))
LINK_STATE="`$IBPATH/ibportstate -G $GUID $PORT | grep PhysLinkState | head -1 | sed 's/.\.\.\./ /g' | awk '{print $NF}'`"

if [ "$LINK_STATE" == "Disabled" ] ; then
	$IBPATH/ibswitches | grep $GUID | grep -q sRB-20210G-1UP
	if [ $? == 0 -a $PORT == 24 ] ; then
		Is_10G=1
	else
		count_disabled=$((count_disabled + 1))
		echo "GUID: $GUID PORT: $PORT is disabled"
	fi
fi

done

rm $TMPDIR/temp.$$

echo ""
echo "## Summary: $checked_ports ports checked, $count_disabled disabled ports found"
}

function ib-mc-info-show {
nodes=$TMPDIR/MCnodes.$$
groups=$TMPDIR/MCgroups.$$
nodeLookup=false
groupLookup=false
MAX_GROUPS=64
version=1.2

function mgid2ip()
{
	local ip=`echo $1 | awk '
	{
		mgid=$1
		n=split(mgid, a, ":")
			if (a[2] == "401b") {
			upper=strtonum("0x" a[n-1])
			lower=strtonum("0x" a[n])
			addr=lshift(upper,16)+lower
			addr=or(addr,0xe0000000)
			a1=and(addr,0xff)
			addr=rshift(addr,8)
			a2=and(addr,0xff)
			addr=rshift(addr,8)
			a3=and(addr,0xff)
			addr=rshift(addr,8)
			a4=and(addr,0xff)
			printf("%u.%u.%u.%u", a4, a3, a2, a1)
		}
		else {
			printf ("IPv6")
		}
	}'`
	echo -en $ip
}
		node=$OPTARG
		nodeLookup=true
		group=$OPTARG
		groupLookup=true

saquery -m | while read line; do
	k=${line%%.*}
	v=${line##*.}
	if [ "$k" == "Mlid" ]; then
		mlid=$v
	elif [ "$k" == "MGID" ]; then
		ip=`mgid2ip $v`
	elif [ "$k" == "NodeDescription" ]; then
		if $groupLookup; then
			echo $mlid $ip $v >> $groups
		fi
		# Ignore switches and routes
		if [[ "$v" =~ "^ISR[29]|^[42]036|^IB-to-TCP|^sRB-20210G" ]]; then
			continue
		fi
		if $nodeLookup; then
			echo $v >> $nodes
		fi
	fi
done

echo  ----------------------------------
echo  -- Number of MC groups per node --
echo  ----------------------------------
if $nodeLookup ; then
		node=sum
		# Summary how many gruops for each node
		echo "Node Name	MC Groups #"
		sort $nodes | uniq -c | while read line; do
			gcount=`echo $line | cut -d " " -f 1`
			name=`echo $line | cut -d " " -f 2-`
			echo -en "$name	--->  $gcount"
			if [ $gcount -gt $MAX_GROUPS ]; then
				echo "	-- PERFORMANCE DROP WARNING --"
			fi
			echo
		done
fi

echo -------------------------------------
echo -- Number of MC members per groups --
echo -------------------------------------

if $groupLookup ; then

		group=sum
		#summary how many members for each MC group
		awk '{print $1, $2}' $groups | sort -k1 -n | uniq -c | awk '{printf("%s %s (%s)\n", $2, ($3=="IPv6"?"":$3), $1)}'
fi

#rm -f $nodes $groups
}

function ib-topology-viewer {

swVerbose=false
caVerbose=false
internal=false
discover=true

netfile="$TMPDIR/net"
swfile="$TMPDIR/sw"
swguids="$TMPDIR/swguids"
tempfile1="$TMPDIR/t1"
tempfile2="$TMPDIR/t2"

if [ ! -f $topofile ] ; then
	echo "$topofile doesnt exists!"
	usage
fi

if $internal; then
	if ! $discover; then
		cp $topofile $netfile
	else
		eval ibnetdiscover -p > $netfile
	fi
else
	if ! $discover; then
	 	cat $topofile |grep -v -i sfb > $netfile
	else
		eval ibnetdiscover -p |grep -v -i sfb > $netfile
	fi
fi

GUIDS=`cat $netfile | grep -e ^SW | awk '{print $4}' | uniq`


if [ "$GUIDS" == "" ] ; then
	echo "No Switch Found"
	exit
fi

for guid in $GUIDS ; do
	string="$guid..x"
	desc=`cat $netfile| grep -e ^SW | grep $string  | awk -F\' '{print $2}' | uniq`
	echo $desc==$guid >>$tempfile1
done

sort $tempfile1 -o $swfile
echo "-----------------------------------"
echo "-  Printing topollogy connection  -"
echo "-----------------------------------"

for guid in `awk -F== '{print $2}' $swfile`; do
	swDesc=`grep $guid $swfile | awk -F== '{print $1}'`
	ca=`awk -vg=$guid '{if ($1 ~ "SW" && $4 ~ g && $8 ~ "CA") print $0}' $netfile >$tempfile1`
	caNumber=`cat $tempfile1 | wc -l`
	sw=`awk -vg=$guid '{if ($1 ~ "SW" && $4 ~ g && $8 ~ "SW") print $0}' $netfile >$tempfile2`
	swNumber=`cat $tempfile2 | wc -l`
	notConnected=`awk -vg=$guid '{if ($1 ~ "SW" && $4 ~ g && $7 != "-") print $0}' $netfile |wc -l`
	printf "%-82s\t" "$swDesc($guid)"
	printf "$caNumber"
	printf " HCA ports and "
	printf "$swNumber"
	printf " switch ports.\n"

	if  [ ${swNumber} > 0 ]; then
		if $swVerbose ; then
			cat $tempfile2
			echo ""
		fi
	fi
	if [ [${caNumber} > 0]  ]; then
		if $caVerbose ; then
			cat $tempfile1
			echo ""
		fi
	fi

done

rm -f $netfile
rm -f $swfile
rm -f $swguids
rm -f $tempfile1
rm -f $tempfile2

}

function eth-tool-all-interfaces {
	for interface in ${NETDEVICES}; do
		echo -e "\nethtool $interface"
		ethtool $interface
		echo "____________"
		echo -e "\nethtool -i $interface"
		ethtool -i $interface
		echo "____________"
		echo -e "\nethtool -g $interface"
		ethtool -g $interface
		echo "____________"
		echo -e "\nethtool -a $interface"
		ethtool -a $interface
		echo "____________"
		echo -e "\nethtool -k $interface"
		ethtool -k $interface
		echo "____________"
		echo -e "\nethtool -c $interface"
		ethtool -c $interface
		echo "____________"
		echo "--------------------------------------------------"
	done
}

function lspci-xxxvvv {
   for interface in `lspci |grep Mellanox | awk '{print $1}'`
      do
         lspci -s $interface -xxxvvv
      done
}

function eth-tool-all-interfaces_S {
   for interface in ${NETDEVICES}
      do
      ethtool -S $interface > $OUT_DIR/stat/ethtool_S_$interface
   done
}

function show_irq_affinity_all {
   for interface in ${NETDEVICES}
      do
      echo -e "\nshow_irq_affinity.sh $interface"
      show_irq_affinity.sh $interface
      echo "--------------------------------------------------"
   done
}

function fw-ini-dump {

   for interface in `lspci |grep Mellanox | awk '{print $1}'`
   do
	mstflint -d $interface dc > $OUT_DIR/mstflint_$interface
   done

}

#============================COMMANDS PREPARE SECTION====================
function prepare_commands {

cat > $TMPDIR/sysinfo-temp.$$ <<'EODEOD'
date
uptime
hostname
uname -a
uname -r
cat /etc/*release*
show_irq_affinity_all
numactl --hardware
lscpu
find /sys/devices/system/node -name  cpulist -exec cat {} \;
lspci
lspci -tv
lspci-xxxvvv
ofed_info -s
ibstat
ibstatus
ibv_devinfo
ibv_devinfo -v
ibdev2netdev
ifconfig -a
route -n
service iptables status
service irqbalance status
service cpuspeed status
eth-tool-all-interfaces
eth-tool-all-interfaces_S
ip a s
ip m s
ip n s
arp -an
netstat -anp
netstat -nlp
netstat -nr
netstat -i
cat /proc/net/igmp
cat /proc/net/dev_mcast
cat /proc/sys/net/ipv4/igmp_max_memberships
cat /proc/sys/net/ipv4/igmp_max_msf
chkconfig --list | sort
service --status-all
initctl list
ofed_info
ompi_info
lsmod
dpkg --list
rpm -qva --qf '%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH} %{SIZE}\n' | sort
free
sysctl -a
ulimit -a
mount
df -lh
fdisk -l
blkid -c /dev/null | sort
zz_proc_net_bonding_files
zz_sys_class_net_files
ps xfalw
EODEOD

exec 10< $TMPDIR/sysinfo-temp.$$
i=0
while read -r commands[$i] <&10; do
    ((i++))
done
exec 10>&-
rm $TMPDIR/sysinfo-temp.$$
}


function prepare_FabricCommands ()
{

# to sort the command requires a temp file as there are spaces in the commands.
sort > $TMPDIR/sysinfo-FabricCommands-temp.$$ <<'EODEOD'
ibnetdiscover
ibnetdiscover -p
ibcheckerrors -nocolor
ib_diagnet
ib-find-bad-ports
ib-find-disabled-ports
ib-mc-info-show
ib-topology-viewer
ibhosts
ibswitches
sminfo
sm_version
sm-status
sm_master_is
ib_switches_FW_scan
Multicast_Information
EODEOD



exec 10< $TMPDIR/sysinfo-FabricCommands-temp.$$
i=0
while read -r FabricCommands[$i] <&10; do
    ((i++))
done
exec 10>&-
rm $TMPDIR/sysinfo-FabricCommands-temp.$$

}

function prepare_files {

files+="/proc/version /proc/modules /proc/cpuinfo /proc/mounts /proc/cmdline /proc/devices /proc/diskstats /proc/dma /proc/interrupts  /proc/meminfo /proc/partitions /proc/stat /proc/uptime /etc/resolv.conf /etc/hosts /etc/hosts.allow /etc/hosts.deny /sys/class/infiniband/*/board_id /sys/class/infiniband/*/fw_ver /sys/class/infiniband/*/hca_type /sys/class/infiniband/*/hw_rev /sys/class/infiniband/*/node_desc /sys/class/infiniband/*/node_guid /sys/class/infiniband/*/node_type /sys/class/infiniband/*/sys_image_guid /etc/issue /proc/net/dev_mcast /etc/modprobe.conf /etc/modprobe.d/* /boot/grub/grub.conf /boot/grub/grub.cfg /etc/default/grub /etc/network/interfaces "

files+=" $(find /etc/sysconfig/ -name 'ifcfg*')"

# As our filenames don't have spaces (assumption), we can used the following to sort them:
files=$(echo $files)
xfiles=( $(echo "${files// /
}" | sort -u ) )

}

function copy_files {
		ln -s /boot/config-$(uname -r)  $OUT_DIR/config-$(uname -r)
		ln -s /proc/config.gz $OUT_DIR/config.gz
		dmesg > $OUT_DIR/dmesg
		biosdecode  > $OUT_DIR/biosdecode
		dmidecode > $OUT_DIR/dmidecode
		ln -s /var/log/messages $OUT_DIR/messages
		ln -s /var/log/syslog $OUT_DIR/syslog
		ln -s /etc/libvma.conf $OUT_DIR/libvma.conf
}

# ------------------------------------------------------------------------
# generating the output HTML/text file.

function generate_html {
echo "<html><head><title>$OUT_FILE</title></head><body><pre>"
echo -n "<a name=\"index\"></a><h1>Mellanox Technologies</h1>"
echo -n "<a name=\"index\"></a><h2>Linux and OFED System Information Snapshot Utility</h2>"
echo -n "<a name=\"index\"></a><h2>Version: ${VERSION}</h2>"
echo -n "<hr>"

#=======================BEGIN OF SERVER COMMANDS SECTION ====================
echo -n '<h2>Server Commands:</h2>'
echo '<table cols="4" width="100%" border="0" bgcolor="#E0E0FF"><tr>'
rows=$(( ${#commands[@]} / 4 + ( ( ${#commands[@]} % 4 ) ? 1 : 0 ) ))
c=0
r=0
base=1000
echo "<!-- rows: $rows Server commands: ${#commands[@]} -->"

for ((i = 0; i < ${#commands[@]} ; i++)); do
  cmd=$(( r + c * rows ))
  sec=$(( base + cmd + 1 ))
  echo "<!-- sec $sec cmd $cmd -->"
  echo "<td width=\"25%\"><a href=\"#sec$sec\">${commands[$cmd]}</a></td>"
  (( c++ ))
  if [[ 0 -eq $(( c % 4 )) ]]; then
      echo '</tr><tr>'
      (( r++ ))
      c=0
  fi
done
echo -n '</tr></table>'
basesec=$(( ${#commands[@]} + 1 ))
#=======================END OF SERVER COMMANDS SECTION =======================

#=======================BEGIN OF FABRIC DIGNASTICS SECTION ===================
echo -n '<h2>Fabric Diagnostics Information:</h2>'
echo '<table cols="4" width="100%" border="0" bgcolor="#E0E0FF"><tr>'

rows=$(( ${#FabricCommands[@]} / 4 + ( ( ${#FabricCommands[@]} % 4 ) ? 1 : 0 ) ))
c=0
r=0
base=2000
echo "<!-- rows: $rows Fabric Diagnostics Information: ${#FabricCommands[@]} ${#FabricCommands[1]}-->"

for ((i = 0; i <= ${#FabricCommands[@]} ; i++)); do
  cmd=$(( r + c * rows ))
  sec=$(( base + cmd + 1 ))
  echo "<!-- sec $sec cmd $cmd -->"
  echo "<td width=\"25%\"><a href=\"#sec$sec\">${FabricCommands[$cmd]}</a></td>"
  (( c++ ))
  if [[ 0 -eq $(( c % 4 )) ]]; then
      echo '</tr><tr>'
      (( r++ ))
      c=0
  fi
done
echo -n '</tr></table>'
basesec=$(( ${#FabricCommands[@]} + 1 ))

#=======================END OF FABRIC DIGNASTICS SECTION =====================

#=======================BEGIN OF FILES SECTION ===============================
echo -n '<h2>Files:</h2>'
echo '<table cols="4" width="100%" border="0" bgcolor="#E0E0FF"><tr>'
n=1
rows=$(( ${#xfiles[@]} / 4 + ( ( ${#xfiles[@]} % 4 ) ? 1 : 0 ) ))
c=0;
r=0;
base=3000
echo "<!-- rows: $rows files: ${#xfiles[@]} -->"
for ((i = 0; i < ${#xfiles[@]}; i++)) ; do
  fno=$(( r + c * rows ))
  #sec=$(( base + fno + basesec ))
  sec=$(( base + fno + 1 ))
  echo "<td width=\"25%\"><a href=\"#sec$sec\">${xfiles[$fno]}</a></td>"
  ((c++))
  if [[ 0 -eq $(( c % 4 )) ]]; then
      echo '</tr><tr>'
      ((r++))
      c=0
  fi
done
echo '</tr></table>'
#=======================BEGIN OF FILES SECTION ===============================

echo '<a href="#systemfiles">Other system files</a>'

base=1000
sec=$(( base + 1 ))
for ((i = 0; i < ${#commands[@]} ; i++)); do
  echo -n "<a name=\"sec$sec\"></a>"
  echo -n "<small><a href=\"#sec$((sec - 1))\">[&lt;&lt;prev]</a></small> "
  echo -n "<small><a href=\"#index\">[back to index]</a></small> "
  echo "<small><a href=\"#sec$((sec + 1))\">[next>>]</a></small>"
  echo -n "<h2>${commands[$i]}</h2>"
  eval "${commands[$i]}" 2>&1 | sed 's/</\&lt;/g;s/>/\&gt;/g'
  ((sec++))
done
base=2000
sec=$(( base + 1 ))
for ((i = 0; i < ${#FabricCommands[@]} ; i++)); do
  echo -n "<a name=\"sec$sec\"></a>"
  echo -n "<small><a href=\"#sec$((sec - 1))\">[&lt;&lt;prev]</a></small> "
  echo -n "<small><a href=\"#index\">[back to index]</a></small> "
  echo "<small><a href=\"#sec$((sec + 1))\">[next>>]</a></small>"
  echo -n "<h2>${FabricCommands[$i]}</h2>"
  eval "${FabricCommands[$i]}" 2>&1 | sed 's/</\&lt;/g;s/>/\&gt;/g'
  ((sec++))
done

base=3000
sec=$(( base + 1))
for ((i = 0; i < ${#xfiles[@]}; i++)) ; do
  f="${xfiles[$i]}"
  echo -n "<a name=\"sec$sec\"></a>"
  echo -n "<small><a href=\"#sec$((sec - 1))\">[&lt;&lt;prev]</a></small> "
  echo -n "<small><a href=\"#index\">[back to index]</a></small> "
  echo "<small><a href=\"#sec$((sec + 1))\">[next>>]</a></small>"
  echo -n "<h2>$f</h2>"
  cat "$f" 2>&1 | sed 's/</\&lt;/g;s/>/\&gt;/g'
  ((sec++))
done

base=4000
sec=$(( base + 1))
echo -n "<small><a href=\"#sec$((sec - 1))\">[&lt;&lt;prev]</a></small> "
echo -n "<small><a href=\"#index\">[back to index]</a></small> "
echo "<small><a href=\"#sec$((sec + 1))\">[next>>]</a></small>"
echo -n "<h2>numa_nodes</h2>"
for f in $(find /sys | grep numa_node |grep -v uevent |sort ); do
  if [[ -f $f ]]; then
    echo -n "</h2>$f </h2>"
    cat "$f" 2>&1 | sed 's/</\&lt;/g;s/>/\&gt;/g'
    ((sec++))
  fi
done

base=5000
sec=$(( base + 1))
  echo -n '<a name="systemfiles"></a>'
  echo -n "<small><a href=\"#sec$((sec - 1))\">[&lt;&lt;prev]</a></small> "
  echo -n "<small><a href=\"#index\">[back to index]</a></small> "
  echo '<h2>System Files</h2>'
  for f in $(find /sys | grep infini |grep -v uevent |sort ) ${NETDEVICES}; do
      if [[ -f $f ]]; then
          echo "File: $f: $(cat $f | sed 's/</\&lt;/g;s/>/\&gt;/g')"
      fi
  done

  echo -n "<small><a href=\"#index\">[back to index]</a></small>"
  echo '<br></pre></body></html>'

}


prepare_commands
prepare_FabricCommands
prepare_files
copy_files
fw-ini-dump

if [[ -z "$HTTP_HOST" ]]; then
	( generate_html ) > $OUT_FILE 2>&1
	cd $TMPDIR
	tar czf $OUT_DIR.tgz `basename $OUT_DIR`
	rm -rf $OUT_DIR
	echo "Created $OUT_DIR.tgz"
else
	echo "Content-type: text/html"
	echo "Cache-Control: no-cache"
	echo ""
	generate_html
fi

exit 0
