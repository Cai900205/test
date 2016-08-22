#!/bin/bash
#
# Copyright (c) 2014 Mellanox Technologies. All rights reserved.
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
#
#


# config: /etc/mlxn-en.conf
CONFIG=${CONFIG:-"/etc/mlnx-en.conf"}
export LANG=en_US.UTF-8
PATH=$PATH:/sbin:/usr/bin:/lib/udev

if [ ! -f $CONFIG ]; then
    echo No mlnx-en configuration found
    exit 0
fi

. $CONFIG

RUN_SYSCTL=${RUN_SYSCTL:-"no"}
RUN_MLNX_TUNE=${RUN_MLNX_TUNE:-"no"}

UNLOAD_MODULES="memtrack mlx4_fc mlx4_en mlx4_ib mlx4_core compat"
STATUS_MODULES="mlx4_en mlx4_core"

# Only use ONBOOT option if called by a runlevel directory.
# Therefore determine the base, follow a runlevel link name ...
base=${0##*/}
link=${base#*[SK][0-9][0-9]}
# ... and compare them
if [[ $link == $base && "$0" != "/etc/rc.d/init.d/openibd" ]] ; then
    RUNMODE=manual
else
    RUNMODE=auto
fi

# Allow unsupported modules, if disallowed by current configuration
modprobe=/sbin/modprobe
if ${modprobe} -c | grep -q '^allow_unsupported_modules  *0'; then
    modprobe="${modprobe} --allow-unsupported-modules"
fi

ACTION=$1
shift

#########################################################################
# Get a sane screen width
[ -z "${COLUMNS:-}" ] && COLUMNS=80

[ -z "${CONSOLETYPE:-}" ] && [ -x /sbin/consoletype ] && CONSOLETYPE="`/sbin/consoletype`"

# Read in our configuration
if [ -z "${BOOTUP:-}" ]; then
    if [ -f /etc/sysconfig/init ]; then
        . /etc/sysconfig/init
    else
        # This all seem confusing? Look in /etc/sysconfig/init,
        # or in /usr/doc/initscripts-*/sysconfig.txt
        BOOTUP=color
        RES_COL=60
        MOVE_TO_COL="echo -en \\033[${RES_COL}G"
        SETCOLOR_SUCCESS="echo -en \\033[1;32m"
        SETCOLOR_FAILURE="echo -en \\033[1;31m"
        SETCOLOR_WARNING="echo -en \\033[1;33m"
        SETCOLOR_NORMAL="echo -en \\033[0;39m"
        LOGLEVEL=1
    fi
    if [ "$CONSOLETYPE" = "serial" ]; then
        BOOTUP=serial
        MOVE_TO_COL=
        SETCOLOR_SUCCESS=
        SETCOLOR_FAILURE=
        SETCOLOR_WARNING=
        SETCOLOR_NORMAL=
    fi
fi

if [ "${BOOTUP:-}" != "verbose" ]; then
    INITLOG_ARGS="-q"
else
    INITLOG_ARGS=
fi

echo_success() {
    echo -n $@
    [ "$BOOTUP" = "color" ] && $MOVE_TO_COL
    echo -n "[  "
    [ "$BOOTUP" = "color" ] && $SETCOLOR_SUCCESS
    echo -n $"OK"
    [ "$BOOTUP" = "color" ] && $SETCOLOR_NORMAL
    echo -n "  ]"
    echo -e "\r"
    return 0
}

echo_done() {
    echo -n $@
    [ "$BOOTUP" = "color" ] && $MOVE_TO_COL
    echo -n "[  "
    [ "$BOOTUP" = "color" ] && $SETCOLOR_NORMAL
    echo -n $"done"
    [ "$BOOTUP" = "color" ] && $SETCOLOR_NORMAL
    echo -n "  ]"
    echo -e "\r"
    return 0
}

echo_failure() {
    echo -n $@
    [ "$BOOTUP" = "color" ] && $MOVE_TO_COL
    echo -n "["
    [ "$BOOTUP" = "color" ] && $SETCOLOR_FAILURE
    echo -n $"FAILED"
    [ "$BOOTUP" = "color" ] && $SETCOLOR_NORMAL
    echo -n "]"
    echo -e "\r"
    return 1
}

echo_warning() {
    echo -n $@
    [ "$BOOTUP" = "color" ] && $MOVE_TO_COL
    echo -n "["
    [ "$BOOTUP" = "color" ] && $SETCOLOR_WARNING
    echo -n $"WARNING"
    [ "$BOOTUP" = "color" ] && $SETCOLOR_NORMAL
    echo -n "]"
    echo -e "\r"
    return 1
}

# If module $1 is loaded return - 0 else - 1
is_module()
{
    local RC

    /sbin/lsmod | grep -w "$1" > /dev/null 2>&1
    RC=$?

    return $RC
}

log_msg()
{
    logger -i "mlnx-en.d: $@"
}

load_module()
{
    local module=$1
    filename=`modinfo $module 2>/dev/null | grep filename | awk '{print $NF}'`

    if [ ! -n "$filename" ]; then
        echo_failure "Module $module does not exist!"
        log_msg "ERROR: Module $module does not exist!"
        return 0
    fi

    ${modprobe} $module > /dev/null 2>&1
}

get_sw_fw_info()
{
    INFO=/etc/infiniband/info
    OFEDHOME="/usr/local"
    if [ -x ${INFO} ]; then
        OFEDHOME=$(${INFO} | grep -w prefix | cut -d '=' -f 2)
    fi
    MREAD=$(which mstmread 2> /dev/null)

    # Get OFED Build id
    if [ -r ${OFEDHOME}/BUILD_ID ]; then
        echo  "Software"
        echo  "-------------------------------------"
        printf "Build ID:\n"
        cat ${OFEDHOME}/BUILD_ID
        echo  "-------------------------------------"
    fi

    # Get FW version
    if [ ! -x ${MREAD} ]; then
        return 1
    fi

    vendor="15b3"
    slots=$(lspci -n -d "${vendor}:" 2> /dev/null | grep -v "5a46" | cut -d ' ' -f 1)
    for mst_device in $slots
    do
        major=$($MREAD ${mst_device} 0x82478 2> /dev/null | cut -d ':' -f 2)
        subminor__minor=$($MREAD ${mst_device} 0x8247c 2> /dev/null | cut -d ':' -f 2)
        ftime=$($MREAD ${mst_device} 0x82480 2> /dev/null | cut -d ':' -f 2)
        fdate=$($MREAD ${mst_device} 0x82484 2> /dev/null | cut -d ':' -f 2)

        major=$(echo -n $major | cut -d x -f 2 | cut -b 4)
        subminor__minor1=$(echo -n $subminor__minor | cut -d x -f 2 | cut -b 3,4)
        subminor__minor2=$(echo -n $subminor__minor | cut -d x -f 2 | cut -b 5,6,7,8)
        echo
        echo "Device ${mst_device} Info:"
        echo "Firmware:"

        printf "\tVersion:"
        printf "\t$major.$subminor__minor1.$subminor__minor2\n"

        day=$(echo -n $fdate | cut -d x -f 2 | cut -b 7,8)
        month=$(echo -n $fdate | cut -d x -f 2 | cut -b 5,6)
        year=$(echo -n $fdate | cut -d x -f 2 | cut -b 1,2,3,4)
        hour=$(echo -n $ftime | cut -d x -f 2 | cut -b 5,6)
        min=$(echo -n $ftime | cut -d x -f 2 | cut -b 3,4)
        sec=$(echo -n $ftime | cut -d x -f 2 | cut -b 1,2)

        printf "\tDate:"
        printf "\t$day/$month/$year $hour:$min:$sec\n"
    done
}

# Create debug info
get_debug_info()
{
    trap '' 2 9 15
    if [ -x /usr/sbin/sysinfo-snapshot.sh ]; then
        echo
        echo "Please run /usr/sbin/sysinfo-snapshot.sh to collect the debug information"
        echo "and open an issue in the http://support.mellanox.com/SupportWeb/service_center/SelfService"
        echo
    else
        DEBUG_INFO=/tmp/ib_debug_info.log
        /bin/rm -f $DEBUG_INFO
        touch $DEBUG_INFO
        echo "Hostname: `hostname -s`" >> $DEBUG_INFO
        test -e /etc/issue && echo "OS: `cat /etc/issue`" >> $DEBUG_INFO
        echo "Current kernel: `uname -r`" >> $DEBUG_INFO
        echo "Architecture: `uname -m`" >> $DEBUG_INFO
        which gcc &>/dev/null && echo "GCC version: `gcc --version`"  >> $DEBUG_INFO
        echo "CPU: `cat /proc/cpuinfo | /bin/grep -E \"model name|arch\" | head -1`" >> $DEBUG_INFO
        echo "`cat /proc/meminfo | /bin/grep \"MemTotal\"`" >> $DEBUG_INFO
        echo "Chipset: `/sbin/lspci 2> /dev/null | head -1 | cut -d ':' -f 2-`" >> $DEBUG_INFO

        echo >> $DEBUG_INFO
        get_sw_fw_info >> $DEBUG_INFO
        echo >> $DEBUG_INFO

        echo >> $DEBUG_INFO
        echo "############# LSPCI ##############" >> $DEBUG_INFO
        /sbin/lspci 2> /dev/null >> $DEBUG_INFO

        echo >> $DEBUG_INFO
        echo "############# LSPCI -N ##############" >> $DEBUG_INFO
        /sbin/lspci -n 2> /dev/null >> $DEBUG_INFO

        echo >> $DEBUG_INFO
        echo "############# LSMOD ##############" >> $DEBUG_INFO
        /sbin/lsmod >> $DEBUG_INFO

        echo >> $DEBUG_INFO
        echo "############# DMESG ##############" >> $DEBUG_INFO
        /bin/dmesg >> $DEBUG_INFO

        if [ -r /var/log/messages ]; then
            echo >> $DEBUG_INFO
            echo "############# Messages ##############" >> $DEBUG_INFO
            tail -50 /var/log/messages >> $DEBUG_INFO
        fi

        echo >> $DEBUG_INFO
        echo "############# Running Processes ##############" >> $DEBUG_INFO
        /bin/ps -ef >> $DEBUG_INFO
        echo "##############################################" >> $DEBUG_INFO

        echo
        echo "Please open an issue in the http://support.mellanox.com/SupportWeb/service_center/SelfService and attach $DEBUG_INFO"
        echo
    fi
}

get_mlx4_en_interfaces()
{
    mlx4_en_interfaces=""
    for ethpath in /sys/class/net/*
    do
        if (grep 0x15b3 ${ethpath}/device/vendor > /dev/null 2>&1); then
            mlx4_en_interfaces="$mlx4_en_interfaces ${ethpath##*/}"
        fi
    done
}

start()
{
    local RC=0

    # W/A: inbox drivers are loaded at boot instead of new ones
    local mlxko=$(/sbin/lsmod 2>/dev/null | grep '^mlx' | head -1 | awk '{print $NR}')
    if [ "X$mlxko" != "X" ]; then
        local loaded_srcver=$(/bin/cat /sys/module/$mlxko/srcversion 2>/dev/null)
        local curr_srcver=$(/sbin/modinfo $mlxko 2>/dev/null | grep srcversion | awk '{print $NF}')
        if [ "X$loaded_srcver" != "X$curr_srcver" ]; then
            log_msg "start(): Detected loaded old version of module '$mlxko', calling stop..."
            stop
        fi
    fi

    # W/A: modules loaded from initrd without taking new params from /etc/modprobe.d/
    local conf_files=$(grep -rE "options.*mlx" /etc/modprobe.d/*.conf 2>/dev/null | grep -v ":#" | cut -d":" -f"1" | uniq)
    local goFlag=1
    if [ "X$conf_files" != "X" ]; then
        for file in $conf_files
        do
            while read line && [ $goFlag -eq 1 ]
            do
                local curr_mod=$(echo $line | sed -r -e 's/.*options //g' | awk '{print $NR}')
                if ! is_module $curr_mod; then
                    continue
                fi
                for item in $(echo $line | sed -r -e "s/.*options\s*${curr_mod}//g")
                do
                    local param=${item%=*}
                    local conf_value=${item##*=}
                    local real_value=$(cat /sys/module/${curr_mod}/parameters/${param} 2>/dev/null)
                    if [ "X$conf_value" != "X$real_value" ]; then
                        log_msg "start(): Detected '$curr_mod' loaded with '$param=$real_value' instead of '$param=$conf_value' as configured in '$file', calling stop..."
                        goFlag=0
                        stop
                        break
                    fi
                done
            done < $file
            if [ $goFlag -ne 1 ]; then
                break
            fi
        done
    fi

    load_module mlx4_core
    my_rc=$?
    if [ $my_rc -ne 0 ]; then
        echo_failure $"Loading Mellanox MLX4 NIC driver: "
    fi

    load_module mlx4_en
    my_rc=$?
    if [ $my_rc -ne 0 ]; then
        echo_failure $"Loading Mellanox MLX4_EN NIC driver: "
    fi
    RC=$[ $RC + $my_rc ]

    if [ $RC -eq 0 ]; then
        echo_success $"Loading NIC driver: "
    else
        echo_failure $"Loading NIC driver: "
        get_debug_info
        exit 1
    fi

    if [ $my_rc -eq 0 ]; then
        # Bring up network interfaces
        sleep 1
        get_mlx4_en_interfaces
        for en_i in $mlx4_en_interfaces
        do
            /sbin/ifup $en_i 2> /dev/null
        done
        /sbin/ifup -a >/dev/null 2>&1
    fi

    if  [ -x /sbin/sysctl_perf_tuning ] && [ "X${RUN_SYSCTL}" == "Xyes" ]; then
        /sbin/sysctl_perf_tuning load
    fi

    if [ -x /usr/sbin/mlnx_tune ] && [ "X${RUN_MLNX_TUNE}" == "Xyes" ];then
        /usr/sbin/mlnx_tune > /dev/null 2>&1
    fi

    return $RC
}

UNLOAD_REC_TIMEOUT=100
unload_rec()
{
    local mod=$1
    shift

    if is_module $mod ; then
    ${modprobe} -r $mod >/dev/null 2>&1
    if [ $? -ne 0 ];then
        for dep in `/sbin/rmmod $mod 2>&1 | grep "is in use by" | sed -r -e 's/.*use by //g' | sed -e 's/,/ /g'`
            do
                # if $dep was not loaded by openibd, don't unload it; fail with error.
                if ! `echo $UNLOAD_MODULES | grep -q $dep` ; then
                        rm_mod $mod
                else
                        unload_rec $dep
                fi
            done
        fi
        if is_module $mod ; then
            if [ "X$RUNMODE" == "Xauto" ] && [ "X$mod" == "Xmlx4_core" ] && [ $UNLOAD_REC_TIMEOUT -gt 0 ]; then
                let UNLOAD_REC_TIMEOUT--
                sleep 1
                unload_rec $mod
            else
                rm_mod $mod
            fi
        fi
    fi
}

rm_mod()
{
    local mod=$1
    shift

    unload_log=`/sbin/rmmod $mod 2>&1`
    if [ $? -ne 0 ]; then
        echo_failure $"Unloading $mod"
        if [ ! -z "${unload_log}" ]; then
            echo $unload_log
        fi
        # get_debug_info
        [ ! -z $2 ] && echo $2
        exit 1
    fi
}

unload()
{
    # Unload module $1
    local mod=$1

    if is_module $mod; then
        unload_rec $mod
    fi
}

stop()
{
# Unload modules
    if [ "$UNLOAD_MODULES" != "" ]; then
        for mod in  $UNLOAD_MODULES
        do
            unload $mod
        done
    fi

    if  [ -x /sbin/sysctl_perf_tuning ] && [ "X${RUN_SYSCTL}" == "Xyes" ]; then
        /sbin/sysctl_perf_tuning unload
    fi

    echo_success $"Unloading NIC driver: "
    sleep 1
}

status()
{
    local RC=0

    if is_module mlx4_core && is_module mlx4_en; then
        echo
        echo "  NIC driver loaded"
        echo
    else
        echo
        echo $"NIC driver is not loaded"
        echo
    fi

    if is_module mlx4_en; then
        get_mlx4_en_interfaces
        if [ -n "$mlx4_en_interfaces" ]; then
            echo $"Configured MLX4_EN devices:"
            echo $mlx4_en_interfaces
            echo
            echo $"Currently active MLX4_EN devices:"

            for i in $mlx4_en_interfaces
            do
                 echo `/sbin/ip -o link show $i | awk -F ": " '/UP>/ { print $2 }'`
            done
        fi
    fi
    echo

    local cnt=0

    for mod in  $STATUS_MODULES
    do
        if is_module $mod; then
            [ $cnt -eq 0 ] && echo "The following mlnx-en modules are loaded:" && echo
            let cnt++
            echo "  $mod"
        fi
    done
    echo

    return $RC
}

RC=0
start_time=$(date +%s | tr -d '[:space:]')

trap_handler()
{
    let run_time=$(date +%s | tr -d '[:space:]')-${start_time}

    # Ask to wait for 5 seconds if trying to stop mlnx-en
    if [ $run_time -gt 5 ] && [ "$ACTION" == "stop" ]; then
        printf "\nProbably some application are still using mlnx-en modules...\n"
    else
        printf "\nPlease wait ...\n"
    fi
    return 0
}

trap 'trap_handler' 2 9 15

case $ACTION in
    start)
        start
        RC=$?
        ;;
    stop)
        stop
        RC=$?
        ;;
    restart)
        stop
        RC=$?
        start
        RC=$(($RC + $?))
        ;;
    status)
        status
        RC=$?
        ;;
    *)
        echo
        echo "Usage: `basename $0` {start|stop|restart|status}"
        echo
        exit 1
        ;;
    esac

exit $RC