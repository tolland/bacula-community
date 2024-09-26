#!/usr/bin/env bash
#
# Copyright (C) 2015-2020 Radosław Korzeniewski
# License: BSD 2-Clause; see file LICENSE-FOSS
#
# This is an Inteos regression tests support utilities.
# Author: Radoslaw Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
#

#
# the support procedures requires a following variables for proper usage:
#   $JobName - a Jobname prefix to execute
#   $tmp - directory for temporary files - set by scripts/functions
#   $CLIENT - test backup client name - set by scripts/functions
#   $JOBID - a sequential number corresponding to executed job
#   $FileSetName - a FileSet prefix for restore job to execute
#

 # now check if we are running under a proper shell
if test "$BASH" != "/bin/bash"
then
   echo "Regression script must use BASH for this utilities!"
   exit 1
fi

#
# setup LPLUG variable used in listing tests
#
setup_plugin_param()
{
LPLUG=$1
if [ "x$debug" != "x" ]
then
   LPLUG="$LPLUG debug=$debug"
fi
export LPLUG
}

#
# Create self-signed certificate.
#
# in:
# $1 (optional): Directory to save the csr, key and pem.
# $2 (optional): Name of csr, key and pem
#
setup_self_signed_cert()
{
   cert_dir="/tmp/"
   cert_name="snakeoil"
   if [ "x$1" != "x" ]
   then
      if [ "${1:(-1)}" != "/" ]
      then
         cert_dir=${1}/
      fi
   fi
   if [ "x${2}" != "x" ]
   then
      cert_name=${2}
   fi
   echo "[INIT] CREATE OPENSSL CERTS"
   `openssl req -nodes -newkey rsa:2048 -keyout ${cert_dir}${cert_name}.key -out ${cert_dir}${cert_name}.csr -subj "/C=CH/ST=Vaud/L=Yverdon-les-Bains/O=Backup Security/OU=R&D Department/CN=baculasystems.com"`
   `openssl x509 -req -sha256 -days 365 -in ${cert_dir}${cert_name}.csr -signkey ${cert_dir}${cert_name}.key -out ${cert_dir}${cert_name}.pem`
   echo "[END] CREATE OPENSSL CERTS"
}

#
# common check if test pass by checking input param
#
# in:
# $1 - if variable is equal to zero ("0") then test pass else fail
#
regress_test_result()
{
   if [ $1 -ne 0 ]
   then
      echo "failed"
   else
      echo "ok"
   fi
}

#
# simplify unittest execution
#
# in:
# $1 - test name to execute - the unittest binary name
# $2 - test binary directory location
#
do_regress_unittest()
{
   . scripts/functions
   tname=$1
   tdirloc=$2
   make -C ${src}/${tdirloc} ${tname}
   if test $? -eq 0; then
      ${src}/${tdirloc}/${tname}
   fi
   exit $?
}

#
# do simple backup job test
#   generate ${tmp}/blog${ltest}.out job output messages logfile
#
# in:
#       $1 - a test number to perform which means it execute a job=${JobName}${1}
#
do_regress_backup_test()
{
ltest=$1
blevel="full"
if [ "x$2" != "x" ]
then
   blevel=$2
fi
printf "     backup test${ltest} ... "
cat << END_OF_DATA >${tmp}/bconcmds
@output /dev/null
messages
@$out ${tmp}/blog${ltest}.out
status client=${CLIENT}
setdebug level=500 client=${CLIENT} trace=1
run job=${JobName}${ltest} level=${blevel} storage=File1 yes
wait
status client=${CLIENT}
messages
setdebug level=0 trace=0 client=${CLIENT}
llist jobid=${JOBID}
list files jobid=${JOBID}
quit
END_OF_DATA
run_bconsole
((JOBID++))
}

#
# do parallel backup job test
#   generate ${tmp}/blog${ltest}.out job output messages logfile
#
# in:
#       $1 - a test number to perform
#       $@ - a sequence of job name. 
#
# for example:
#   do_regress_parallel_backup_test 10 23 43
#
# Explication:
#   We run the test number 10.
#   We run the jobs ${JobName}-23 and ${jobName}-43
#
do_regress_parallel_backup_test()
{
test_id=$1
shift
job_ids=""
blevel="full"

printf "     backup test${test_id} ... "
cat << END_OF_DATA >${tmp}/bconcmds
@output /dev/null
messages
@$out ${tmp}/blog${test_id}.out
status client=${CLIENT}
setdebug level=500 client=${CLIENT} trace=1
END_OF_DATA
counter=1
for job_id in "$@"; do
   
   job_ids="${job_ids} jobid=${JOBID}"
   echo "run job=${JobName}${job_id} level=${blevel} storage=File1 yes" >>${tmp}/bconcmds
   ((JOBID++))

done

cat << END_OF_DATA >>${tmp}/bconcmds
wait
status client=${CLIENT}
messages
setdebug level=0 trace=0 client=${CLIENT}
llist ${job_ids}
list files ${job_ids}
quit
END_OF_DATA
run_bconsole
}

#
# do simpe estimation listing test
#   generate ${tmp}/elog${ltest}.out job output messages logfile
#
# in:
# $1 - a test number to perform which means it execute a job=${JobName}${1}
#
do_regress_estimate_test()
{
ltest=$1
printf "     estimate test${ltest} ... "
cat << END_OF_DATA > ${tmp}/bconcmds
#@output /dev/null
messages
@$out ${tmp}/elog${ltest}.out
setdebug level=150 client=${CLIENT} trace=1
estimate listing job=$JobName fileset=${FileSetName}${ltest} level=Full
messages
setdebug level=50 trace=0 client=${CLIENT}
quit
END_OF_DATA
run_bconsole
}

#
# do plugin listing test
#   generate ${tmp}/llog${ltest}.out output listing logfile
#
# in:
# $1 - a test number to perform
# $2 - a path=... parameter for listing command
# $3 - custom $LPLUG plugin reference - optional, uses exported LPLUG variable
#
do_regress_listing_test()
{
ltest=$1
lpath=$2
lplug=$LPLUG
if [ "x$3" != "x" ]
then
   lplug=$3
fi
printf "     listing test${ltest} ... "
cat << END_OF_DATA >${tmp}/bconcmds
#@output /dev/null
messages
@$out ${tmp}/llog${ltest}.out
setdebug level=150 client=${CLIENT} tracee=1
.ls client=${CLIENT} plugin="$lplug" path=${lpath}
setdebug level=50 trace=0 client=${CLIENT}
quit
END_OF_DATA
run_bconsole
}

#
# do standard restore test
#   generate ${tmp}/rlog${ltest}.out job output messages logfile
#
# in:
# $1 - a test number to perform
# $2 - a FileSet number to use for restore test as ${FileSetName}${fs}
# $3 - a where=... restore parameter
# $4 - a restore selection command, optional. you can use strings like:
#     "file=..." or "select all"
#
do_regress_restore_test()
{
ltest=$1
fs=$2
where=$3
file=$4
cmd="restore fileset=${FileSetName}${fs} where=$3"
if [ "x$file" != "x" ]
then
   cmd="${cmd} $file"
fi
cmd="${cmd} storage=File1 done"
printf "     restore test${ltest} ... "
cat << END_OF_DATA >${tmp}/bconcmds
messages
@$out ${tmp}/rlog${ltest}.out
setdebug level=500 client=${CLIENT} trace=1
${cmd}
yes
wait
setdebug level=0 client=${CLIENT} trace=0
messages
llist jobid=${JOBID}
quit
END_OF_DATA
run_bconsole
((JOBID++))
}

#
# check the expected backup job execution status based on logfile
#   check for status Successful
#
# in:
# $1 - a test number to examine which means we will check blog${ltest}.out logfile
#
check_regress_backup_statusT()
{
ltest=$1
RET=`grep "jobstatus: " ${tmp}/blog${ltest}.out | awk '{print $2}'`
ERRS=$((`grep "joberrors: " ${tmp}/blog${ltest}.out | awk '{print $2}'`+0))
if [ "x$RET" != "xT" -o $ERRS -ne 0 ]
then
   ((bstat++))
   return 1
else
   return 0
fi
}

#
# check the expected backup job execution status based on logfile
#   check for status Successful
#
# in:
# $1 - a test number to examine which means we will check blog${ltest}.out logfile
# $2 - number of jobs to examine
#
check_regress_parallel_backup_statusT()
{
   ltest=$1
   n_jobs=$2
   RET=`grep "jobstatus: " ${tmp}/blog${ltest}.out | sed 's/^[[:space:]]*//g' | awk '{print $2}' | grep "T" | wc -l`
   ERRS=$((`grep "joberrors: " ${tmp}/blog${ltest}.out | awk '{print $2}' | awk '{sum+=$1} END {print sum}'`+0))
   if [ "$RET" != "$n_jobs" -o $ERRS -ne 0 ]
   then
      ((bstat++))
      return 1
   else
      return 0
   fi
}

#
# check the expected backup job execution status based on logfile
#   check for status Warning
#
# in:
# $1 - a test number to examine which means we will check blog${ltest}.out logfile
#
check_regress_backup_statusW()
{
ltest=$1
RET=`grep "jobstatus: " ${tmp}/blog${ltest}.out | awk '{print $2}'`
ERRS=$((`grep "joberrors: " ${tmp}/blog${ltest}.out | awk '{print $2}'`+0))
if [ "x$RET" != "xT" -o $ERRS -eq 0 ]
then
   ((bstat++))
   return 1
else
   return 0
fi
}

#
# check the expected backup job execution status based on logfile
#   check for status Error or Fatal error
#
# in:
# $1 - a test number to examine which means we will check blog${ltest}.out logfile
#
check_regress_backup_statusE()
{
ltest=$1
RET=`grep "jobstatus: " ${tmp}/blog${ltest}.out | awk '{print $2}'`
if [ "x$RET" != "xf" -a "x$RET" != "xE" ]
then
   ((bstat++))
   return 1
else
   return 0
fi
}

#
# check the expected restore job execution status based on logfile
#   check for status Successful
#
# in:
# $1 - a test number to examine which means we will check rlog${ltest}.out logfile
#
check_regress_restore_statusT()
{
ltest=$1
RET=`grep "jobstatus: " ${tmp}/rlog${ltest}.out | awk '{print $2}'`
ERRS=$((`grep "joberrors: " ${tmp}/rlog${ltest}.out | awk '{print $2}'`+0))
if [ "x$RET" != "xT" -o $ERRS -ne 0 ]
then
   ((rstat++))
   return 1
else
   return 0
fi
}

#
# Check the expected string in any log file
#
# in:
# $1 - type of log. Values allowed: b,r,e,l
# $2 - the string to search into logfile
# $3 - a test number to examine which means we will check log${ltest}.out logfile
check_regress_string_in_log()
{
   type_log=$1
   to_search=$2
   n_test=$3
   log_file=${tmp}/${type_log}log${n_test}.out
   grep "${to_search}" ${log_file} > /dev/null
   F=$?
   if [ $F -ne 0 ]
   then
      error_stat="${type_log}stat"
      declare -g "${error_stat}"=$((${!error_stat}+1))
      return 1
   else
      return 0
   fi
}

#
# Check expected string is not in any log file
#
# in:
# $1 - type of log. Values allowed: b,r,e,l
# $2 - the string to search into logfile
# $3 - a test number to examine which means we will check log${ltest}.out logfile
check_regress_string_not_in_log()
{
   type_log=$1
   to_search=$2
   n_test=$3
   log_file=${tmp}/${type_log}log${n_test}.out
   grep "${to_search}" ${log_file} > /dev/null
   F=$?
   if [ $F -ne 1 ]
   then
      error_stat="${type_log}stat"
      declare -g "${error_stat}"=$((${!error_stat}+1))
      return 1
   else
      return 0
   fi
}

#
# Get a value of parameter
#
# in:
# $1 - type of log. Values allowed: b,r,e,l
# $2 - the parameter to get value. For example: 'FD Bytes Written', 'SD Bytes Written', 'jobbytes'
# $3 - a test number to examine which means we will check log${ltest}.out logfile
#
# Example of call: To get value of parameter "SD Bytes Written" in test 5
# get_value_of_parameter_in_log "b" "SD Bytes Written" 5
get_value_of_parameter_in_log()
{
   type_log=$1
   job_parameter=$2
   n_test=$3
   # echo -e "get_value_of_parameter_in_log:381 - Input: ${type_log} ${job_parameter} ${n_test}\n"
   log_file=${tmp}/${type_log}log${n_test}.out
   # Catch up the line with job_parameter, remove spaces and ',' and get value after ':'.
   parameter_value=$(grep "${job_parameter}:" ${log_file} | sed 's/ //g' | sed 's/,//g' | awk -F'[: (]+' '{print $2}')
   echo $parameter_value
}
#
#
# Check a value of parameter if greater, equal or less than input value
#
# in:
# $1 - type of log. Values allowed: b,r,e,l
# $2 - if operator without '-'. For example: 'gt' to greater than, 'ge' to greater than or equal, 'lt' to less than
# $3 - the parameter to check. For example: 'FD Bytes Written', 'SD Bytes Written', 'jobbytes'
# $4 - number to check. Can use *KB, *MB format. Example: 5, 50KB, 100MB 
# $5 - a test number to examine which means we will check log${ltest}.out logfile
#
# Example of call: To check if a backup job wrote more than 100MB in test 5
# check_regress_number_in_log "b" "gt" "SD Bytes Written" "100MB" 5
check_regress_number_in_log()
{
   type_log=$1
   operator="-"$2
   job_parameter=$3
   number=$4
   n_test=$5
   # printf "regress-utils.sh:420 - Input: %s;%s;%s;%s;%s\n" "${type_log}" "$operator" "${job_parameter}" "${number}" "${n_test}"
   
   parameter_value=$(get_value_of_parameter_in_log "${type_log}" "${job_parameter}" "${n_test}")

   if [[ $number == *KB ]]; then
      number=$(echo $number | tr -d 'KB' | awk '{print $1 * 1000}')
   elif [[ $number == *MB ]]; then
      number=$(echo $number | tr -d 'MB' | awk '{print $1 * 1000 * 1000}')
   elif [[ $number == *GB ]]; then
      number=$(echo $number | tr -d 'GB' | awk '{print $1 * 1000 * 1000 * 1000}')
   fi
   # printf "regress-utils.sh:420 - %s;%s;%s\n" "${parameter_value}" "$operator" "${number}"
   if [ "${parameter_value}" $operator "${number}"  ]
   then
      return 0
   else
      error_stat="${type_log}stat"
      declare -g "${error_stat}"=$((${!error_stat}+1))
      return 1
   fi
}

#
# This is a simple common function which start a fresh, new local slapd
# available on ldap://localhost:3890
#
# On ubuntu
# sudo apparmor_parser -R /etc/apparmor.d/usr.sbin.slapd
#
start_local_slapd()
{
if [ "x${SLAPD_DAEMON}" == "x" ]
then
   S1=`which slapd | wc -l`
   if [ $S1 -eq 0 ]
   then
      echo "slapd not found! required!"
      exit 1
   fi
   SLAPD_DAEMON="slapd"
fi

rm -rf ${tmp}/ldap
mkdir ${tmp}/ldap

db_name="database$$"
echo ${db_name} > ${tmp}/ldap_db_name

ldaphome=/etc/openldap
if [ -d /etc/ldap ]
then
    ldaphome=/etc/ldap
fi

cat << END_OF_DATA > ${tmp}/ldap/slapd.conf
include        ${ldaphome}/schema/core.schema
pidfile         ${tmp}/slapd.pid
argsfile        ${tmp}/slapd.args

moduleload back_bdb.la
database bdb
suffix "dc=${db_name},dc=bacula,dc=com"
directory ${tmp}/ldap
rootdn "cn=root,dc=${db_name},dc=bacula,dc=com"
rootpw rootroot

index cn,sn,uid pres,eq,approx,sub
index objectClass eq

END_OF_DATA

printf "Starting local slapd ... "
${SLAPD_DAEMON} -f ${tmp}/ldap/slapd.conf -h ldap://localhost:3890 -d0 &
SLAPD=$!
trap "kill $SLAPD" EXIT
sleep 5

cat << END_OF_DATA > ${tmp}/entries.ldif
dn: dc=$db_name,dc=bacula,dc=com
objectClass: dcObject
objectClass: organization
dc: $db_name
o: Example Corporation
description: The Example Corporation $db_name

# Organizational Role for Directory Manager
dn: cn=root,dc=$db_name,dc=bacula,dc=com
objectClass: organizationalRole
cn: root
description: Directory Manager
# The regress special object
dn: cn=other/name,dc=$db_name,dc=bacula,dc=com
objectClass: top
objectClass: device
END_OF_DATA

ldapadd -f $tmp/entries.ldif -x -D "cn=root,dc=$db_name,dc=bacula,dc=com" -w rootroot -H ldap://localhost:3890 2>&1 > ${tmp}/ldap.add.log

if [ $? -ne 0 ]; then
    print_debug "ERROR: Need to setup ldap access correctly"
    kill -INT `cat $tmp/slapd.pid`
    exit 1;
fi

echo "done"
}

#
# simply stops a background slapd daemon
#
stop_local_slapd()
{
   trap - EXIT
   kill -INT `cat ${tmp}/slapd.pid`
   sleep 5
}
