#! /bin/sh
console_save=test.log
while read newline
do
    #echo $newline
    IFS=' ' 
    array=($newline)
    counter=1
    for argu in ${array[@]};
    do
        if [ $counter -eq 2 ];
        then
            exe=`echo $argu | awk -F '=' '{print $1}' | sed 's/_do//g'`
            do_or_not=`echo $argu | awk -F '=' '{print $2}'`
        elif [ $counter -gt 2 ];
        then
            parameter[ $counter - 3 ]=`echo $argu | awk -F '=' '{print $2}'`
        else
            module_name=`echo $argu | awk -F ']' '{print $1}' | sed 's/\[//g'`
        fi
        counter=$[ $counter + 1 ]
    done

    if [ $do_or_not = "do" ];
    then
        #echo $exe,$do_or_not,"counter:"$counter
        command="./script/$exe.sh"
        for para in ${parameter[@]}
        do
            command=$command" "$para
        done
        test_module="[Test-Module]: $module_name"
        add_space=$[50-${#test_module}]
        space=" "
        while [ $add_space -ne 0 ]
        do
            test_module="$test_module""$space"
            add_space=$[ $add_space - 1 ]
        done
        IFS=
        echo -n $test_module | tee -a $console_save >&2
        IFS=' '
        #echo $command
     	$command  >> $console_save
        result=$?
        if [ -f "./script/${exe}.sh" ]
        then
            if [ $result -eq 0  ]
            then
                echo -e '\033[0;32;1mPassed\033[0m' | tee -a $console_save >&2
            else
                echo -e '\033[0;31;1mFailed\033[0m' | tee -a $console_save >&2
            fi
        else
            if [ $result -eq 0  ]
            then
                echo -e '\033[0;32;1mPassed\033[0m' | tee -a $console_save >&2
            else
                echo -e '\033[0;31;1mFailed\033[0m' | tee -a $console_save >&2
            fi
        fi
    fi
    unset parameter
    unset array
done < $1
